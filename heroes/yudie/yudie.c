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
 * 化形：结束阶段转化牌（简化实现）
 * 破茧：回合结束时重置展示花色（本回合有效）
 * ================================================================ */
void yudie_on_phase_end(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_YUDIE) return;

    /* 破茧：回合结束时重置展示花色（本回合有效） */
    if(!p->yudie.chengdie)
    {
        if(p->yudie.break_suit >= 0)
        {
            const char* suit_names[] = {"黑桃", "红桃", "梅花", "方块"};
            int suit_idx = p->yudie.break_suit;
            if(suit_idx >= 0 && suit_idx < 4)
                game_log(g, "【破茧】%s回合结束，%s牌不再不计手牌上限", p->name, suit_names[suit_idx]);
            p->yudie.break_suit = -1;
        }
        return;
    }

    /* 已进化：化形 */
    /* 简化：结束阶段摸一张牌（化形的简化版本） */
    if(g->deck.count > 0)
    {
        Card* c = deck_draw(&g->deck);
        if(c)
        {
            player_draw_card(p, c);
            game_log(g, "【化形】%s结束阶段发动，摸一张牌", p->name);
        }
    }
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
