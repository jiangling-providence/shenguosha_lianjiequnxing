#include <stdio.h>
#include <string.h>
#include "linyuxia.h"
#include "../../game.h"
#include "../../player.h"


/* ===== 注册角色信息（包含技能回调函数指针） ===== */
void linyuxia_register(Hero* h)
{
    if (!h) return;
    memset(h, 0, sizeof(Hero));
    h->id = HERO_LINYUXIA;
    strncpy(h->name, "linyuxia", 31);
    h->max_hp = 2;
    h->max_shield = 1;
    h->initial_shield = 1;  /* 初始盾量为1 */

    h->skill_count = 3;
    /* 一技能：琉璃（锁定技） */
    strncpy(h->skills[0].name, "琉璃", 31);
    strncpy(h->skills[0].desc,
            "锁定技，若你有盾，你受到的伤害始终减一，你每发动一次琉璃，摸一张牌。",
            255);
    h->skills[0].type = SKILL_LOCKED;
    h->skills[0].allowed_phases = 0;
    h->skills[0].max_uses = -1;
    h->skills[0].used_count = 0;
    h->skills[0].active = 0;

    /* 二技能：玉盏（主动技，出牌阶段限一次） */
    strncpy(h->skills[1].name, "玉盏", 31);
    strncpy(h->skills[1].desc,
            "出牌阶段一次，失去一个盾，摸四张牌，本回合使用的牌可增减一个目标。凑齐四花色回合结束摸2牌并获得花色免疫。",
            255);
    h->skills[1].type = SKILL_ACTIVE;
    h->skills[1].allowed_phases = HERO_PHASE_PLAY;
    h->skills[1].max_uses = 1;
    h->skills[1].used_count = 0;
    h->skills[1].active = 0;

    /* 三技能：凝盾（锁定技） */
    strncpy(h->skills[2].name, "凝盾", 31);
    strncpy(h->skills[2].desc,
            "锁定技，每轮开始时，若你的盾数为0，则获得一个盾。",
            255);
    h->skills[2].type = SKILL_LOCKED;
    h->skills[2].allowed_phases = 0;
    h->skills[2].max_uses = -1;
    h->skills[2].used_count = 0;
    h->skills[2].active = 0;

    /* 注册回调函数指针 */
    h->damage_reduce     = linyuxia_damage_reduce;
    h->on_damage_reduced = linyuxia_on_damage_reduced;
    h->on_turn_start     = linyuxia_on_turn_start;
    h->on_turn_end       = linyuxia_on_turn_end;
    h->on_round_start    = linyuxia_on_round_start;
    h->on_card_used      = linyuxia_on_card_used;
    h->can_use_skill     = linyuxia_can_use_skill;
    h->use_skill         = linyuxia_use_skill;
    h->ai_use_skill      = linyuxia_ai_use_skill;
}


/* ================================================================
 * 琉璃（锁定技）：有盾时伤害减一
 * ================================================================ */
int linyuxia_damage_reduce(GameState* g, int victim_idx, int amount)
{
    if(!g || victim_idx < 0 || victim_idx >= g->player_count) return 0;
    Player* p = &g->players[victim_idx];
    if(p->hero_id != HERO_LINYUXIA) return 0;
    if(p->shield <= 0) return 0;
    if(amount <= 0) return 0;
    return 1;
}


void linyuxia_on_damage_reduced(GameState* g, int victim_idx, int reduced)
{
    if(!g || victim_idx < 0 || victim_idx >= g->player_count) return;
    if(reduced <= 0) return;
    Player* p = &g->players[victim_idx];
    if(p->hero_id != HERO_LINYUXIA) return;

    game_draw_cards(g, victim_idx, 1);
    game_log(g, "【琉璃】%s发动琉璃，伤害减%d，摸1张牌", p->name, reduced);
}


/* ================================================================
 * 玉盏（主动技）
 * ================================================================ */
int linyuxia_can_use_skill(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    if(skill_idx != 1) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_LINYUXIA) return 0;

    return (g->phase == PHASE_PLAY &&
            g->current_player == player_idx &&
            p->skill_used[1] == 0 &&
            p->shield > 0);
}


