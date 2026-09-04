#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "game.h"

/* 估算序列化大小 */
int game_state_serialized_size(GameState* g);

/* 序列化游戏状态到缓冲区 */
void game_state_serialize(GameState* g, unsigned char* buf);

/* 从缓冲区反序列化游戏状态 */
void game_state_deserialize(GameState* g, unsigned char* buf);

#endif /* SERIALIZE_H */
