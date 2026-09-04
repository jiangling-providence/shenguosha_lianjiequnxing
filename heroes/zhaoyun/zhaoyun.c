#include <stdio.h>
#include <string.h>
#include "zhaoyun.h"
#include "../../game.h"
#include "../../player.h"

/* ===== 注册角色信息 ===== */
void zhaoyun_register(Hero* h)
{
    if (!h) return;
    memset(h, 0, sizeof(Hero));

    h->id = HERO_ZHAOYUN;
    strncpy(h->name, "zhaoyun", 31);
    h->max_hp = 4;
    h->skill_count = 2;

    /* 一技能：龙胆（主动技） */
    strncpy(h->skills[0].name, "龙胆", 31);
    strncpy(h->skills[0].desc,
            "你可以点击此技能，将杀当闪、闪当杀使用或打出（花色点数不变），随时可发动。",
            255);
    h->skills[0].type = SKILL_ACTIVE;
    h->skills[0].allowed_phases = HERO_PHASE_ALL;
    h->skills[0].max_uses = -1;
    h->skills[0].used_count = 0;
    h->skills[0].active = 0;

    /* 二技能：虎威（锁定技） */
    strncpy(h->skills[1].name, "虎威", 31);
    strncpy(h->skills[1].desc,
            "你每次发动【龙胆】后，摸两张牌。",
            255);
    h->skills[1].type = SKILL_LOCKED;
    h->skills[1].allowed_phases = 0;
    h->skills[1].max_uses = -1;
    h->skills[1].used_count = 0;
    h->skills[1].active = 0;

    /* 设置技能回调函数 */
    h->can_use_skill = zhaoyun_can_use_skill;
    h->use_skill = zhaoyun_use_skill;
    h->ai_use_skill = zhaoyun_ai_use_skill;

    /* 调试日志：确认龙胆技能类型 */
    printf("[DEBUG] zhaoyun_register: 龙胆技能类型=%d (0=锁定,1=被动,2=主动)\n", h->skills[0].type);
}

/* ===== 龙胆：能否发动主动技能 ===== */
int zhaoyun_can_use_skill(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_ZHAOYUN) return 0;
    if(skill_idx != 0) return 0;  /* 只有龙胆是主动技能 */

    /* 龙胆模式已激活时，显示为可点击（再次点击取消） */
    if(p->longdan_active) return 1;

    /* 检查是否有杀或闪可以转换 */
    for(int i = 0; i < p->hand_count; i++)
    {
        if(p->hand[i] && p->hand[i]->type == CARD_BASIC)
        {
            BasicType bt = p->hand[i]->sub.basic.basic_type;
            if(bt == BASIC_SHA || bt == BASIC_SHAN) return 1;
        }
    }
    return 0;
}

/* ===== 龙胆：发动主动技能 ===== */
void zhaoyun_use_skill(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_ZHAOYUN) return;
    if(skill_idx != 0) return;

    if(p->longdan_active)
    {
        /* 已激活：再次点击取消龙胆模式 */
        p->longdan_active = 0;
        game_log(g, "%s 取消【龙胆】模式", p->name);
    }
    else
    {
        /* 未激活：进入龙胆模式 */
        p->longdan_active = 1;
        game_log(g, "%s 发动【龙胆】！杀当闪、闪当杀（花色点数不变），点击手牌打出", p->name);
    }
}

/* ===== 龙胆：杀当闪，闪当杀 ===== */
int zhaoyun_longdan_can(const Card* card, BasicType want_type)
{
    if (!card) return 0;
    if (card->type != CARD_BASIC) return 0;
    BasicType bt = card->sub.basic.basic_type;

    /* 原生就是想要的类型 */
    if (bt == want_type) return 1;
    /* 杀当闪 */
    if (want_type == BASIC_SHAN && bt == BASIC_SHA) return 1;
    /* 闪当杀 */
    if (want_type == BASIC_SHA && bt == BASIC_SHAN) return 1;
    return 0;
}

