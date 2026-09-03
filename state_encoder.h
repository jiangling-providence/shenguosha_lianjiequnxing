#ifndef STATE_ENCODER_H
#define STATE_ENCODER_H

#include "game.h"

/* 状态编码维度（与Python端state_encoder.py一致） */
#define NN_GLOBAL_DIM 53
#define NN_PLAYER_DIM 797
#define NN_CARD_DIM 23
#define NN_MAX_PLAYERS 8
#define NN_MAX_HAND 30

/* 编码完整游戏状态到神经网络输入 */
/* global_data: 输出53维全局特征 */
/* players_data: 输出8*796维玩家特征（按行存储） */
/* mask_data: 输出8维玩家掩码 */
/* ai_player_idx: AI玩家的索引（自己的手牌明牌，其他玩家只显示数量） */
void encode_game_state(GameState* g, int ai_player_idx,
                       float* global_data,
                       float* players_data,
                       float* mask_data);

#endif /* STATE_ENCODER_H */
