#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "yudie.h"
#include "../../game.h"
#include "../../player.h"
#include "../../card.h"


/* 前置声明 */
int yudie_card_matches_equip_suit(Player* p, Card* card);
static void yudie_huaxing_use_converted(GameState* g, int idx, Card* src_card, const char* trick_name);


/* ================================================================
 * 注册角色信息
 * ================================================================ */
void yudie_register(Hero* h)
{
    if(!h) return;
    memset(h, 0, sizeof(Hero));
    h->id = HERO_YUDIE;
    strncpy(h->name, "yudie", 31);
    h->max_hp = 3;
    h->skill_count = 3;

    /* 一技能：破茧（锁定技） */
    strncpy(h->skills[0].name, "破茧", 31);
    strncpy(h->skills[0].desc,
            "锁定技，准备阶段展示一张牌，本回合同花色牌不计手牌上限；使用/打出该花色牌后摸一张牌；装备牌仅能重铸。",
            255);
    h->skills[0].type = SKILL_LOCKED;
    h->skills[0].allowed_phases = 0;
    h->skills[0].max_uses = -1;
    h->skills[0].used_count = 0;
    h->skills[0].active = 0;

    /* 二技能：飞舞（主动技，出牌阶段限一次） */
    strncpy(h->skills[1].name, "飞舞", 31);
    strncpy(h->skills[1].desc,
            "出牌阶段限一次，将任意张牌置入装备区，亮出牌堆顶(6-X)张牌，获得其中与装备区同花色的牌。",
            255);
    h->skills[1].type = SKILL_ACTIVE;
    h->skills[1].allowed_phases = HERO_PHASE_PLAY;
    h->skills[1].max_uses = 1;
    h->skills[1].used_count = 0;
    h->skills[1].active = 0;

    /* 三技能：成蝶（使命技，锁定技） */
    strncpy(h->skills[2].name, "成蝶", 31);
    strncpy(h->skills[2].desc,
            "使命技，飞舞累计获得≥1张牌且使用过≥1种牌名时进化：失去破茧，飞舞变化蝶，获得化形、成形，体力上限+1。",
            255);
    h->skills[2].type = SKILL_LOCKED;
    h->skills[2].allowed_phases = 0;
    h->skills[2].max_uses = -1;
    h->skills[2].used_count = 0;
    h->skills[2].active = 0;

    /* 回调函数指针 */
    h->on_turn_start  = yudie_on_turn_start;
    h->on_card_used   = yudie_on_play_card;
    h->hand_limit_mod = yudie_hand_limit_mod;
    h->on_turn_end    = yudie_on_phase_end;
    h->on_dying       = yudie_on_dying;
    h->use_skill      = yudie_use_skill;
    h->ai_use_skill   = yudie_ai_use_skill;
}


/* ================================================================
 * 破茧：准备阶段开始时，摸一张牌并展示，记录花色
 * ================================================================ */
void yudie_on_turn_start(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return;
    if(p->yudie.chengdie) return;  /* 已进化后失去破茧 */

    /* 摸一张牌并展示（使用通用展示函数） */
    if(g->deck.count > 0)
    {
        Card* c = deck_draw(&g->deck);
        if(c)
        {
            player_draw_card(p, c);
            p->yudie.break_suit = c->suit;
            game_show_card(g, c, p->name);  /* 屏幕中心展示 */
            char suit_char = card_get_suit_char(c);
            game_log(g, "【破茧】%s展示【%s】(花色%c)，本回合同花色牌不计手牌上限",
                     p->name, card_get_full_name(c), suit_char);
        }
    }

    /* 重置本回合计数 */
    p->yudie.chengxing_count = 0;
    p->yudie.first_card_played = 0;
    p->yudie.last_suit = -1;
}


/* ================================================================
 * 破茧：使用/打出展示花色牌后摸一张牌
 * 成形：同花色连摸牌（每回合限5次）
 * ================================================================ */
