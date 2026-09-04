#ifndef YUDIE_H
#define YUDIE_H
#include "../hero.h"

/* 注册角色信息到 Hero 结构体 */
void yudie_register(Hero* h);

/* 回调函数 */
void yudie_on_turn_start(GameState* g, int player_idx);
void yudie_on_play_card(GameState* g, int player_idx, Card* card);
int  yudie_hand_limit_mod(GameState* g, int player_idx);
void yudie_on_phase_end(GameState* g, int player_idx);
void yudie_on_dying(GameState* g, int player_idx);
void yudie_use_skill(GameState* g, int player_idx, int skill_idx);

/* 破茧：准备阶段展示牌的花色 */
int yudie_get_break_suit(const Player* p);

/* 破茧：判断一张牌是否不计入手牌上限（与展示牌同花色） */
int yudie_is_free_suit(const Player* p, int suit);

/* 破茧：使用/打出展示花色牌后摸牌 */
void yudie_on_play_break_suit(GameState* g, int player_idx, int suit);

/* 飞舞：出牌阶段限一次，置牌入装备区并亮牌 */
void yudie_feiwuu(GameState* g, int player_idx);

/* 飞舞：累计获得牌数 */
int yudie_get_feiwuu_cards(const Player* p);

/* 成蝶：检查使命技成功条件 */
int yudie_check_chengdie(GameState* g, int player_idx);

/* 成蝶：使命技成功，进化 */
void yudie_evolve(GameState* g, int player_idx);

/* 成蝶：使命技失败（濒死时） */
void yudie_mission_fail(GameState* g, int player_idx);

/* 化蝶：出牌阶段限一次，置一张牌入装备区并亮牌 */
void yudie_huadie(GameState* g, int player_idx);

/* 成形：使用牌后检查同花色连摸 */
void yudie_chengxing_on_play(GameState* g, int player_idx, Card* card);

/* 成形：判断一张牌是否视为包含所有花色（本回合第一张） */
int yudie_is_all_suit(const Player* p, Card* card);

/* 化形：结束阶段转化牌 */
void yudie_huaxing(GameState* g, int player_idx);

/* 化形①：选花色/选手牌/选锦囊名/结束 */
void yudie_huaxing_pick_suit(GameState* g, int player_idx, int suit);
void yudie_huaxing_pick_hand(GameState* g, int player_idx, int hand_idx);
void yudie_huaxing_pick_trick(GameState* g, int player_idx, int trick_idx);
void yudie_huaxing_end(GameState* g, int player_idx);

/* 化形①：根据牌名创建虚拟牌并使用 */
void yudie_huaxing_use_trick(GameState* g, int player_idx, const char* trick_name, int hand_index);

/* 化形②：其他角色回合结束时触发 */
void yudie_huaxing_phase2_on_turn_end(GameState* g, int yudie_idx, int turn_idx);

/* 化形②：重置每回合使用次数 */
void yudie_huaxing_phase2_reset(GameState* g, int yudie_idx);

/* 化形②：设置目标角色 */
void yudie_huaxing_set_target(GameState* g, int yudie_idx, int target_idx);

/* 装备区花色数（用于化蝶计算Y） */
int yudie_get_equip_suit_count(const Player* p);

/* 判断一张牌是否与装备区某张牌同花色 */
int yudie_card_matches_equip_suit(Player* p, Card* card);

/* 记录使用过的牌名（用于成蝶统计10种） */
void yudie_record_card_name(GameState* g, int player_idx, const char* name);

/* 获取使用过的不同牌名数 */
int yudie_get_card_name_count(const Player* p);

/* AI自动使用技能（飞舞/化蝶），返回1表示用了 */
int yudie_ai_use_skill(GameState* g, int player_idx);

#endif /* YUDIE_H */