void linyuxia_use_skill(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    if(skill_idx != 1) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_LINYUXIA) return;
    if(!linyuxia_can_use_skill(g, player_idx, skill_idx)) return;

    if(p->is_ai)
    {
        /* AI直接发动，不进入玩家确认状态 */
        p->shield--;
        p->skill_used[1] = 1;
        p->yuzhan_active = 1;
        game_draw_cards(g, player_idx, 4);
        game_log(g, "【玉盏】%s失去1盾（剩余%d盾），摸4张牌，本回合用牌可增减目标", p->name, p->shield);
    }
    else
    {
        /* 玩家进入确认状态 */
        g->resp_state = RESPONSE_NEED_LINYUXIA_YUZHAN;
        game_log(g, "【玉盏】是否发动？失去1盾，摸4张牌（点击确认发动，取消放弃）");
    }
}

/* 玉盏确认发动 */
void linyuxia_yuzhan_confirm(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_LINYUXIA_YUZHAN) return;
    Player* p = &g->players[0];
    if(p->hero_id != HERO_LINYUXIA) return;

    p->shield--;
    p->skill_used[1] = 1;
    p->yuzhan_active = 1;  /* 玉盏发动，本回合用牌可增减目标 */
    game_draw_cards(g, 0, 4);

    game_log(g, "【玉盏】%s失去1盾（剩余%d盾），摸4张牌，本回合用牌可增减目标", p->name, p->shield);
    g->resp_state = RESPONSE_NONE;
}

/* 玉盏取消发动 */
void linyuxia_yuzhan_cancel(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_LINYUXIA_YUZHAN) return;
    game_log(g, "【玉盏】取消发动");
    g->resp_state = RESPONSE_NONE;
}


/* ================================================================
 * 事件回调
 * ================================================================ */
void linyuxia_on_turn_start(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_LINYUXIA) return;

    if(p->immune_suit != -1) {
        game_log(g, "【玉盏】%s的花色免疫效果结束", p->name);
        p->immune_suit = -1;
    }
    p->skill_used[1] = 0;
    p->yuzhan_active = 0;  /* 玉盏效果只持续本回合 */
    memset(p->suits_used, 0, sizeof(p->suits_used));
}


/* ================================================================
 * 凝盾（锁定技）：每轮开始时，若盾数为0，则获得一个盾
 * ================================================================ */
void linyuxia_on_round_start(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_LINYUXIA) return;
    if(!p->alive) return;

    if(p->shield <= 0)
    {
        p->shield = 1;
        game_log(g, "【凝盾】%s每轮开始获得1个盾（当前%d盾）", p->name, p->shield);
    }
}


void linyuxia_on_turn_end(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_LINYUXIA) return;

    int all_suits = 1;
    for(int i = 0; i < 4; i++) {
        if(!p->suits_used[i]) { all_suits = 0; break; }
    }

    if(all_suits) {
        game_draw_cards(g, player_idx, 2);
        game_log(g, "【玉盏】%s本回合凑齐四种花色，摸2张牌", p->name);

        if(p->hand_count > 0) {
            Card* show = p->hand[0];
            if(show && show->suit >= 0 && show->suit < 4) {
                p->immune_suit = show->suit;
                g->central_show_card = show;
                const char* suit_names[] = {"黑桃", "红桃", "梅花", "方块"};
                game_log(g, "【玉盏】%s展示【%s】，下回合前免疫%s花色伤害",
                         p->name, card_get_full_name(show), suit_names[show->suit]);
            }
        }
    }
}


void linyuxia_on_card_used(GameState* g, int player_idx, Card* card)
{
    if(!g || !card || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_LINYUXIA) return;

    if(card->suit >= 0 && card->suit < 4) {
        p->suits_used[card->suit] = 1;
    }
}


/* ================================================================
 * AI自动使用技能（玉盏）
 * 策略：有盾且手牌少时，失去1盾摸4张牌
 * ================================================================ */
int linyuxia_ai_use_skill(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_LINYUXIA) return 0;
    if(!p->alive) return 0;
    if(g->phase != PHASE_PLAY || g->current_player != player_idx) return 0;
    if(g->resp_state != RESPONSE_NONE) return 0;

    /* 检查玉盏是否可用（有盾且本回合未使用） */
    if(p->shield <= 0 || p->skill_used[1] >= 1) return 0;

    /* 手牌少（<=2）时优先用玉盏摸牌 */
    if(p->hand_count > 2) return 0;

    /* 直接发动玉盏效果 */
    p->shield--;
    p->skill_used[1] = 1;
    p->yuzhan_active = 1;  /* 玉盏发动，本回合用牌可增减目标 */
    game_draw_cards(g, player_idx, 4);
    game_log(g, "【玉盏】%s失去1盾（剩余%d盾），摸4张牌，本回合用牌可增减目标", p->name, p->shield);

    return 1;
}
