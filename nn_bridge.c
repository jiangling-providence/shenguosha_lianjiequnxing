#include "nn_bridge.h"
#include "state_encoder.h"
#include "onnxruntime_c_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static const OrtApi* g_ort = NULL;
static OrtEnv* g_env = NULL;
static OrtSession* g_session = NULL;
static OrtMemoryInfo* g_memory_info = NULL;

/* 把char*转换为wchar_t* */
static wchar_t* to_wide(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    wchar_t* wstr = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!wstr) return NULL;
    mbstowcs(wstr, str, len);
    return wstr;
}

int nn_init(const char* model_path) {
    if (g_session) nn_destroy();

    // 获取ONNX Runtime API
    g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (!g_ort) {
        fprintf(stderr, "[NN] 获取ONNX Runtime API失败\n");
        return 0;
    }

    // 创建环境
    OrtStatus* status = g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "killgame", &g_env);
    if (status != NULL) {
        fprintf(stderr, "[NN] 创建环境失败: %s\n", g_ort->GetErrorMessage(status));
        g_ort->ReleaseStatus(status);
        return 0;
    }

    printf("[NN] 开始初始化...\n");

    // 创建会话选项
    OrtSessionOptions* session_options;
    status = g_ort->CreateSessionOptions(&session_options);
    if (status != NULL) {
        fprintf(stderr, "[NN] 创建会话选项失败: %s\n", g_ort->GetErrorMessage(status));
        g_ort->ReleaseStatus(status);
        return 0;
    }
    g_ort->SetIntraOpNumThreads(session_options, 4);
    g_ort->SetSessionGraphOptimizationLevel(session_options, ORT_ENABLE_ALL);

    // 使用CPU推理（当前是CPU版本ONNX Runtime）
    printf("[NN] 使用CPU推理\n");

    // 创建会话（加载模型，Windows需要宽字符路径）
    printf("[NN] 加载模型: %s\n", model_path);
    wchar_t* wpath = to_wide(model_path);
    status = g_ort->CreateSession(g_env, wpath, session_options, &g_session);
    free(wpath);
    g_ort->ReleaseSessionOptions(session_options);

    if (status != NULL) {
        fprintf(stderr, "[NN] 加载模型失败: %s\n", g_ort->GetErrorMessage(status));
        g_ort->ReleaseStatus(status);
        return 0;
    }
    printf("[NN] 会话创建成功\n");

    // 创建内存信息
    status = g_ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &g_memory_info);
    if (status != NULL) {
        fprintf(stderr, "[NN] 创建内存信息失败: %s\n", g_ort->GetErrorMessage(status));
        g_ort->ReleaseStatus(status);
        return 0;
    }

    printf("[NN] 模型加载成功: %s\n", model_path);
    return 1;
}

void nn_destroy(void) {
    if (g_memory_info) {
        g_ort->ReleaseMemoryInfo(g_memory_info);
        g_memory_info = NULL;
    }
    if (g_session) {
        g_ort->ReleaseSession(g_session);
        g_session = NULL;
    }
    if (g_env) {
        g_ort->ReleaseEnv(g_env);
        g_env = NULL;
    }
}

int nn_infer(const float* global_data,
             const float* players_data,
             const float* mask_data,
             NNResult* out) {
    if (!g_session || !g_ort || !out) return 0;

    memset(out, 0, sizeof(NNResult));

    // 输入形状
    int64_t global_shape[] = {1, 53};
    int64_t players_shape[] = {1, 8, NN_PLAYER_DIM};
    int64_t mask_shape[] = {1, 8};

    // 创建输入张量
    OrtValue* input_tensors[3] = {NULL, NULL, NULL};
    OrtStatus* status;

    status = g_ort->CreateTensorWithDataAsOrtValue(
        g_memory_info, (void*)global_data, 53 * sizeof(float),
        global_shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_tensors[0]);
    if (status != NULL) { fprintf(stderr, "[NN] 创建global张量失败\n"); goto cleanup; }

    status = g_ort->CreateTensorWithDataAsOrtValue(
        g_memory_info, (void*)players_data, 8 * NN_PLAYER_DIM * sizeof(float),
        players_shape, 3, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_tensors[1]);
    if (status != NULL) { fprintf(stderr, "[NN] 创建players张量失败\n"); goto cleanup; }

    status = g_ort->CreateTensorWithDataAsOrtValue(
        g_memory_info, (void*)mask_data, 8 * sizeof(float),
        mask_shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_tensors[2]);
    if (status != NULL) { fprintf(stderr, "[NN] 创建mask张量失败\n"); goto cleanup; }

    // 输入输出名称
    const char* input_names[] = {"global_input", "player_input", "mask_input"};
    const char* output_names[] = {"value_output", "policy_output"};

    // 运行推理
    OrtValue* output_tensors[2] = {NULL, NULL};
    status = g_ort->Run(g_session, NULL,
                        input_names, (const OrtValue* const*)input_tensors, 3,
                        output_names, 2, output_tensors);
    if (status != NULL) {
        fprintf(stderr, "[NN] 推理失败: %s\n", g_ort->GetErrorMessage(status));
        goto cleanup;
    }

    // 获取输出数据
    float* value_data = NULL;
    float* policy_data = NULL;

    g_ort->GetTensorMutableData(output_tensors[0], (void**)&value_data);
    g_ort->GetTensorMutableData(output_tensors[1], (void**)&policy_data);

    if (value_data) out->value = value_data[0];
    if (policy_data) {
        memcpy(out->policy, policy_data, 10000 * sizeof(float));
    }

    // 释放输出张量
    g_ort->ReleaseValue(output_tensors[0]);
    g_ort->ReleaseValue(output_tensors[1]);

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
