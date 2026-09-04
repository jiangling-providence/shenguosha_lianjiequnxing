#ifndef PALADIN_H
#define PALADIN_H

#include "../hero.h"

/* 注册角色信息到 Hero 结构体 */
void paladin_register(Hero* h);

/* 圣骑士技能统一入口（use_skill 回调） */
void paladin_use_skill(GameState* g, int player_idx, int skill_idx);

/* ===== 神圣护盾（锁定技） ===== */
/* 每名角色回合开始时触发，当前回合角色选择一项 */
void paladin_on_any_turn_start(GameState* g, int paladin_idx, int turn_player_idx);

/* 盾数变化：摸X+1张牌（X为变化绝对值） */
void paladin_shield_changed(GameState* g, int paladin_idx, int delta);

/* 受到伤害时：有盾则优先扣盾，不扣体力不濒死 */
/* 返回实际扣的盾数，0表示没有扣盾 */
int paladin_take_damage(GameState* g, int paladin_idx, int amount);

/* ===== 破灭护盾 ===== */
/* 响应牌时触发神圣护盾选择，或主动发动选择牌名打出虚拟牌，每名角色回合限1次 */
int paladin_can_use_pomie(GameState* g, int paladin_idx);
void paladin_use_pomie(GameState* g, int paladin_idx);

/* 破灭护盾：牌名选择相关 */
int paladin_pomie_get_card_count(void);
const char* paladin_pomie_get_card_name(int idx);
void paladin_pomie_select_card(GameState* g, int card_idx);
void paladin_pomie_confirm(GameState* g);
void paladin_pomie_cancel(GameState* g);
void paladin_pomie_resume_response(GameState* g); /* 响应模式：神圣护盾完成后恢复响应 */

/* AI自动选择神圣护盾的选项 */
void paladin_ai_choose(GameState* g, int paladin_idx, int turn_player_idx);

/* AI自动使用技能（破灭护盾），返回1表示用了 */
int paladin_ai_use_skill(GameState* g, int paladin_idx);

/* 玩家选择选项后执行（option: 1-4） */
void paladin_choose_option(GameState* g, int option);

/* 判断某个选项是否可用（根据当前盾量） */
int paladin_option_available(GameState* g, int option);

/* 弃牌选择：点击手牌时选中/取消选中 */
void paladin_select_discard_card(GameState* g, int hand_index);

/* 玩家点击确定后执行弃牌 */
void paladin_confirm_discard(GameState* g);

#endif /* PALADIN_H */