BasicType zhaoyun_longdan_result(const Card* card)
{
    if (!card || card->type != CARD_BASIC) return BASIC_SHA;
    if (card->sub.basic.basic_type == BASIC_SHA) return BASIC_SHAN;
    if (card->sub.basic.basic_type == BASIC_SHAN) return BASIC_SHA;
    return card->sub.basic.basic_type;
}

/* ===== 虎威：使用龙胆后摸两张牌 ===== */
void zhaoyun_huwei(GameState* g, int player_idx)
{
    if (!g) return;
    if (player_idx < 0 || player_idx >= g->player_count) return;
    if (g->players[player_idx].hero_id != HERO_ZHAOYUN) return;

    Hero* h = hero_get(HERO_ZHAOYUN);
    h->skills[0].used_count++;  /* 龙胆使用次数+1 */

    game_draw_cards(g, player_idx, 2);
    game_log(g, "%s 发动【虎威】，摸两张牌",
             g->players[player_idx].name);
}

/* ===== AI响应选牌：龙胆转换 ===== */
int zhaoyun_ai_pick_response(GameState* g, int player_idx,
                              BasicType need_type, int* used_skill)
{
    if (used_skill) *used_skill = 0;
    if (!g || player_idx < 0 || player_idx >= g->player_count) return -1;
    Player* p = &g->players[player_idx];

    /* 找可以用龙胆转换的牌 */
    for (int i = 0; i < p->hand_count; i++) {
        if (p->hand[i] && zhaoyun_longdan_can(p->hand[i], need_type)) {
            /* 如果是原生牌，不算技能转换 */
            if (p->hand[i]->sub.basic.basic_type != need_type) {
                if (used_skill) *used_skill = 1;
            }
            return i;
        }
    }
    return -1;
}


/* ================================================================
 * AI自动使用技能（龙胆：闪当杀）
 * 策略：没杀但有闪且能打到人时，用闪当杀
 * ================================================================ */
int zhaoyun_ai_use_skill(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_ZHAOYUN) return 0;
    if(!p->alive) return 0;
    if(g->phase != PHASE_PLAY || g->current_player != player_idx) return 0;
    if(g->resp_state != RESPONSE_NONE) return 0;

    int enemy_idx = (player_idx == 0) ? 1 : 0;
    Player* enemy = &g->players[enemy_idx];
    if(!enemy->alive) return 0;

    /* 检查是否已经有杀 */
    int has_sha = 0;
    for(int i = 0; i < p->hand_count; i++)
    {
        if(p->hand[i] && p->hand[i]->type == CARD_BASIC &&
           p->hand[i]->sub.basic.basic_type == BASIC_SHA)
        {
            has_sha = 1;
            break;
        }
    }
    if(has_sha) return 0;  /* 有杀就不用龙胆了 */

    /* 检查是否有闪 */
    int shan_idx = -1;
    for(int i = 0; i < p->hand_count; i++)
    {
        if(p->hand[i] && p->hand[i]->type == CARD_BASIC &&
           p->hand[i]->sub.basic.basic_type == BASIC_SHAN)
        {
            shan_idx = i;
            break;
        }
    }
    if(shan_idx == -1) return 0;  /* 没闪也用不了 */

    /* 检查距离和出杀次数 */
    int dist = game_calc_distance(g, player_idx, enemy_idx);
    int range = player_attack_range(p);
    if(dist > range || p->sha_used >= 1) return 0;

    /* 激活龙胆，打出闪当杀 */
    p->longdan_active = 1;
    game_log(g, "%s 发动【龙胆】，将闪当杀打出", p->name);
    game_use_card(g, player_idx, shan_idx, enemy_idx);
    p->longdan_active = 0;

    /* 虎威：发动龙胆后摸两张牌 */
    zhaoyun_huwei(g, player_idx);

    return 1;
}