void yudie_on_play_card(GameState* g, int player_idx, Card* card)
{
    if(!g || !card || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return;

    /* 记录使用过的牌名（用于成蝶统计） */
    yudie_record_card_name(g, player_idx, card_get_full_name(card));

    /* 检查成蝶使命技成功条件（使用牌后也检查，不仅限于飞舞后） */
    if(!p->yudie.chengdie)
        yudie_check_chengdie(g, player_idx);

    /* 破茧：使用展示花色牌后摸一张 */
    if(!p->yudie.chengdie && p->yudie.break_suit >= 0)
    {
        if(card->suit == p->yudie.break_suit ||
           yudie_is_all_suit(p, card))  /* 成形：第一张牌全花色 */
        {
            if(g->deck.count > 0)
            {
                Card* draw = deck_draw(&g->deck);
                if(draw)
                {
                    player_draw_card(p, draw);
                    char sc1 = card_get_suit_char(card);
                    game_log(g, "【破茧】%s使用%c花色牌，摸一张牌", p->name, sc1);
                }
            }
        }
    }

    /* 成形：同花色连摸牌（已进化后） */
    if(p->yudie.chengdie)
    {
        /* 判断是否同花色：
           - 如果上一张是第一张牌（全花色，last_suit=-2），则当前牌只要有花色就算同花色
           - 否则需要实际花色相同 */
        int is_same_suit = 0;
        if(p->yudie.first_card_played && p->yudie.last_suit >= -1)
        {
            if(p->yudie.last_suit == -2)  /* -2表示上一张是全花色的第一张牌 */
                is_same_suit = 1;
            else if(card->suit == p->yudie.last_suit)
                is_same_suit = 1;
        }

        if(is_same_suit && p->yudie.chengxing_count < 5)
        {
            if(g->deck.count > 0)
            {
                Card* draw = deck_draw(&g->deck);
                if(draw)
                {
                    player_draw_card(p, draw);
                    p->yudie.chengxing_count++;
                    char sc2 = card_get_suit_char(card);
                    game_log(g, "【成形】%s连续使用%c花色牌，摸一张牌（%d/5）",
                             p->name, sc2, p->yudie.chengxing_count);
                }
            }
        }

        /* 记录上一张牌花色：第一张牌用-2表示全花色 */
        if(!p->yudie.first_card_played)
            p->yudie.last_suit = -2;  /* 第一张牌全花色 */
        else
            p->yudie.last_suit = card->suit;
        p->yudie.first_card_played = 1;
    }

    /* 化形①：记录本回合使用过的非装备/非延时锦囊牌 */
    if(p->yudie.chengdie && p->yudie.huaxing_record_count < 20)
    {
        if(card->type == CARD_TRICK && card->sub.trick.trick_type != TRICK_WUXIE)
        {
            const char* name = card_get_full_name(card);
            strncpy(p->yudie.huaxing_record_names[p->yudie.huaxing_record_count], name, 31);
            p->yudie.huaxing_record_names[p->yudie.huaxing_record_count][31] = '\0';
            p->yudie.huaxing_record_suits[p->yudie.huaxing_record_count] = card->suit;
            p->yudie.huaxing_record_ranks[p->yudie.huaxing_record_count] = card->rank;
            p->yudie.huaxing_record_count++;
        }
    }

    /* 检查成蝶使命技成功条件 */
    yudie_check_chengdie(g, player_idx);
}


/* ================================================================
 * 破茧：同花色牌不计入手牌上限
 * ================================================================ */
int yudie_hand_limit_mod(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return 0;
    if(p->yudie.chengdie) return 0;  /* 已进化后失去破茧 */
    if(p->yudie.break_suit < 0) return 0;

    /* 统计手牌中与展示牌同花色的数量 */
    int free_count = 0;
    for(int i = 0; i < p->hand_count; i++)
    {
        if(p->hand[i] && p->hand[i]->suit == p->yudie.break_suit)
            free_count++;
    }
    return free_count;  /* 返回增加的手牌上限 */
}


/* ================================================================
 * 飞舞：出牌阶段限一次，置牌入装备区并亮牌
 * ================================================================ */
void yudie_feiwuu(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return;
    if(p->yudie.chengdie) return;  /* 已进化后用化蝶 */
    if(p->skill_used[1] >= 1) return;  /* 出牌阶段限一次 */

    p->skill_used[1]++;
    p->yudie.feiwuu_count++;

    /* 将所有手牌置入装备区（简化：替换武器） */
    int placed = 0;
    while(p->hand_count > 0 && placed < 4)  /* 最多4个装备槽 */
    {
        Card* c = player_remove_hand(p, 0);
        if(c)
        {
            /* 简化：全部当武器装备 */
            if(p->equip.weapon) discard_add(&g->discard, p->equip.weapon);
            p->equip.weapon = c;
            placed++;
        }
    }
    game_log(g, "【飞舞】%s将%d张牌置入装备区", p->name, placed);

    /* 亮出牌堆顶(6-X)张牌 */
    int reveal_count = 6 - p->yudie.feiwuu_count;
    if(reveal_count < 1) reveal_count = 1;
    if(reveal_count > g->deck.count) reveal_count = g->deck.count;

    Card* revealed[20];
    int reveal_idx = 0;
    for(int i = 0; i < reveal_count; i++)
    {
        Card* c = deck_draw(&g->deck);
        if(c) revealed[reveal_idx++] = c;
    }

    /* 获得其中与装备区花色相同的牌 */
    int got = 0;
    for(int i = 0; i < reveal_idx; i++)
    {
        if(yudie_card_matches_equip_suit(p, revealed[i]))
        {
            player_draw_card(p, revealed[i]);
            got++;
            p->yudie.feiwuu_cards++;
        }
        else
        {
            /* 其余牌置入弃牌堆（简化：原描述是置于牌堆顶） */
            discard_add(&g->discard, revealed[i]);
        }
    }

    game_log(g, "【飞舞】亮出%d张牌，获得%d张同花色牌（累计%d张）",
             reveal_count, got, p->yudie.feiwuu_cards);

    /* 检查成蝶使命技成功条件 */
    yudie_check_chengdie(g, player_idx);
}


/* ================================================================
 * 主动技能发动：飞舞/化蝶
 * ================================================================ */
void yudie_use_skill(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return;

    /* 进化后：skills[0]=化蝶（主动技），skills[1]=化形，skills[2]=成形 */
    /* 进化前：skills[0]=破茧，skills[1]=飞舞（主动技），skills[2]=成蝶 */
    if((p->yudie.chengdie && skill_idx == 0) ||
       (!p->yudie.chengdie && skill_idx == 1))
    {
        if(p->yudie.chengdie)
        {
            /* 化蝶：直接结算 */
            if(!hero_skill_use(g, player_idx, skill_idx)) return;
            yudie_huadie(g, player_idx);
            hero_skill_finish(g, player_idx, skill_idx);
        }
        else
        {
            /* 飞舞：先检查是否可以使用 */
            if(!hero_skill_can_use(g, player_idx, skill_idx)) return;

            if(p->is_ai)
            {
                /* AI发动飞舞：自动选0-2张牌放置到装备区，然后亮牌结算 */
                if(!hero_skill_use(g, player_idx, skill_idx)) return;

                Player* ap = &g->players[player_idx];
                /* AI选前2张牌（如果有）放置到装备区 */
                int equip_count = 0;
                for(int i = 0; i < ap->hand_count && equip_count < 2; i++)
                {
                    Card* c = ap->hand[i];
                    if(!c) continue;
                    /* 简单策略：非基本牌优先放装备区 */
                    if(c->type != CARD_BASIC || equip_count == 0)
                    {
                        Card* removed = player_remove_hand(ap, i);
                        if(removed)
                        {
                            /* 放置到装备区：替换武器/防具/攻击马/防御马 */
                            if(equip_count == 0) {
                                if(ap->equip.weapon) discard_add(&g->discard, ap->equip.weapon);
                                ap->equip.weapon = removed;
                            } else if(equip_count == 1) {
                                if(ap->equip.armor) discard_add(&g->discard, ap->equip.armor);
                                ap->equip.armor = removed;
                            }
                            equip_count++;
                            i--; /* 移除后下标前移 */
                        }
                    }
                }
                game_log(g, "【飞舞】%s（AI）自动放置%d张牌到装备区，亮牌结算", ap->name, equip_count);
                /* 直接进入亮牌结算 */
                game_feiwuu_finish_resolve(g);
            }
            else
            {
                /* 玩家：进入选牌状态 */
                game_start_feiwuu_pick(g);
                /* 注意：不在此调用 hero_skill_use，在 game_feiwuu_confirm 里调用 */
            }
        }
    }
}


/* ================================================================
 * 化蝶：飞舞进化版，置一张牌入装备区，亮(7-Y)张牌
 * ================================================================ */
void yudie_huadie(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return;
    if(!p->yudie.chengdie) return;  /* 未进化不能用化蝶 */

    /* 将一张手牌置入装备区（简化：替换武器） */
    if(p->hand_count > 0)
    {
        Card* c = player_remove_hand(p, 0);
        if(c)
        {
            if(p->equip.weapon) discard_add(&g->discard, p->equip.weapon);
            p->equip.weapon = c;
            game_log(g, "【化蝶】%s将【%s】置入装备区", p->name, card_get_full_name(c));
        }
    }

    /* Y = 装备区花色数 */
    int Y = yudie_get_equip_suit_count(p);
    int reveal_count = 7 - Y;
    if(reveal_count < 1) reveal_count = 1;
    if(reveal_count > g->deck.count) reveal_count = g->deck.count;

    Card* revealed[20];
    int reveal_idx = 0;
    for(int i = 0; i < reveal_count; i++)
    {
        Card* c = deck_draw(&g->deck);
        if(c) revealed[reveal_idx++] = c;
    }

    /* 屏幕中心展示亮出的牌（时间 = Ln(X)+0.5秒，X为牌数） */
    float dur_sec = logf((float)reveal_idx) + 0.5f;
    if(dur_sec < 1.0f) dur_sec = 1.0f;
    int dur_frames = (int)(dur_sec * 60);
    game_show_cards(g, revealed, reveal_idx, p->name, dur_frames);

    /* 获得其中与装备区花色相同的牌 */
    int got = 0;
    for(int i = 0; i < reveal_idx; i++)
    {
        if(yudie_card_matches_equip_suit(p, revealed[i]))
        {
            player_draw_card(p, revealed[i]);
            got++;
        }
        else
        {
            discard_add(&g->discard, revealed[i]);
        }
    }

    game_log(g, "【化蝶】亮出%d张牌（Y=%d），获得%d张同花色牌",
             reveal_count, Y, got);
}


/* ================================================================
 * 成蝶：检查使命技成功条件
 * ================================================================ */
int yudie_check_chengdie(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return 0;
    if(p->yudie.chengdie) return 0;

    /* 成功条件：飞舞累计获得≥10张牌 且 使用过≥10种不同牌名 */
    if(p->yudie.feiwuu_cards >= 10 && p->yudie.card_name_count >= 10)
    {
        yudie_evolve(g, player_idx);
        return 1;
    }
    return 0;
}


/* ================================================================
 * 成蝶：使命技成功，进化
 * ================================================================ */
void yudie_evolve(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return;

    p->yudie.chengdie = 1;
    p->max_hp += 1;
    p->hp += 1;
    if(p->hp > p->max_hp) p->hp = p->max_hp;

    /* 更新技能列表：失去破茧，飞舞变化蝶，获得化形、成形 */
    if(p->hero)
    {
        Hero* h = p->hero;
        h->skill_count = 3;

        /* 技能0：化蝶（主动技，出牌阶段限一次） */
        memset(&h->skills[0], 0, sizeof(Skill));
        strncpy(h->skills[0].name, "化蝶", 31);
        strncpy(h->skills[0].desc,
                "出牌阶段限一次，你可以将任意张牌置入装备区，然后亮出牌堆顶(6-X)张牌，获得其中与装备区花色相同的牌。",
                255);
        h->skills[0].type = SKILL_ACTIVE;
        h->skills[0].allowed_phases = HERO_PHASE_PLAY;
        h->skills[0].max_uses = 1;
        h->skills[0].used_count = 0;
        h->skills[0].enabled = 1;

        /* 技能1：化形（锁定技/被动） */
        memset(&h->skills[1], 0, sizeof(Skill));
        strncpy(h->skills[1].name, "化形", 31);
        strncpy(h->skills[1].desc,
                "结束阶段，你可以将一张手牌当任意一张基本牌或普通锦囊牌使用。",
                255);
        h->skills[1].type = SKILL_LOCKED;
        h->skills[1].allowed_phases = 0;
        h->skills[1].max_uses = -1;
        h->skills[1].enabled = 1;

        /* 技能2：成形（锁定技/被动） */
        memset(&h->skills[2], 0, sizeof(Skill));
        strncpy(h->skills[2].name, "成形", 31);
        strncpy(h->skills[2].desc,
                "你使用的第一张牌视为所有花色，每使用一张同花色牌摸一张牌（每回合限5次）。",
                255);
        h->skills[2].type = SKILL_LOCKED;
        h->skills[2].allowed_phases = 0;
        h->skills[2].max_uses = -1;
        h->skills[2].enabled = 1;
    }

    /* 屏幕中心提示使命成功 */
    game_show_message(g, "【成蝶】使命成功！雨蝶进化", 180);

    game_log(g, "【成蝶】使命达成！%s进化：失去【破茧】，【飞舞】→【化蝶】，获得【化形】【成形】，体力上限+1",
             p->name);
    game_log(g, "【成蝶】当前体力：%d/%d", p->hp, p->max_hp);
}


/* ================================================================
 * 成蝶：使命技失败（濒死时）
 * ================================================================ */
void yudie_on_dying(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return;
    if(p->yudie.chengdie) return;  /* 已进化后不触发失败 */

    /* 失败：失去破茧，重置飞舞使用次数 */
    p->yudie.break_suit = -1;
    p->yudie.feiwuu_count = 0;
    game_log(g, "【成蝶】使命失败！%s失去【破茧】，重置【飞舞】使用次数", p->name);
}


/* ================================================================
 * 化形：结束阶段转化牌（完整版）
 * ================================================================ */

/* 化形①：当前选择状态 */
static int huaxing_cur_suit = -1;
static int huaxing_cur_hand = -1;

/* 化形①：获取某花色可用锦囊牌名列表 */
static int yudie_huaxing_list_tricks(Player* p, int suit, char out[10][32])
{
    int cnt = 0;
    for(int i = 0; i < p->yudie.huaxing_record_count && cnt < 10; i++)
    {
        if(p->yudie.huaxing_record_suits[i] != suit) continue;
        int dup = 0;
        for(int j = 0; j < cnt; j++)
            if(strcmp(out[j], p->yudie.huaxing_record_names[i]) == 0) { dup = 1; break; }
        for(int j = 0; j < p->yudie.huaxing_played_name_count && !dup; j++)
            if(strcmp(p->yudie.huaxing_played_names[j], p->yudie.huaxing_record_names[i]) == 0) { dup = 1; break; }
        if(!dup) { strncpy(out[cnt], p->yudie.huaxing_record_names[i], 31); out[cnt][31]='\0'; cnt++; }
    }
    return cnt;
}

/* 化形①：某花色是否可用 */
static int yudie_huaxing_suit_ok(Player* p, int suit)
{
    int has_hand = 0;
    for(int i = 0; i < p->hand_count; i++)
        if(p->hand[i] && p->hand[i]->suit == suit) { has_hand = 1; break; }
    if(!has_hand) return 0;
    char tricks[10][32];
    return yudie_huaxing_list_tricks(p, suit, tricks) > 0;
}

/* 化形①：训练模式自动执行所有可用花色 */
static void yudie_huaxing_auto_execute(GameState* g, int idx)
{
    Player* p = &g->players[idx];
    extern int g_training_mode;
    if(!g_training_mode) return;

    game_log(g, "【化形】训练模式：自动执行化形①");
    for(int s = 0; s < 4; s++)
    {
        if(!yudie_huaxing_suit_ok(p, s)) continue;

        /* 找该花色第一张手牌 */
        int hand_idx = -1;
        for(int i = 0; i < p->hand_count; i++)
        {
            if(p->hand[i] && p->hand[i]->suit == s) { hand_idx = i; break; }
        }
        if(hand_idx < 0) continue;

        /* 找该花色第一个可用锦囊名 */
        char tricks[10][32];
        int cnt = yudie_huaxing_list_tricks(p, s, tricks);
        if(cnt == 0) continue;

        Card* hc = p->hand[hand_idx];
        game_log(g, "【化形】自动：花色%d，手牌【%s】→【%s】",
                 s, card_get_full_name(hc), tricks[0]);

        /* 记录已使用 */
        strncpy(p->yudie.huaxing_played_names[p->yudie.huaxing_played_name_count], tricks[0], 31);
        p->yudie.huaxing_played_names[p->yudie.huaxing_played_name_count][31] = '\0';
        p->yudie.huaxing_played_name_count++;
        p->yudie.huaxing_used_suits |= (1 << s);

        /* 移除手牌，使用转化牌真实效果 */
        Card* removed = player_remove_hand(p, hand_idx);
        if(removed)
        {
            yudie_huaxing_use_converted(g, idx, removed, tricks[0]);
            discard_add(&g->discard, removed);
        }
    }
    p->yudie.huaxing_free_suits = (~p->yudie.huaxing_used_suits) & 0xF;
    game_log(g, "【化形】训练模式自动执行完成，未选花色mask=%d", p->yudie.huaxing_free_suits);
}

/* 化形①：启动 */
static void yudie_huaxing_start(GameState* g, int idx)
{
    Player* p = &g->players[idx];
    p->yudie.huaxing_used_suits = 0;
    p->yudie.huaxing_played_name_count = 0;
    huaxing_cur_suit = -1;
    huaxing_cur_hand = -1;
    game_log(g, "【化形】%s结束阶段，按花色执行转化", p->name);

    /* 训练模式：自动执行 */
    extern int g_training_mode;
    if(g_training_mode)
    {
        yudie_huaxing_auto_execute(g, idx);
        return;
    }

    int ok = 0;
    for(int s = 0; s < 4; s++) if(yudie_huaxing_suit_ok(p, s)) { ok = 1; break; }
    if(!ok)
    {
        game_log(g, "【化形】无可用花色，跳过");
        p->yudie.huaxing_free_suits = 0xF;
        return;
    }
    g->resp_state = RESPONSE_NEED_HUAXING_SUIT;
}

/* 化形①：选花色 */
void yudie_huaxing_pick_suit(GameState* g, int idx, int suit)
{
    if(g->resp_state != RESPONSE_NEED_HUAXING_SUIT) return;
    Player* p = &g->players[idx];
    if(suit < 0 || suit >= 4) return;
    if(p->yudie.huaxing_used_suits & (1<<suit)) return;
    if(!yudie_huaxing_suit_ok(p, suit)) return;
    huaxing_cur_suit = suit;
    g->resp_state = RESPONSE_NEED_HUAXING_HAND;
    game_log(g, "【化形】选花色%d，请选该花色手牌", suit);
}

/* 化形①：选手牌 */
void yudie_huaxing_pick_hand(GameState* g, int idx, int hand_idx)
{
    if(g->resp_state != RESPONSE_NEED_HUAXING_HAND) return;
    Player* p = &g->players[idx];
    if(hand_idx < 0 || hand_idx >= p->hand_count) return;
    if(!p->hand[hand_idx] || p->hand[hand_idx]->suit != huaxing_cur_suit) return;
    huaxing_cur_hand = hand_idx;
    g->resp_state = RESPONSE_NEED_HUAXING_TRICK;
    game_log(g, "【化形】选手牌【%s】，请选锦囊名", card_get_full_name(p->hand[hand_idx]));
}

/* ================================================================
 * 化形①：根据牌名创建转化牌并使用（真实效果）
 * ================================================================ */
static void yudie_huaxing_use_converted(GameState* g, int idx, Card* src_card, const char* trick_name)
{
    Player* p = &g->players[idx];

    /* 创建转化牌 */
    Card* conv = (Card*)malloc(sizeof(Card));
    memset(conv, 0, sizeof(Card));
    conv->id = src_card->id;
    conv->is_valid = 1;
    conv->card_nature = CARD_NATURE_CONVERTED;
    conv->suit = src_card->suit;
    conv->rank = src_card->rank;
    conv->color = src_card->color;
    conv->type = CARD_TRICK;

    /* 根据牌名设置子类型 */
    int trick_type = -1;
    int need_target = 0;
    int is_group = 0;

    if(strcmp(trick_name, "决斗") == 0) { trick_type = TRICK_JUEDOU; need_target = 1; }
    else if(strcmp(trick_name, "万箭齐发") == 0) { trick_type = TRICK_WANJIAN; is_group = 1; }
    else if(strcmp(trick_name, "南蛮入侵") == 0) { trick_type = TRICK_NANMAN; is_group = 1; }
    else if(strcmp(trick_name, "桃园结义") == 0) { trick_type = TRICK_TAOYUAN; is_group = 1; }
    else if(strcmp(trick_name, "五谷丰登") == 0) { trick_type = TRICK_WUGU; is_group = 1; }
    else if(strcmp(trick_name, "顺手牵羊") == 0) { trick_type = TRICK_SHUNSHOU; need_target = 1; }
    else if(strcmp(trick_name, "无中生有") == 0) { trick_type = TRICK_WUZHONG; }
    else if(strcmp(trick_name, "过河拆桥") == 0) { trick_type = TRICK_GUOHE; need_target = 1; }
    else if(strcmp(trick_name, "火攻") == 0) { trick_type = TRICK_HUOGONG; need_target = 1; }
    else if(strcmp(trick_name, "铁索连环") == 0) { trick_type = TRICK_TIESUO; need_target = 1; }

    if(trick_type < 0)
    {
        game_log(g, "【化形】未知牌名【%s】，转化失败", trick_name);
        free(conv);
        return;
    }

    conv->sub.trick.trick_type = trick_type;

    game_log(g, "【化形】转化牌【%s】(花色%d点数%d)，类型=%d",
             trick_name, conv->suit, conv->rank, trick_type);

    /* 标记结束阶段，杀无次数限制（在game_use_card中检查） */
    int old_phase = g->phase;
    g->phase = PHASE_PLAY; /* 临时设为出牌阶段，让game_use_card正常工作 */

    if(is_group)
    {
        /* 群体锦囊：转化牌不在手牌中，直接执行简化版真实效果 */
        game_log(g, "【化形】群体锦囊【%s】生效", trick_name);

        if(trick_type == TRICK_WANJIAN)
        {
            /* 万箭齐发：所有其他角色需出闪，否则受1点伤害 */
            g->current_damage_source = DMG_SRC_WANJIAN;
            for(int i = 0; i < g->player_count; i++)
            {
                if(i == idx || !g->players[i].alive) continue;
                int has_shan = 0;
                for(int h = 0; h < g->players[i].hand_count; h++)
                {
                    if(g->players[i].hand[h] && g->players[i].hand[h]->type == CARD_BASIC &&
                       g->players[i].hand[h]->sub.basic.basic_type == BASIC_SHAN)
                    {
                        has_shan = 1;
                        Card* sh = player_remove_hand(&g->players[i], h);
                        if(sh) discard_add(&g->discard, sh);
                        game_log(g, "【万箭齐发】%s打出闪", g->players[i].name);
                        break;
                    }
                }
                if(!has_shan)
                {
                    game_log(g, "【万箭齐发】%s无闪，受到1点伤害", g->players[i].name);
                    game_deal_damage(g, i, 1, idx, DMG_NORMAL);
                }
                if(g->game_over) break;
            }
        }
        else if(trick_type == TRICK_NANMAN)
        {
            /* 南蛮入侵：所有其他角色需出杀，否则受1点伤害 */
            g->current_damage_source = DMG_SRC_NANMAN;
            for(int i = 0; i < g->player_count; i++)
            {
                if(i == idx || !g->players[i].alive) continue;
                int has_sha = 0;
                for(int h = 0; h < g->players[i].hand_count; h++)
                {
                    if(g->players[i].hand[h] && g->players[i].hand[h]->type == CARD_BASIC &&
                       g->players[i].hand[h]->sub.basic.basic_type == BASIC_SHA)
                    {
                        has_sha = 1;
                        Card* sh = player_remove_hand(&g->players[i], h);
                        if(sh) discard_add(&g->discard, sh);
                        game_log(g, "【南蛮入侵】%s打出杀", g->players[i].name);
                        break;
                    }
                }
                if(!has_sha)
                {
                    game_log(g, "【南蛮入侵】%s无杀，受到1点伤害", g->players[i].name);
                    game_deal_damage(g, i, 1, idx, DMG_NORMAL);
                }
                if(g->game_over) break;
            }
        }
        else if(trick_type == TRICK_TAOYUAN)
        {
            /* 桃园结义：所有存活角色回复1点体力 */
            for(int i = 0; i < g->player_count; i++)
            {
                if(!g->players[i].alive) continue;
                if(g->players[i].hp < g->players[i].max_hp)
                {
                    g->players[i].hp++;
                    game_log(g, "【桃园结义】%s回复1点体力（当前%d）", g->players[i].name, g->players[i].hp);
                }
            }
        }
        else if(trick_type == TRICK_WUGU)
        {
            /* 五谷丰登：简化为使用者摸2张牌 */
            for(int d = 0; d < 2 && g->deck.count > 0; d++)
            {
                Card* dc = deck_draw(&g->deck);
                if(dc) player_draw_card(p, dc);
            }
            game_log(g, "【五谷丰登】%s摸2张牌（简化）", p->name);
        }
    }
    else if(need_target)
    {
        /* 单体锦囊：目标默认为敌方 */
        int target = (idx == 0) ? 1 : 0;
        if(g->players[target].alive)
        {
            game_log(g, "【化形】对【%s】使用【%s】", g->players[target].name, trick_name);
            /* 简化：直接执行效果 */
            if(trick_type == TRICK_WUZHONG)
            {
                for(int d = 0; d < 2 && g->deck.count > 0; d++)
                {
                    Card* dc = deck_draw(&g->deck);
                    if(dc) player_draw_card(p, dc);
                }
                game_log(g, "【化形】无中生有：摸2张");
            }
            else if(trick_type == TRICK_GUOHE)
            {
                /* 过河拆桥：弃置对方一张牌 */
                Player* tgt = &g->players[target];
                if(tgt->hand_count > 0)
                {
                    Card* del = player_remove_hand(tgt, 0);
                    if(del) { discard_add(&g->discard, del); game_log(g, "【化形】过河拆桥：弃置对方一张手牌"); }
                }
                else if(tgt->equip.weapon) { Card* d=tgt->equip.weapon; tgt->equip.weapon=NULL; discard_add(&g->discard,d); game_log(g,"【化形】过河拆桥：弃置武器"); }
                else if(tgt->equip.armor) { Card* d=tgt->equip.armor; tgt->equip.armor=NULL; discard_add(&g->discard,d); game_log(g,"【化形】过河拆桥：弃置防具"); }
            }
            else if(trick_type == TRICK_JUEDOU)
            {
                game_log(g, "【化形】决斗：简化为对对方造成1点伤害");
                game_deal_damage(g, target, idx, 1, 0);
            }
            else if(trick_type == TRICK_HUOGONG)
            {
                game_log(g, "【化形】火攻：简化为对对方造成1点火焰伤害");
                game_deal_damage(g, target, idx, 1, 1); /* 1=火属性 */
            }
            else if(trick_type == TRICK_SHUNSHOU)
            {
                Player* tgt = &g->players[target];
                if(tgt->hand_count > 0)
                {
                    Card* st = player_remove_hand(tgt, 0);
                    if(st) { player_draw_card(p, st); game_log(g, "【化形】顺手牵羊：获得对方一张手牌"); }
                }
            }
            else if(trick_type == TRICK_TIESUO)
            {
                game_log(g, "【化形】铁索连环：简化为横置双方");
                g->players[idx].chained = !g->players[idx].chained;
                g->players[target].chained = !g->players[target].chained;
            }
        }
    }
    else /* 无中生有等无需目标 */
    {
        if(trick_type == TRICK_WUZHONG)
        {
            for(int d = 0; d < 2 && g->deck.count > 0; d++)
            {
                Card* dc = deck_draw(&g->deck);
                if(dc) player_draw_card(p, dc);
            }
            game_log(g, "【化形】无中生有：摸2张");
        }
    }

    g->phase = old_phase; /* 恢复阶段 */
    free(conv); /* 转化牌使用后释放 */
}

/* 化形①：选锦囊名并使用转化牌 */
void yudie_huaxing_pick_trick(GameState* g, int idx, int trick_idx)
{
    if(g->resp_state != RESPONSE_NEED_HUAXING_TRICK) return;
    Player* p = &g->players[idx];
    char tricks[10][32];
    int cnt = yudie_huaxing_list_tricks(p, huaxing_cur_suit, tricks);
    if(trick_idx < 0 || trick_idx >= cnt) return;
    if(huaxing_cur_hand < 0 || huaxing_cur_hand >= p->hand_count) return;

    Card* hc = p->hand[huaxing_cur_hand];
    game_log(g, "【化形】将【%s】当【%s】使用", card_get_full_name(hc), tricks[trick_idx]);

    /* 记录 */
    strncpy(p->yudie.huaxing_played_names[p->yudie.huaxing_played_name_count], tricks[trick_idx], 31);
    p->yudie.huaxing_played_names[p->yudie.huaxing_played_name_count][31] = '\0';
    p->yudie.huaxing_played_name_count++;
    p->yudie.huaxing_used_suits |= (1 << huaxing_cur_suit);

    /* 移除手牌，使用转化牌真实效果 */
    Card* src_card = p->hand[huaxing_cur_hand];
    if(src_card)
    {
        Card* removed = player_remove_hand(p, huaxing_cur_hand);
        if(removed)
        {
            yudie_huaxing_use_converted(g, idx, removed, tricks[trick_idx]);
            discard_add(&g->discard, removed); /* 原手牌进入弃牌堆 */
        }
    }

    huaxing_cur_suit = -1;
    huaxing_cur_hand = -1;

    /* 检查是否继续 */
    int more = 0;
    for(int s = 0; s < 4; s++)
        if(!(p->yudie.huaxing_used_suits & (1<<s)) && yudie_huaxing_suit_ok(p, s)) { more = 1; break; }
    if(more)
    {
        g->resp_state = RESPONSE_NEED_HUAXING_SUIT;
    }
    else
    {
        p->yudie.huaxing_free_suits = (~p->yudie.huaxing_used_suits) & 0xF;
        g->resp_state = RESPONSE_NONE;
        game_log(g, "【化形】结束，未选花色mask=%d", p->yudie.huaxing_free_suits);
    }
}

/* 化形①：结束 */
void yudie_huaxing_end(GameState* g, int idx)
{
    if(g->resp_state != RESPONSE_NEED_HUAXING_SUIT &&
       g->resp_state != RESPONSE_NEED_HUAXING_HAND &&
       g->resp_state != RESPONSE_NEED_HUAXING_TRICK) return;
    Player* p = &g->players[idx];
    p->yudie.huaxing_free_suits = (~p->yudie.huaxing_used_suits) & 0xF;
    g->resp_state = RESPONSE_NONE;
    game_log(g, "【化形】手动结束，未选花色mask=%d", p->yudie.huaxing_free_suits);
}

void yudie_on_phase_end(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return;

    /* 破茧：回合结束时重置 */
    if(!p->yudie.chengdie)
    {
        if(p->yudie.break_suit >= 0)
        {
            const char* sn[] = {"黑桃","红桃","梅花","方块"};
            game_log(g, "【破茧】%s回合结束，%s牌不再不计手牌上限", p->name, sn[p->yudie.break_suit]);
            p->yudie.break_suit = -1;
        }
        return;
    }

    /* 已进化：启动化形① */
    yudie_huaxing_start(g, player_idx);
}

/* ================================================================
 * 化形②：其他角色回合结束时触发
 * 未选择的花色：弃置同花色手牌，视为使用虚拟牌
 * ================================================================ */
void yudie_huaxing_phase2_on_turn_end(GameState* g, int yudie_idx, int turn_idx)
{
    if(!g || yudie_idx < 0 || yudie_idx >= g->player_count) return;
    if(turn_idx < 0 || turn_idx >= g->player_count) return;
    Player* p = &g->players[yudie_idx];
    if(p->hero_id != HERO_YUDIE || !p->yudie.chengdie) return;
    if(p->yudie.huaxing_target != turn_idx) return;
    if(p->yudie.huaxing_free_suits == 0) return;
    if(p->yudie.huaxing_response_used[turn_idx] > 0) return; /* 每回合限一次 */
    if(!p->alive) return;

    const char* suit_names[] = {"黑桃", "红桃", "梅花", "方块"};
    game_log(g, "【化形②】%s回合结束，检查化形②触发", g->players[turn_idx].name);

    /* 遍历未选择的花色，且目标角色本回合使用过该花色 */
    for(int s = 0; s < 4; s++)
    {
        if(!(p->yudie.huaxing_free_suits & (1 << s))) continue;
        /* 关键：目标角色必须使用过该花色的牌 */
        if(!(p->yudie.huaxing_target_used_suits & (1 << s))) continue;

        /* 找一张该花色手牌 */
        int hand_idx = -1;
        for(int i = 0; i < p->hand_count; i++)
        {
            if(p->hand[i] && p->hand[i]->suit == s)
            {
                hand_idx = i;
                break;
            }
        }
        if(hand_idx < 0) continue;

        /* 获取目标角色使用过的该花色牌名 */
        const char* target_card_name = p->yudie.huaxing_target_used_names[s];
        if(strlen(target_card_name) == 0) continue;

        /* 弃置该手牌 */
        Card* discarded = player_remove_hand(p, hand_idx);
        if(!discarded) continue;

        game_log(g, "【化形②】%s弃置【%s】(%s)，视为使用【%s】",
                 p->name, card_get_full_name(discarded), suit_names[s], target_card_name);

        discard_add(&g->discard, discarded);

        /* 创建虚拟牌：与目标使用过的牌同牌名 */
        Card* vc = (Card*)malloc(sizeof(Card));
        memset(vc, 0, sizeof(Card));
        vc->id = 300000 + rand() % 10000;
        vc->is_valid = 1;
        vc->card_nature = CARD_NATURE_VIRTUAL;
        vc->suit = s;
        vc->rank = 0;  /* 虚拟牌无点数 */
        vc->color = (s == 1 || s == 3) ? COLOR_RED : COLOR_BLACK;

        /* 根据牌名设置虚拟牌类型 */
        int is_basic = 0;
        if(strcmp(target_card_name, "杀") == 0)
        {
            vc->type = CARD_BASIC;
            vc->sub.basic.basic_type = BASIC_SHA;
            is_basic = 1;
        }
        else if(strcmp(target_card_name, "闪") == 0)
        {
            vc->type = CARD_BASIC;
            vc->sub.basic.basic_type = BASIC_SHAN;
            is_basic = 1;
        }
        else if(strcmp(target_card_name, "桃") == 0)
        {
            vc->type = CARD_BASIC;
            vc->sub.basic.basic_type = BASIC_TAO;
            is_basic = 1;
        }
        else if(strcmp(target_card_name, "酒") == 0)
        {
            vc->type = CARD_BASIC;
            vc->sub.basic.basic_type = BASIC_JIU;
            is_basic = 1;
        }
        else
        {
            /* 锦囊牌 */
            vc->type = CARD_TRICK;
            vc->sub.trick.trick_type = TRICK_JUEDOU;  /* 默认，后面按牌名处理 */
        }

        /* 对当前回合角色使用虚拟牌 */
        int target = turn_idx;
        if(is_basic && vc->sub.basic.basic_type == BASIC_SHA)
        {
            if(g->players[target].alive && target != yudie_idx)
            {
                game_log(g, "【化形②】对【%s】使用虚拟杀", g->players[target].name);
                game_deal_damage(g, target, yudie_idx, 1, DMG_NORMAL);
            }
        }
        else if(is_basic && vc->sub.basic.basic_type == BASIC_TAO)
        {
            /* 虚拟桃：回复自己1点体力 */
            if(p->hp < p->max_hp)
            {
                p->hp++;
                game_log(g, "【化形②】使用虚拟桃，回复1点体力");
            }
        }
        else if(is_basic && vc->sub.basic.basic_type == BASIC_SHAN)
        {
            /* 虚拟闪：结束阶段无实际效果，记录日志 */
            game_log(g, "【化形②】使用虚拟闪（结束阶段无效果）");
        }
        else if(is_basic && vc->sub.basic.basic_type == BASIC_JIU)
        {
            /* 虚拟酒：结束阶段无实际效果，记录日志 */
            game_log(g, "【化形②】使用虚拟酒（结束阶段无效果）");
        }
        else if(!is_basic)
        {
            /* 虚拟锦囊：根据牌名执行真实效果 */
            Player* tgt = &g->players[target];
            if(strcmp(target_card_name, "决斗") == 0)
            {
                /* 决斗：对方需出杀，否则受1点伤害 */
                game_log(g, "【化形②】对【%s】使用虚拟决斗", tgt->name);
                int has_sha = 0;
                for(int h = 0; h < tgt->hand_count; h++)
                {
                    if(tgt->hand[h] && tgt->hand[h]->type == CARD_BASIC &&
                       tgt->hand[h]->sub.basic.basic_type == BASIC_SHA)
                    {
                        has_sha = 1;
                        Card* sh = player_remove_hand(tgt, h);
                        if(sh) discard_add(&g->discard, sh);
                        game_log(g, "【决斗】%s打出杀", tgt->name);
                        break;
                    }
                }
                if(!has_sha && tgt->alive)
                {
                    game_log(g, "【决斗】%s无杀，受到1点伤害", tgt->name);
                    game_deal_damage(g, target, yudie_idx, 1, DMG_NORMAL);
                }
            }
            else if(strcmp(target_card_name, "过河拆桥") == 0)
            {
                /* 过河拆桥：弃置对方一张牌 */
                game_log(g, "【化形②】对【%s】使用虚拟过河拆桥", tgt->name);
                if(tgt->hand_count > 0)
                {
                    Card* del = player_remove_hand(tgt, 0);
                    if(del) { discard_add(&g->discard, del); game_log(g, "【过河拆桥】弃置对方一张手牌"); }
                }
                else if(tgt->equip.weapon) { Card* d=tgt->equip.weapon; tgt->equip.weapon=NULL; discard_add(&g->discard,d); game_log(g,"【过河拆桥】弃置武器"); }
                else if(tgt->equip.armor) { Card* d=tgt->equip.armor; tgt->equip.armor=NULL; discard_add(&g->discard,d); game_log(g,"【过河拆桥】弃置防具"); }
            }
            else if(strcmp(target_card_name, "顺手牵羊") == 0)
            {
                /* 顺手牵羊：获得对方一张手牌 */
                game_log(g, "【化形②】对【%s】使用虚拟顺手牵羊", tgt->name);
                if(tgt->hand_count > 0)
                {
                    Card* st = player_remove_hand(tgt, 0);
                    if(st) { player_draw_card(p, st); game_log(g, "【顺手牵羊】获得对方一张手牌"); }
                }
            }
            else if(strcmp(target_card_name, "火攻") == 0)
            {
                /* 火攻：造成1点火焰伤害 */
                game_log(g, "【化形②】对【%s】使用虚拟火攻", tgt->name);
                if(tgt->alive)
                    game_deal_damage(g, target, yudie_idx, 1, DMG_FIRE);
            }
            else if(strcmp(target_card_name, "铁索连环") == 0)
            {
                /* 铁索连环：横置双方 */
                game_log(g, "【化形②】使用虚拟铁索连环");
                p->chained = !p->chained;
                if(tgt->alive) tgt->chained = !tgt->chained;
                game_log(g, "【铁索连环】%s横置=%d, %s横置=%d", p->name, p->chained, tgt->name, tgt->chained);
            }
            else if(strcmp(target_card_name, "无中生有") == 0)
            {
                /* 无中生有：摸2张牌 */
                game_log(g, "【化形②】使用虚拟无中生有");
                for(int d = 0; d < 2 && g->deck.count > 0; d++)
                {
                    Card* dc = deck_draw(&g->deck);
                    if(dc) player_draw_card(p, dc);
                }
                game_log(g, "【无中生有】摸2张牌");
            }
            else if(strcmp(target_card_name, "万箭齐发") == 0)
            {
                /* 万箭齐发：所有其他角色需出闪，否则受1点伤害 */
                game_log(g, "【化形②】使用虚拟万箭齐发");
                g->current_damage_source = DMG_SRC_WANJIAN;
                for(int i = 0; i < g->player_count; i++)
                {
                    if(i == yudie_idx || !g->players[i].alive) continue;
                    int has_shan = 0;
                    for(int h = 0; h < g->players[i].hand_count; h++)
                    {
                        if(g->players[i].hand[h] && g->players[i].hand[h]->type == CARD_BASIC &&
                           g->players[i].hand[h]->sub.basic.basic_type == BASIC_SHAN)
                        {
                            has_shan = 1;
                            Card* sh = player_remove_hand(&g->players[i], h);
                            if(sh) discard_add(&g->discard, sh);
                            game_log(g, "【万箭齐发】%s打出闪", g->players[i].name);
                            break;
                        }
                    }
                    if(!has_shan)
                    {
                        game_log(g, "【万箭齐发】%s无闪，受到1点伤害", g->players[i].name);
                        game_deal_damage(g, i, 1, yudie_idx, DMG_NORMAL);
                    }
                    if(g->game_over) break;
                }
            }
            else if(strcmp(target_card_name, "南蛮入侵") == 0)
            {
                /* 南蛮入侵：所有其他角色需出杀，否则受1点伤害 */
                game_log(g, "【化形②】使用虚拟南蛮入侵");
                g->current_damage_source = DMG_SRC_NANMAN;
                for(int i = 0; i < g->player_count; i++)
                {
                    if(i == yudie_idx || !g->players[i].alive) continue;
                    int has_sha = 0;
                    for(int h = 0; h < g->players[i].hand_count; h++)
                    {
                        if(g->players[i].hand[h] && g->players[i].hand[h]->type == CARD_BASIC &&
                           g->players[i].hand[h]->sub.basic.basic_type == BASIC_SHA)
                        {
                            has_sha = 1;
                            Card* sh = player_remove_hand(&g->players[i], h);
                            if(sh) discard_add(&g->discard, sh);
                            game_log(g, "【南蛮入侵】%s打出杀", g->players[i].name);
                            break;
                        }
                    }
                    if(!has_sha)
                    {
                        game_log(g, "【南蛮入侵】%s无杀，受到1点伤害", g->players[i].name);
                        game_deal_damage(g, i, 1, yudie_idx, DMG_NORMAL);
                    }
                    if(g->game_over) break;
                }
            }
            else if(strcmp(target_card_name, "桃园结义") == 0)
            {
                /* 桃园结义：所有存活角色回复1点体力 */
                game_log(g, "【化形②】使用虚拟桃园结义");
                for(int i = 0; i < g->player_count; i++)
                {
                    if(g->players[i].alive && g->players[i].hp < g->players[i].max_hp)
                    {
                        g->players[i].hp++;
                        game_log(g, "【桃园结义】%s回复1点体力", g->players[i].name);
                    }
                }
            }
            else if(strcmp(target_card_name, "五谷丰登") == 0)
            {
                /* 五谷丰登：简化为自己摸2张牌 */
                game_log(g, "【化形②】使用虚拟五谷丰登");
                for(int d = 0; d < 2 && g->deck.count > 0; d++)
                {
                    Card* dc = deck_draw(&g->deck);
                    if(dc) player_draw_card(p, dc);
                }
                game_log(g, "【五谷丰登】摸2张牌（简化）");
            }
            else
            {
                /* 未知锦囊：记录日志 */
                game_log(g, "【化形②】未知虚拟锦囊【%s】，无效果", target_card_name);
            }
        }

        free(vc);
        p->yudie.huaxing_response_used[turn_idx]++;
        break; /* 每回合只触发一次 */
    }
}

/* ================================================================
 * 化形②：重置每回合使用次数（回合开始时调用）
 * ================================================================ */
void yudie_huaxing_phase2_reset(GameState* g, int yudie_idx)
{
    if(!g || yudie_idx < 0 || yudie_idx >= g->player_count) return;
    Player* p = &g->players[yudie_idx];
    if(p->hero_id != HERO_YUDIE) return;
    for(int i = 0; i < 2; i++)
        p->yudie.huaxing_response_used[i] = 0;
}

/* ================================================================
 * 化形②：设置目标角色
 * ================================================================ */
void yudie_huaxing_set_target(GameState* g, int yudie_idx, int target_idx)
{
    if(!g || yudie_idx < 0 || yudie_idx >= g->player_count) return;
    if(target_idx < 0 || target_idx >= g->player_count) return;
    Player* p = &g->players[yudie_idx];
    if(p->hero_id != HERO_YUDIE || !p->yudie.chengdie) return;

    p->yudie.huaxing_target = target_idx;
    game_log(g, "【化形②】%s指定目标为【%s】", p->name, g->players[target_idx].name);

    /* 完成化形②目标指定，清除响应状态 */
    g->resp_state = RESPONSE_NONE;
}

/* ================================================================
 * 辅助函数：判断一张牌是否与装备区某张牌同花色
 * ================================================================ */
int yudie_card_matches_equip_suit(Player* p, Card* card)
{
    if(!p || !card) return 0;
    if(p->equip.weapon && p->equip.weapon->suit == card->suit) return 1;
    if(p->equip.armor && p->equip.armor->suit == card->suit) return 1;
    if(p->equip.horse_atk && p->equip.horse_atk->suit == card->suit) return 1;
    if(p->equip.horse_def && p->equip.horse_def->suit == card->suit) return 1;
    return 0;
}


/* ================================================================
 * 辅助函数：装备区花色数
 * ================================================================ */
int yudie_get_equip_suit_count(const Player* p)
{
    if(!p) return 0;
    int suits[4] = {0, 0, 0, 0};
    if(p->equip.weapon) suits[p->equip.weapon->suit] = 1;
    if(p->equip.armor) suits[p->equip.armor->suit] = 1;
    if(p->equip.horse_atk) suits[p->equip.horse_atk->suit] = 1;
    if(p->equip.horse_def) suits[p->equip.horse_def->suit] = 1;
    int count = 0;
    for(int i = 0; i < 4; i++) count += suits[i];
    return count;
}


/* ================================================================
 * 辅助函数：记录使用过的牌名
 * ================================================================ */
void yudie_record_card_name(GameState* g, int player_idx, const char* name)
{
    if(!g || !name || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return;

    /* 检查是否已记录过 */
    for(int i = 0; i < p->yudie.card_name_count; i++)
    {
        if(strcmp(p->yudie.card_names[i], name) == 0) return;
    }

    /* 记录新牌名 */
    if(p->yudie.card_name_count < 20)
    {
        strncpy(p->yudie.card_names[p->yudie.card_name_count], name, 31);
        p->yudie.card_names[p->yudie.card_name_count][31] = '\0';
        p->yudie.card_name_count++;
    }
}


/* ================================================================
 * 辅助函数：成形 - 本回合第一张牌视为包含所有花色
 * ================================================================ */
int yudie_is_all_suit(const Player* p, Card* card)
{
    if(!p || !card) return 0;
    if(p->hero_id != HERO_YUDIE) return 0;
    if(!p->yudie.chengdie) return 0;
    return (!p->yudie.first_card_played);  /* 第一张牌全花色 */
}


/* ================================================================
 * 辅助函数：获取使用过的不同牌名数
 * ================================================================ */
int yudie_get_card_name_count(const Player* p)
{
    if(!p) return 0;
    return p->yudie.card_name_count;
}


/* ================================================================
 * 辅助函数：飞舞累计获得牌数
 * ================================================================ */
int yudie_get_feiwuu_cards(const Player* p)
{
    if(!p) return 0;
    return p->yudie.feiwuu_cards;
}


/* ================================================================
 * AI自动使用技能（飞舞/化蝶）
 * 策略：有手牌时就使用飞舞/化蝶获得牌
 * ================================================================ */
int yudie_ai_use_skill(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return 0;
    if(!p->alive) return 0;
    if(g->phase != PHASE_PLAY || g->current_player != player_idx) return 0;
    if(g->resp_state != RESPONSE_NONE) return 0;

    /* 检查是否可用（出牌阶段限一次） */
    if(p->skill_used[1] >= 1) return 0;

    /* 有手牌时才使用（至少1张） */
    if(p->hand_count < 1) return 0;

    /* 直接调用飞舞/化蝶的简化实现 */
    if(p->yudie.chengdie)
    {
        /* 已进化：化蝶（将1张牌置入装备区，亮出7-Y张） */
        yudie_feiwuu(g, player_idx);  /* 复用飞舞逻辑，化蝶内部会处理 */
    }
    else
    {
        /* 未进化：飞舞 */
        yudie_feiwuu(g, player_idx);
    }

    return 1;
}
