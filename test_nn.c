#include <stdio.h>
#include <stdlib.h>
#include "nn_bridge.h"
#include "state_encoder.h"

int main() {
    printf("=== 神经网络测试程序 ===\n");
    printf("1. 测试模型加载...\n");

    if (!nn_init("model.onnx")) {
        printf("模型加载失败！\n");
        return 1;
    }
    printf("模型加载成功！\n");

    printf("\n2. 测试推理...\n");
    float global_data[NN_GLOBAL_DIM] = {0};
    float players_data[NN_MAX_PLAYERS * NN_PLAYER_DIM] = {0};
    float mask_data[NN_MAX_PLAYERS] = {0};

    /* 设置一些假数据 */
    global_data[0] = 1.0f;  /* 回合数 */
    global_data[1] = 0.0f;  /* 当前玩家 */
    mask_data[0] = 1.0f;
    mask_data[1] = 1.0f;

    NNResult result;
    if (!nn_infer(global_data, players_data, mask_data, &result)) {
        printf("推理失败！\n");
        return 1;
    }
    printf("推理成功！\n");
    printf("  value = %.4f\n", result.value);
    printf("  policy[0] = %.4f\n", result.policy[0]);
    printf("  policy[1000] = %.4f\n", result.policy[1000]);
    printf("  policy[3000] = %.4f\n", result.policy[3000]);

    printf("\n=== 测试完成 ===\n");
    nn_destroy();
    return 0;
}
