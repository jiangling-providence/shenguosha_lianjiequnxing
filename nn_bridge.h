#ifndef NN_BRIDGE_H
#define NN_BRIDGE_H

/* 神经网络推理结果 */
typedef struct {
    float value;           /* 状态价值（0-1） */
    float policy[10000];   /* 动作logits（10000个动作） */
} NNResult;

/* 初始化神经网络（加载ONNX模型） */
int nn_init(const char* model_path);

/* 释放神经网络 */
void nn_destroy(void);

/* 推理：传入状态张量，返回价值和策略 */
int nn_infer(const float* global_data,    /* 53维 */
             const float* players_data,   /* 8*796维 */
             const float* mask_data,      /* 8维 */
             NNResult* out);

#endif /* NN_BRIDGE_H */
