#ifndef GILGAMESH_H
#define GILGAMESH_H

#include "../hero.h"

/* 注册角色信息 */
void gilgamesh_register(Hero* h);

/*
 * 一技能【所见】：出牌阶段限3次，从牌堆检索一张指定类型的牌
 *   type     : 牌大类(CARD_BASIC/CARD_TRICK/CARD_EQUIP/CARD_DELAYED)
 *   sub_type : 子类型(对应 basic_type/trick_type/equip_type/delayed_type)
 *   返回检索到的牌指针，加入手牌；NULL=没找到或次数用尽
 */
Card* gilgamesh_suojian(GameState* g, int player_idx,
                         CardType type, int sub_type);

/*
 * 二技能【乖离】：出牌阶段限4次，弃置手牌中某一种花色的牌，摸等量的牌
 *   suit : 指定花色
 *   返回弃置的牌数，0=没有该花色牌或次数用尽
 */
int gilgamesh_guaili(GameState* g, int player_idx, Suit suit);

/*
 * 三技能【天辟】：出牌阶段限1次，弃置手牌中某一种点数的牌，对目标造成X点伤害
 *   target_idx : 目标玩家索引
 *   rank       : 指定点数
 *   返回弃置的牌数（=造成的伤害），0=没有该点数牌或次数用尽
 */
int gilgamesh_tianpi(GameState* g, int player_idx, int target_idx, Rank rank);

/* 技能使用分发函数 */
void gilgamesh_use_skill(GameState* g, int player_idx, int skill_idx);

/* AI自动使用技能，返回1表示用了 */
int gilgamesh_ai_use_skill(GameState* g, int player_idx);

#endif /* GILGAMESH_H */
