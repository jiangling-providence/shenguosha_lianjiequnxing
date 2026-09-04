#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "liuying.h"
#include "../../game.h"
#include "../../player.h"


/* ================================================================
 * 注册角色信息
 * ================================================================ */
void liuying_register(Hero* h)
{
    if (!h) return;
    memset(h, 0, sizeof(Hero));
    h->id = HERO_LIUYING;
    strncpy(h->name, "liuying", 31);
    h->max_hp = 6;
    h->skill_count = 4;

    /* 一技能：完全燃烧（锁定技） */
    strncpy(h->skills[0].name, "完全燃烧", 31);
    strncpy(h->skills[0].desc,
            "锁定技，回合开始时，你流失X点体力并摸X张牌（X为你当前体力值的一半向下取整）。",
            255);
    h->skills[0].type = SKILL_LOCKED;
    h->skills[0].allowed_phases = 0;
    h->skills[0].max_uses = -1;
    h->skills[0].used_count = 0;
    h->skills[0].active = 0;

    /* 二技能：迸发（主动技） */
    strncpy(h->skills[1].name, "迸发", 31);
    strncpy(h->skills[1].desc,
            "出牌阶段限一次，下次造成的伤害视为火属性（再点一次切换为雷属性，再点取消）。",
            255);
    h->skills[1].type = SKILL_ACTIVE;
    h->skills[1].allowed_phases = HERO_PHASE_PLAY;
    h->skills[1].max_uses = 1;
    h->skills[1].used_count = 0;
    h->skills[1].active = 0;

    /* 三技能：过载（锁定技） */
    strncpy(h->skills[2].name, "过载", 31);
    strncpy(h->skills[2].desc,
            "锁定技，当你使用杀指定角色后，若此杀造成伤害，你回复2点体力；否则你失去1点体力。",
            255);
    h->skills[2].type = SKILL_LOCKED;
    h->skills[2].allowed_phases = 0;
    h->skills[2].max_uses = -1;
    h->skills[2].used_count = 0;
    h->skills[2].active = 0;

    /* 四技能：超新星燃烧（主动技） */
    strncpy(h->skills[3].name, "超新星燃烧", 31);
    strncpy(h->skills[3].desc,
            "出牌阶段与结束阶段各限一次，你视为对所有其他角色打出一张无距离限制的火杀，每有一人出闪你摸一张牌，每有人因此受到1点伤害你回复1点体力。",
            255);
    h->skills[3].type = SKILL_ACTIVE;
    h->skills[3].allowed_phases = HERO_PHASE_PLAY | HERO_PHASE_END;
    h->skills[3].max_uses = 2;  /* 出牌阶段1次+结束阶段1次 */
    h->skills[3].used_count = 0;
    h->skills[3].active = 0;

    /* 回调函数 */
    h->on_turn_start  = liuying_on_turn_start;
    h->on_turn_end    = liuying_on_turn_end;
    h->on_card_used   = liuying_on_card_used;
    h->can_use_skill  = liuying_can_use_bengfa;  /* 迸发和超新星共用，内部判断 */
    h->use_skill      = liuying_use_bengfa;       /* 迸发和超新星共用，内部判断 */
    h->ai_use_skill   = liuying_ai_use_skill;
}


/* ================================================================
 * 完全燃烧：回合开始时流失X体力并摸X张牌
 * X = 当前体力值的一半向下取整
 * ================================================================ */
void liuying_on_turn_start(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(!p->alive || p->hero_id != HERO_LIUYING) return;

    int x = p->hp / 2;  /* 一半向下取整 */
    if(x <= 0)
    {
        game_log(g, "【完全燃烧】%s体力过低，X=0，不发动", p->name);
        return;
    }

    game_log(g, "【完全燃烧】%s流失%d点体力，摸%d张牌", p->name, x, x);

    /* 流失体力（不触发伤害相关技能） */
    game_lose_hp(g, player_idx, x);

    /* 摸牌 */
    if(p->alive)
    {
        game_draw_cards(g, player_idx, x);
    }
}


/* ================================================================
 * 迸发：检查是否可以使用
 * skill_idx: 1=迸发, 3=超新星燃烧
 * ================================================================ */
