#include "nn_bridge.h"
#include "state_encoder.h"
#include "onnxruntime_c_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* DirectML provider 函数声明 */
ORT_API_STATUS(OrtSessionOptionsAppendExecutionProvider_DML, _In_ OrtSessionOptions* options, int device_id);

static const OrtApi* g_ort = NULL;
static OrtEnv* g_env = NULL;
static OrtSession* g_policy_session = NULL;  /* 策略网络 */
static OrtSession* g_value_session = NULL;   /* 价值网络 */
static OrtMemoryInfo* g_memory_info = NULL;
static int g_use_gpu = 0;

static wchar_t* to_wide(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    wchar_t* wstr = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!wstr) return NULL;
    mbstowcs(wstr, str, len);
    return wstr;
}

static OrtSession* create_session(const char* model_path, const char* name) {
    if (!g_ort || !g_env) return NULL;

    OrtSessionOptions* session_options;
    OrtStatus* status = g_ort->CreateSessionOptions(&session_options);
    if (status != NULL) {
        fprintf(stderr, "[NN] %s 创建会话选项失败: %s\n", name, g_ort->GetErrorMessage(status));
        g_ort->ReleaseStatus(status);
        return NULL;
    }
    g_ort->SetIntraOpNumThreads(session_options, 4);
    g_ort->SetSessionGraphOptimizationLevel(session_options, ORT_ENABLE_ALL);

    if (g_use_gpu) {
        status = OrtSessionOptionsAppendExecutionProvider_DML(session_options, 0);
        if (status != NULL) {
            printf("[NN] %s DirectML不可用，回退CPU\n", name);
            g_ort->ReleaseStatus(status);
        }
    }

    printf("[NN] 加载%s: %s\n", name, model_path);
    wchar_t* wpath = to_wide(model_path);
    OrtSession* session = NULL;
    status = g_ort->CreateSession(g_env, wpath, session_options, &session);
    free(wpath);
    g_ort->ReleaseSessionOptions(session_options);

    if (status != NULL) {
        fprintf(stderr, "[NN] %s 加载失败: %s\n", name, g_ort->GetErrorMessage(status));
        g_ort->ReleaseStatus(status);
        return NULL;
    }
    printf("[NN] %s 加载成功\n", name);
    return session;
}

int nn_init(const char* model_path) {
    /* 兼容旧接口：尝试加载分离的双模型，如果失败则回退单模型 */
    char policy_path[512], value_path[512];

    /* 从model_path推导双模型路径 */
    const char* dir_end = strrchr(model_path, '\\');
    if (!dir_end) dir_end = strrchr(model_path, '/');
    if (dir_end) {
        size_t dir_len = dir_end - model_path + 1;
        strncpy(policy_path, model_path, dir_len);
        policy_path[dir_len] = '\0';
        strcat(policy_path, "policy.onnx");
        strncpy(value_path, model_path, dir_len);
        value_path[dir_len] = '\0';
        strcat(value_path, "value.onnx");
    } else {
        strcpy(policy_path, "policy.onnx");
        strcpy(value_path, "value.onnx");
    }

    return nn_init_separate(policy_path, value_path);
}

int nn_init_separate(const char* policy_path, const char* value_path) {
    if (g_policy_session || g_value_session) nn_destroy();

    g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (!g_ort) {
        fprintf(stderr, "[NN] 获取ONNX Runtime API失败\n");
        return 0;
    }

    OrtStatus* status = g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "killgame", &g_env);
    if (status != NULL) {
        fprintf(stderr, "[NN] 创建环境失败: %s\n", g_ort->GetErrorMessage(status));
        g_ort->ReleaseStatus(status);
        return 0;
    }

    printf("[NN] 开始初始化（双网络分离模式）...\n");

    /* 先尝试GPU */
    g_use_gpu = 1;
    g_policy_session = create_session(policy_path, "策略网络");
    g_value_session = create_session(value_path, "价值网络");

    /* 如果两个都失败，回退CPU重试 */
    if (!g_policy_session && !g_value_session) {
        printf("[NN] GPU加载失败，回退CPU模式...\n");
        g_use_gpu = 0;
        g_policy_session = create_session(policy_path, "策略网络");
        g_value_session = create_session(value_path, "价值网络");
    }

    if (!g_policy_session && !g_value_session) {
        fprintf(stderr, "[NN] 双模型均加载失败\n");
        return 0;
    }

    status = g_ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &g_memory_info);
    if (status != NULL) {
        fprintf(stderr, "[NN] 创建内存信息失败: %s\n", g_ort->GetErrorMessage(status));
        g_ort->ReleaseStatus(status);
        return 0;
    }

    printf("[NN] 双网络初始化完成 (%s)\n", g_use_gpu ? "GPU" : "CPU");
    return 1;
}

