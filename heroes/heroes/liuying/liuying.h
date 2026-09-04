#ifndef LIUYING_H
#define LIUYING_H
#include "../hero.h"

/* 注册角色信息到 Hero 结构体 */
void liuying_register(Hero* h);

/* 完全燃烧：回合开始时流失X体力并摸X张牌（X=当前体力一半向下取整） */
void liuying_on_turn_start(GameState* g, int player_idx);

/* 迸发：出牌阶段限一次，下次造成的伤害视为火属性/雷属性 */
int  liuying_can_use_bengfa(GameState* g, int player_idx, int skill_idx);
void liuying_use_bengfa(GameState* g, int player_idx, int skill_idx);

/* 过载：使用杀指定角色后，造成伤害回2体力，反之失去1体力 */
void liuying_on_card_used(GameState* g, int player_idx, Card* card);
void liuying_guozai_on_damage(GameState* g, int source_idx);  /* 杀造成伤害时调用 */
void liuying_guozai_on_shan(GameState* g, int source_idx);    /* 杀被闪时调用 */

/* 超新星燃烧：出牌阶段与结束阶段各限一次，对所有角色打出无距离火杀 */
int  liuying_can_use_chaoxing(GameState* g, int player_idx, int skill_idx);
void liuying_use_chaoxing(GameState* g, int player_idx, int skill_idx);

/* 回合结束时重置超新星状态 */
void liuying_on_turn_end(GameState* g, int player_idx);

/* AI自动使用技能，返回1表示用了 */
int liuying_ai_use_skill(GameState* g, int player_idx);

void liuying_bengfa_confirm(GameState* g, int player_idx, int element);
void liuying_bengfa_cancel(GameState* g);
#endif /* LIUYING_H */