int liuying_can_use_bengfa(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(!p->alive || p->hero_id != HERO_LIUYING) return 0;

    /* 迸发：出牌阶段限一次 */
    if(skill_idx == 1)
    {
        if(g->phase != PHASE_PLAY || g->current_player != player_idx) return 0;
        if(p->hero && p->hero->skills[1].used_count >= 1) return 0;
        return 1;
    }

    /* 超新星燃烧：出牌阶段或结束阶段各限一次 */
    if(skill_idx == 3)
    {
        if(g->current_player != player_idx) return 0;
        if(g->phase == PHASE_PLAY)
        {
            if(p->liuying.chaoxing_used_play) return 0;
            return 1;
        }
        if(g->phase == PHASE_END)
        {
            if(p->liuying.chaoxing_used_end) return 0;
            return 1;
        }
        return 0;
    }

    return 0;
}


/* ================================================================
 * 迸发：使用技能
 * skill_idx: 1=迸发, 3=超新星燃烧
 * ================================================================ */
void liuying_use_bengfa(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(!p->alive || p->hero_id != HERO_LIUYING) return;

    /* 迸发：进入选择界面，让玩家选择火属性/雷属性（出牌阶段限一次） */
    if(skill_idx == 1)
    {
        /* 标记技能已使用（在选择确认后才真正消耗次数，这里先不设置） */
        g->resp_state = RESPONSE_NEED_LIUYING_BENGFA;
        game_log(g, "【迸发】请选择下次伤害的属性（火/雷）");
        return;
    }

    /* 超新星燃烧：对所有其他角色打出无距离火杀 */
    if(skill_idx == 3)
    {
        if(g->phase == PHASE_PLAY)
            p->liuying.chaoxing_used_play = 1;
        else if(g->phase == PHASE_END)
            p->liuying.chaoxing_used_end = 1;

        liuying_use_chaoxing(g, player_idx, skill_idx);
        return;
    }
}


/* ================================================================
 * 超新星燃烧：对所有其他角色打出无距离火杀
 * 每有一人出闪摸一张牌，每有人受到1点伤害回复1体力
 * ================================================================ */
void liuying_use_chaoxing(GameState* g, int player_idx, int skill_idx)
{
    (void)skill_idx;
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(!p->alive) return;

    game_log(g, "【超新星燃烧】%s对所有其他角色打出无距离火杀！", p->name);

    int shan_count = 0;   /* 出闪人数 */
    int damage_count = 0; /* 造成伤害总数 */

    for(int i = 0; i < g->player_count; i++)
    {
        if(i == player_idx) continue;
        Player* target = &g->players[i];
        if(!target->alive) continue;

        /* 检查目标是否有闪 */
        int has_shan = 0;
        for(int j = 0; j < target->hand_count; j++)
        {
            if(target->hand[j] &&
               target->hand[j]->type == CARD_BASIC &&
               target->hand[j]->sub.basic.basic_type == BASIC_SHAN)
            {
                has_shan = 1;
                /* AI或训练环境中自动打出闪 */
                if(target->is_ai || g_training_mode)
                {
                    Card* shan = player_remove_hand(target, j);
                    discard_add(&g->discard, shan);
                    g->central_show_card = shan;
                    game_log(g, "%s 打出了【闪】", target->name);
                    shan_count++;
                }
                break;
            }
        }

        /* 玩家目标（游戏本体）：简化处理，默认不出闪 */
        if(!target->is_ai && !g_training_mode && has_shan)
        {
            has_shan = 0;
        }

        /* 没有闪则受到1点火焰伤害 */
        if(!has_shan)
        {
            game_log(g, "%s 没有闪，受到1点火焰伤害", target->name);
            g->current_damage_source = DMG_SRC_SHA;
            game_deal_damage(g, i, 1, player_idx, DMG_FIRE);
            damage_count++;

            /* 每受到1点伤害回复1体力 */
            if(p->alive && p->hp < p->max_hp)
            {
                p->hp++;
                game_log(g, "【超新星燃烧】%s回复1点体力（当前%d）", p->name, p->hp);
            }
        }
    }

    /* 每有一人出闪摸一张牌 */
    if(shan_count > 0 && p->alive)
    {
        game_log(g, "【超新星燃烧】共%d人出闪，%s摸%d张牌",
                 shan_count, p->name, shan_count);
        game_draw_cards(g, player_idx, shan_count);
    }

    game_log(g, "【超新星燃烧】结算完毕：%d人出闪，造成%d点伤害",
             shan_count, damage_count);
}


/* ================================================================
 * 过载：使用杀指定角色后触发
 * 造成伤害回2体力，反之失去1体力
 * ================================================================ */
