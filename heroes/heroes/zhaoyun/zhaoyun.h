#ifndef ZHAOYUN_H
#define ZHAOYUN_H

#include "../hero.h"

/* 注册角色信息 */
void zhaoyun_register(Hero* h);

/* 龙胆主动技能 */
int  zhaoyun_can_use_skill(GameState* g, int player_idx, int skill_idx);
void zhaoyun_use_skill(GameState* g, int player_idx, int skill_idx);

/*
 * 龙胆：杀当闪，闪当杀
 *   card      : 手牌
 *   want_type : 想要的类型(BASIC_SHA / BASIC_SHAN)
 *   返回 1=可以用(含原生和转换), 0=不能
 */
int zhaoyun_longdan_can(const Card* card, BasicType want_type);

/* 龙胆转换后实际打出的牌类型 */
BasicType zhaoyun_longdan_result(const Card* card);

/* 虎威：使用龙胆后摸两张牌 */
void zhaoyun_huwei(GameState* g, int player_idx);

/*
 * AI响应选牌：龙胆转换
 * 由 hero.c 的统一接口路由调用
 *   返回手牌索引，-1=没有可转换的牌
 */
int zhaoyun_ai_pick_response(GameState* g, int player_idx,
                              BasicType need_type, int* used_skill);

/* AI自动使用技能（龙胆：闪当杀），返回1表示用了 */
int zhaoyun_ai_use_skill(GameState* g, int player_idx);

#endif /* ZHAOYUN_H */
