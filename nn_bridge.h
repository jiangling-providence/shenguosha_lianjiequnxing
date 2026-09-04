#ifndef NN_BRIDGE_H
#define NN_BRIDGE_H

/* 神经网络推理结果 */
typedef struct {
    float value;           /* 状态价值（0-1） */
    float policy[10000];   /* 动作logits（10000个动作） */
} NNResult;

/* 初始化神经网络（兼容旧接口：自动推导policy.onnx和value.onnx路径） */
int nn_init(const char* model_path);

/* 初始化双网络分离模式 */
int nn_init_separate(const char* policy_path, const char* value_path);

/* 释放神经网络 */
void nn_destroy(void);

/* 推理：同时返回价值和策略（兼容接口） */
int nn_infer(const float* global_data,    /* 53维 */
             const float* players_data,   /* 8*NN_PLAYER_DIM维 */
             const float* mask_data,      /* 8维 */
             NNResult* out);

/* 仅策略网络推理 */
int nn_infer_policy(const float* global_data,
                    const float* players_data,
                    const float* mask_data,
                    float* policy_out);    /* 10000维输出 */

/* 仅价值网络推理 */
int nn_infer_value(const float* global_data,
                   const float* players_data,
                   const float* mask_data,
                   float* value_out);      /* 1维输出 */

/* 查询网络是否可用 */
int nn_policy_available(void);
int nn_value_available(void);

#endif /* NN_BRIDGE_H */