void liuying_on_card_used(GameState* g, int player_idx, Card* card)
{
    if(!g || !card || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(!p->alive || p->hero_id != HERO_LIUYING) return;

    /* 使用杀时记录过载状态 */
    if(card->type == CARD_BASIC && card->sub.basic.basic_type == BASIC_SHA)
    {
        p->liuying.guozai_sha_active = 1;
        game_log(g, "【过载】%s使用杀，等待结算结果...", p->name);
    }
}


/* ================================================================
 * 过载：杀造成伤害时调用（在 game_deal_damage 里检测）
 * ================================================================ */
void liuying_guozai_on_damage(GameState* g, int source_idx)
{
    if(!g || source_idx < 0 || source_idx >= g->player_count) return;
    Player* p = &g->players[source_idx];
    if(!p->alive || p->hero_id != HERO_LIUYING) return;
    if(!p->liuying.guozai_sha_active) return;

    /* 杀造成伤害，回复2点体力 */
    p->liuying.guozai_sha_active = 0;
    int heal = 2;
    if(p->hp + heal > p->max_hp) heal = p->max_hp - p->hp;
    if(heal > 0)
    {
        p->hp += heal;
        game_log(g, "【过载】%s的杀造成伤害，回复%d点体力（当前%d）",
                 p->name, heal, p->hp);
    }
}


/* ================================================================
 * 过载：杀被闪时调用（在 input_handle_response_y 里检测）
 * ================================================================ */
void liuying_guozai_on_shan(GameState* g, int source_idx)
{
    if(!g || source_idx < 0 || source_idx >= g->player_count) return;
    Player* p = &g->players[source_idx];
    if(!p->alive || p->hero_id != HERO_LIUYING) return;
    if(!p->liuying.guozai_sha_active) return;

    /* 杀被闪，失去1点体力 */
    p->liuying.guozai_sha_active = 0;
    game_log(g, "【过载】%s的杀被闪，失去1点体力", p->name);
    game_lose_hp(g, source_idx, 1);
}


/* ================================================================
 * 回合结束时重置超新星状态
 * ================================================================ */
void liuying_on_turn_end(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(!p->alive || p->hero_id != HERO_LIUYING) return;

    /* 重置迸发（如果还没使用） */
    p->liuying.bengfa_element = 0;

    /* 超新星状态在下回合开始时重置（这里不重置，因为结束阶段还能用） */
}


/* ================================================================
 * AI自动使用技能
 * 策略：超新星燃烧（出牌阶段）> 迸发（有杀时）
 * ================================================================ */
int liuying_ai_use_skill(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_LIUYING) return 0;
    if(!p->alive) return 0;
    if(g->resp_state != RESPONSE_NONE) return 0;

    Hero* h = hero_get(HERO_LIUYING);
    if(!h) return 0;

    /* 超新星燃烧：出牌阶段优先使用 */
    if(g->phase == PHASE_PLAY && !p->liuying.chaoxing_used_play)
    {
        liuying_use_bengfa(g, player_idx, 3);
        return 1;
    }

    /* 迸发：有杀且能打到人时使用 */
    if(g->phase == PHASE_PLAY && g->current_player == player_idx &&
       h->skills[1].used_count < 1)
    {
        int enemy_idx = (player_idx == 0) ? 1 : 0;
        Player* enemy = &g->players[enemy_idx];
        if(enemy->alive)
        {
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
            if(has_sha)
            {
                liuying_use_bengfa(g, player_idx, 1);
                return 1;
            }
        }
    }

    return 0;
}


/* ================================================================
 * 迸发：选择属性确认
 * element: 1=火属性, 2=雷属性
 * ================================================================ */
void liuying_bengfa_confirm(GameState* g, int player_idx, int element)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(!p->alive || p->hero_id != HERO_LIUYING) return;
    if(g->resp_state != RESPONSE_NEED_LIUYING_BENGFA) return;

    p->liuying.bengfa_element = element;  /* 1=火, 2=雷 */
    if(p->hero) p->hero->skills[1].used_count = 1;

    const char* elem_str = (element == 1) ? "火属性" : "雷属性";
    game_log(g, "【迸发】%s下次造成的伤害视为%s", p->name, elem_str);

    g->resp_state = RESPONSE_NONE;
}


/* ================================================================
 * 迸发：取消选择
 * ================================================================ */
void liuying_bengfa_cancel(GameState* g)
{
    if(!g) return;
    if(g->resp_state != RESPONSE_NEED_LIUYING_BENGFA) return;
    game_log(g, "【迸发】取消选择");
    g->resp_state = RESPONSE_NONE;
}
