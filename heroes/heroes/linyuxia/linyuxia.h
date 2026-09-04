#ifndef LINYUXIA_H
#define LINYUXIA_H

#include "../hero.h"

/* 注册角色信息到 Hero 结构体（包含技能回调函数指针） */
void linyuxia_register(Hero* h);

/* ===== 琉璃（锁定技） ===== */
int linyuxia_damage_reduce(GameState* g, int victim_idx, int amount);
void linyuxia_on_damage_reduced(GameState* g, int victim_idx, int reduced);

/* ===== 玉盏（主动技） ===== */
int linyuxia_can_use_skill(GameState* g, int player_idx, int skill_idx);
void linyuxia_use_skill(GameState* g, int player_idx, int skill_idx);
void linyuxia_yuzhan_confirm(GameState* g);  /* 确认发动 */
void linyuxia_yuzhan_cancel(GameState* g);   /* 取消发动 */

/* ===== 事件回调 ===== */
void linyuxia_on_turn_start(GameState* g, int player_idx);
void linyuxia_on_turn_end(GameState* g, int player_idx);
void linyuxia_on_round_start(GameState* g, int player_idx);  /* 每轮开始：盾为0则获得1盾 */
void linyuxia_on_card_used(GameState* g, int player_idx, Card* card);

/* AI自动使用技能（玉盏），返回1表示用了 */
int linyuxia_ai_use_skill(GameState* g, int player_idx);

#endif /* LINYUXIA_H */
