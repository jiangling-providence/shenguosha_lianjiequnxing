#ifndef JINGLIU_H
#define JINGLIU_H
#include "../hero.h"

/* 镜流形态 */
#define JINGLIU_FORM_NORMAL   0
#define JINGLIU_FORM_DENGXIAN 1  /* 登仙：失去狂乱 */
#define JINGLIU_FORM_RUMO     2  /* 入魔：失去全部原有技能 */

/* 古镜照神操作选项 */
#define GUJING_OPT_GET_ALL 1     /* 自己回合：摸3，拿所有人一张 */
#define GUJING_OPT_KILL_ALL 2    /* 自己回合：摸5，视为对全体出杀 */
#define GUJING_OPT_RESP 3        /* 回合外响应：摸1，视为需要的牌 */

/* 注册角色信息到 Hero 结构体 */
void jingliu_register(Hero* h);

/* 狂乱：手牌>4弃置至4，获得等量薨标记 */
void jingliu_kuangluan_check(GameState* g, int player_idx);

/* 狂乱弃牌完成后的处理 */
void jingliu_kuangluan_discard_done(GameState* g, int player_idx);

/* 狂乱摸牌加成：返回 X+1，X薨标记 */
int jingliu_draw_bonus(const Player* p);

/* 判断是否免疫过河拆桥（狂乱锁定技） */
int jingliu_cannot_be_guohe(const Player* p);

/* 无罅飞光：展示全部手牌，设置本回合buff */
void jingliu_wuxia_use(GameState* g, int player_idx);

/* 无罅飞光花色3效果：指定目标+区域，弃置一张牌 */
/* zone:0手牌 1武器 2防具 3进攻马 4防御马 5判定区 */
int jingliu_wuxia_discard_zone(GameState* g, int src_idx, int tgt_idx, int zone);

/* 古镜照神 */
void jingliu_gujing_use(GameState* g, int player_idx, int option, int target_idx);

/* 魔阴：濒死无人出桃触发判定 */
void jingliu_moyin_judge(GameState* g, int player_idx);

/* 形态切换：登仙 / 入魔 */
void jingliu_transform(GameState* g, int player_idx, int form);

/* 回合开始回调 */
void jingliu_on_turn_start(GameState* g, int player_idx);

/* 轮次开始回调 */
void jingliu_on_round_start(GameState* g, int player_idx);

/* 牌使用后回调 */
void jingliu_on_card_used(GameState* g, int player_idx, Card* card);

/* 卡牌转化：登仙/入魔视图转换，返回1可转化，out_sub输出转化后类型 */
int jingliu_card_convert(const Player* p, const Card* src_card, int* out_type, int* out_sub);

/* 技能可用性检测 */
int jingliu_can_use_skill(GameState* g, int player_idx, int skill_idx);

/* 主动技能发动 */
void jingliu_use_skill(GameState* g, int player_idx, int skill_idx);

/* AI自动使用技能，返回1表示用了 */
int jingliu_ai_use_skill(GameState* g, int player_idx);

#endif /* JINGLIU_H */
