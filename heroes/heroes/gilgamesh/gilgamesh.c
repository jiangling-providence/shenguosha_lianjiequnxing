#include <stdio.h>
#include <string.h>
#include "gilgamesh.h"
#include "../../game.h"
#include "../../player.h"

/* ===== 注册角色信息 ===== */
void gilgamesh_register(Hero* h)
{
    if (!h) return;
    memset(h, 0, sizeof(Hero));

    h->id = HERO_GILGAMESH;
    strncpy(h->name, "gilgamesh", 31);
    h->max_hp = 4;
    h->skill_count = 3;

    /* 一技能：所见（主动技，限3次） */
    strncpy(h->skills[0].name, "所见", 31);
    strncpy(h->skills[0].desc,
            "出牌阶段限3次，你可以从牌堆中检索一张指定类型的牌。",
            255);
    h->skills[0].type = SKILL_ACTIVE;
    h->skills[0].allowed_phases = HERO_PHASE_PLAY;
    h->skills[0].max_uses = 3;
    h->skills[0].used_count = 0;
    h->skills[0].active = 0;

    /* 二技能：乖离（主动技，限4次） */
    strncpy(h->skills[1].name, "乖离", 31);
    strncpy(h->skills[1].desc,
            "出牌阶段限4次，你可以弃置手牌中某一种花色的牌，并摸等量的牌。",
            255);
    h->skills[1].type = SKILL_ACTIVE;
    h->skills[1].allowed_phases = HERO_PHASE_PLAY;
    h->skills[1].max_uses = 4;
    h->skills[1].used_count = 0;
    h->skills[1].active = 0;

    /* 三技能：天辟（主动技，限1次） */
    strncpy(h->skills[2].name, "天辟", 31);
    strncpy(h->skills[2].desc,
            "出牌阶段限1次，你可以弃置手牌中某一种点数的牌，对一个角色造成X点伤害，X为弃置牌的数量。",
            255);
    h->skills[2].type = SKILL_ACTIVE;
    h->skills[2].allowed_phases = HERO_PHASE_PLAY;
    h->skills[2].max_uses = 1;
    h->skills[2].used_count = 0;
    h->skills[2].active = 0;

    /* 设置技能使用函数 */
    h->use_skill = gilgamesh_use_skill;
    h->ai_use_skill = gilgamesh_ai_use_skill;
}

/* ===== 一技能：所见 ===== */
Card* gilgamesh_suojian(GameState* g, int player_idx,
                         CardType type, int sub_type)
{
    if (!g) return NULL;
    Hero* h = hero_get(HERO_GILGAMESH);
    if (!h || h->skills[0].used_count >= h->skills[0].max_uses)
        return NULL;

    Player* p = &g->players[player_idx];

    /* 调用通用函数：从牌堆中随机获取一张指定大类的牌 */
    Card* c = game_draw_random_card_by_type(g, player_idx, type);
    if (c) {
        h->skills[0].used_count++;
        game_log(g, "%s 发动【所见】，随机检索到【%s】",
                 p->name, card_get_full_name(c));
    }
    return c;
}

/* ===== 二技能：乖离 ===== */
int gilgamesh_guaili(GameState* g, int player_idx, Suit suit)
{
    if (!g) return 0;
    Hero* h = hero_get(HERO_GILGAMESH);
    if (!h || h->skills[1].used_count >= h->skills[1].max_uses)
        return 0;

    Player* p = &g->players[player_idx];
    int discarded = 0;

    /* 从后往前弃置所有指定花色的手牌 */
    for (int i = p->hand_count - 1; i >= 0; i--) {
        if (p->hand[i] && p->hand[i]->suit == suit) {
            Card* c = player_remove_hand(p, i);
            discard_add(&g->discard, c);
            discarded++;
        }
    }

    if (discarded > 0) {
        game_draw_cards(g, player_idx, discarded);
        h->skills[1].used_count++;
        game_log(g, "%s 发动【乖离】，弃置%d张牌，摸%d张牌",
                 p->name, discarded, discarded);
    }
    return discarded;
}

/* ===== 三技能：天辟 ===== */
int gilgamesh_tianpi(GameState* g, int player_idx, int target_idx, Rank rank)
{
    if (!g) return 0;
    Hero* h = hero_get(HERO_GILGAMESH);
    if (!h || h->skills[2].used_count >= h->skills[2].max_uses)
        return 0;
    if (target_idx < 0 || target_idx >= g->player_count) return 0;
    if (!g->players[target_idx].alive) return 0;

    Player* p = &g->players[player_idx];
    int discarded = 0;

    /* 弃置所有指定点数的手牌 */
    for (int i = p->hand_count - 1; i >= 0; i--) {
        if (p->hand[i] && p->hand[i]->rank == rank) {
            Card* c = player_remove_hand(p, i);
            discard_add(&g->discard, c);
            discarded++;
        }
    }

    if (discarded > 0) {
        game_deal_damage(g, target_idx, discarded, player_idx, DMG_NORMAL);
        h->skills[2].used_count++;
        game_log(g, "%s 发动【天辟】，弃置%d张牌，对%s造成%d点伤害",
                 p->name, discarded,
                 g->players[target_idx].name, discarded);
    }
    return discarded;
}