void nn_destroy(void) {
    if (g_memory_info) { g_ort->ReleaseMemoryInfo(g_memory_info); g_memory_info = NULL; }
    if (g_policy_session) { g_ort->ReleaseSession(g_policy_session); g_policy_session = NULL; }
    if (g_value_session) { g_ort->ReleaseSession(g_value_session); g_value_session = NULL; }
    if (g_env) { g_ort->ReleaseEnv(g_env); g_env = NULL; }
}

static int run_session(OrtSession* session, const char* output_name,
                       const float* global_data, const float* players_data,
                       const float* mask_data, float* output, int output_size) {
    if (!session || !g_ort) return 0;

    int64_t global_shape[] = {1, 53};
    int64_t players_shape[] = {1, 8, NN_PLAYER_DIM};
    int64_t mask_shape[] = {1, 8};

    OrtValue* input_tensors[3] = {NULL, NULL, NULL};
    OrtStatus* status;

    status = g_ort->CreateTensorWithDataAsOrtValue(
        g_memory_info, (void*)global_data, 53 * sizeof(float),
        global_shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_tensors[0]);
    if (status != NULL) goto cleanup;

    status = g_ort->CreateTensorWithDataAsOrtValue(
        g_memory_info, (void*)players_data, 8 * NN_PLAYER_DIM * sizeof(float),
        players_shape, 3, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_tensors[1]);
    if (status != NULL) goto cleanup;

    status = g_ort->CreateTensorWithDataAsOrtValue(
        g_memory_info, (void*)mask_data, 8 * sizeof(float),
        mask_shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_tensors[2]);
    if (status != NULL) goto cleanup;

    const char* input_names[] = {"global_input", "player_input", "mask_input"};
    OrtValue* output_tensor = NULL;

    status = g_ort->Run(session, NULL,
                        input_names, (const OrtValue* const*)input_tensors, 3,
                        &output_name, 1, &output_tensor);
    if (status != NULL) {
        fprintf(stderr, "[NN] 推理失败: %s\n", g_ort->GetErrorMessage(status));
        goto cleanup;
    }

    float* output_data = NULL;
    g_ort->GetTensorMutableData(output_tensor, (void**)&output_data);
    if (output_data && output) {
        memcpy(output, output_data, output_size * sizeof(float));
    }

    g_ort->ReleaseValue(output_tensor);
    for (int i = 0; i < 3; i++) {
        if (input_tensors[i]) g_ort->ReleaseValue(input_tensors[i]);
    }
    return 1;

cleanup:
    if (status) g_ort->ReleaseStatus(status);
    for (int i = 0; i < 3; i++) {
        if (input_tensors[i]) g_ort->ReleaseValue(input_tensors[i]);
    }
    return 0;
}

int nn_infer(const float* global_data, const float* players_data,
             const float* mask_data, NNResult* out) {
    if (!out) return 0;
    memset(out, 0, sizeof(NNResult));

    int ok = 0;
    if (g_value_session) {
        ok |= run_session(g_value_session, "value_output",
                          global_data, players_data, mask_data,
                          &out->value, 1);
    }
    if (g_policy_session) {
        ok |= run_session(g_policy_session, "policy_output",
                          global_data, players_data, mask_data,
                          out->policy, 10000);
    }
    return ok;
}

int nn_infer_policy(const float* global_data, const float* players_data,
                    const float* mask_data, float* policy_out) {
    if (!g_policy_session) return 0;
    return run_session(g_policy_session, "policy_output",
                       global_data, players_data, mask_data, policy_out, 10000);
}

int nn_infer_value(const float* global_data, const float* players_data,
                   const float* mask_data, float* value_out) {
    if (!g_value_session) return 0;
    return run_session(g_value_session, "value_output",
                       global_data, players_data, mask_data, value_out, 1);
}

int nn_policy_available(void) { return g_policy_session != NULL; }
int nn_value_available(void) { return g_value_session != NULL; }