/* ================================================================
 * 技能使用分发函数（进入交互式选择状态）
 * ================================================================ */
void gilgamesh_use_skill(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_GILGAMESH) return;

    Hero* h = hero_get(HERO_GILGAMESH);
    if(!h) return;

    /* 检查使用次数 */
    if(skill_idx < 0 || skill_idx >= h->skill_count) return;
    if(h->skills[skill_idx].used_count >= h->skills[skill_idx].max_uses)
    {
        game_log(g, "【%s】使用次数已用完", h->skills[skill_idx].name);
        return;
    }

    /* AI发动：直接执行效果，不进入玩家交互状态 */
    if(p->is_ai)
    {
        int enemy_idx = (player_idx == 0) ? 1 : 0;
        switch(skill_idx)
        {
            case 0: /* 所见：AI随机选基本牌 */
                gilgamesh_suojian(g, player_idx, CARD_BASIC, 0);
                break;
            case 1: /* 乖离：AI选闪最多的花色 */
            {
                int suit_count[4] = {0};
                for(int i = 0; i < p->hand_count; i++)
                    if(p->hand[i] && p->hand[i]->suit >= 0 && p->hand[i]->suit < 4)
                        suit_count[p->hand[i]->suit]++;
                int best_suit = 0, best_cnt = 0;
                for(int s = 0; s < 4; s++)
                    if(suit_count[s] > best_cnt) { best_cnt = suit_count[s]; best_suit = s; }
                gilgamesh_guaili(g, player_idx, (Suit)best_suit);
                break;
            }
            case 2: /* 天辟：AI选相同点数最多的 */
            {
                int rank_count[16] = {0};
                for(int i = 0; i < p->hand_count; i++)
                    if(p->hand[i] && p->hand[i]->rank >= 0 && p->hand[i]->rank < 16)
                        rank_count[p->hand[i]->rank]++;
                int best_rank = 0, best_cnt = 0;
                for(int r = 0; r < 16; r++)
                    if(rank_count[r] > best_cnt) { best_cnt = rank_count[r]; best_rank = r; }
                if(best_cnt >= 1)
                    gilgamesh_tianpi(g, player_idx, enemy_idx, (Rank)best_rank);
                break;
            }
        }
        return;
    }

    switch(skill_idx)
    {
        case 0: /* 所见：进入选择牌类型状态 */
            game_gilgamesh_start_suojian(g);
            break;
        case 1: /* 乖离：进入选择花色状态 */
            game_gilgamesh_start_guaili(g);
            break;
        case 2: /* 天辟：进入选择目标状态 */
            game_gilgamesh_start_tianpi_target(g);
            break;
    }
}


/* ================================================================
 * AI自动使用技能
 * 策略：天辟（相同点数>=2）> 乖离（弃闪）> 所见（缺牌）
 * ================================================================ */
int gilgamesh_ai_use_skill(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_GILGAMESH) return 0;
    if(!p->alive) return 0;
    if(g->phase != PHASE_PLAY || g->current_player != player_idx) return 0;
    if(g->resp_state != RESPONSE_NONE) return 0;

    Hero* h = hero_get(HERO_GILGAMESH);
    if(!h) return 0;

    int enemy_idx = (player_idx == 0) ? 1 : 0;
    Player* enemy = &g->players[enemy_idx];
    if(!enemy->alive) return 0;

    /* 天辟：找相同点数最多的牌，>=2张就用 */
    if(h->skills[2].used_count < h->skills[2].max_uses)
    {
        int rank_count[16] = {0};
        for(int i = 0; i < p->hand_count; i++)
        {
            if(p->hand[i] && p->hand[i]->rank >= 0 && p->hand[i]->rank < 16)
                rank_count[p->hand[i]->rank]++;
        }
        int best_rank = -1, best_count = 0;
        for(int r = 0; r < 16; r++)
        {
            if(rank_count[r] > best_count)
            {
                best_count = rank_count[r];
                best_rank = r;
            }
        }
        if(best_count >= 2)
        {
            gilgamesh_tianpi(g, player_idx, enemy_idx, best_rank);
            return 1;
        }
    }

    /* 乖离：弃置所有闪，摸等量的牌 */
    if(h->skills[1].used_count < h->skills[1].max_uses)
    {
        int shan_count = 0;
        for(int i = 0; i < p->hand_count; i++)
        {
            if(p->hand[i] && p->hand[i]->type == CARD_BASIC &&
               p->hand[i]->sub.basic.basic_type == BASIC_SHAN)
                shan_count++;
        }
        if(shan_count >= 1)
        {
            /* 找闪的花色 */
            Suit shan_suit = SUIT_HEART;
            for(int i = 0; i < p->hand_count; i++)
            {
                if(p->hand[i] && p->hand[i]->type == CARD_BASIC &&
                   p->hand[i]->sub.basic.basic_type == BASIC_SHAN)
                {
                    shan_suit = p->hand[i]->suit;
                    break;
                }
            }
            gilgamesh_guaili(g, player_idx, shan_suit);
            return 1;
        }
    }

    /* 所见：手牌少时检索一张基本牌 */
    if(h->skills[0].used_count < h->skills[0].max_uses && p->hand_count <= 2)
    {
        gilgamesh_suojian(g, player_idx, CARD_BASIC, 0);
        return 1;
    }

    return 0;
}
