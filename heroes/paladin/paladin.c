#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "paladin.h"
#include "../../game.h"
#include "../../player.h"


/* ===== 注册角色信息 ===== */
void paladin_register(Hero* h)
{
    if (!h) return;
    memset(h, 0, sizeof(Hero));
    h->id = HERO_PALADIN;
    strncpy(h->name, "paladin", 31);
    h->max_hp = 1;
    h->max_shield = 5;
    h->initial_shield = 3;  /* 初始盾量为3 */

    h->skill_count = 2;

    /* 一技能：神圣护盾（锁定技） */
    strncpy(h->skills[0].name, "神圣护盾", 31);
    strncpy(h->skills[0].desc,
            "锁定技，每名角色回合开始时，其选择一项：1.流失1体力使你+2盾；2.弃1牌使你+1盾；3.弃1牌使你-1盾；4.受1伤害使你-2盾。护盾变化你摸X+1牌，有盾时不伤血不濒死。",
            255);
    h->skills[0].type = SKILL_LOCKED;
    h->skills[0].allowed_phases = 0;
    h->skills[0].max_uses = -1;
    h->skills[0].used_count = 0;
    h->skills[0].active = 0;

    /* 二技能：破灭护盾（主动技，每回合限1次） */
    strncpy(h->skills[1].name, "破灭护盾", 31);
    strncpy(h->skills[1].desc,
            "当你需要响应/打出非装备/非延时锦囊牌时，可立刻令当前回合进行神圣护盾选择；出牌阶段主动发动可选择牌名打出虚拟牌。每回合限1次。",
            255);
    h->skills[1].type = SKILL_ACTIVE;
    h->skills[1].allowed_phases = HERO_PHASE_PLAY;
    h->skills[1].max_uses = 1;
    h->skills[1].used_count = 0;
    h->skills[1].active = 0;

    /* 注册回调函数指针 */
    h->on_any_turn_start = paladin_on_any_turn_start;
    h->use_skill = paladin_use_skill;
    h->ai_use_skill = paladin_ai_use_skill;
}

/* ================================================================
 * 圣骑士技能统一入口（use_skill 回调）
 * ================================================================ */
void paladin_use_skill(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_PALADIN) return;

    if(skill_idx == 1)
    {
        /* 破灭护盾 */
        paladin_use_pomie(g, player_idx);
    }
    /* skill_idx == 0 是神圣护盾（锁定技），不需要主动发动 */
}


/* ================================================================
 * 盾数变化：摸X+1张牌（X为变化绝对值）
 * ================================================================ */
void paladin_shield_changed(GameState* g, int paladin_idx, int delta)
{
    if(!g || paladin_idx < 0 || paladin_idx >= g->player_count) return;
    Player* p = &g->players[paladin_idx];
    if(p->hero_id != HERO_PALADIN) return;
    if(delta == 0) return;

    int x = delta > 0 ? delta : -delta;
    int draw_num = x + 1;
    game_draw_cards(g, paladin_idx, draw_num);
    game_log(g, "【神圣护盾】%s护盾变化%d，摸%d张牌", p->name, delta, draw_num);
}


/* ================================================================
 * 受到伤害时：有盾则优先扣盾，不扣体力不濒死
 * 返回实际扣的盾数
 * ================================================================ */
int paladin_take_damage(GameState* g, int paladin_idx, int amount)
{
    if(!g || paladin_idx < 0 || paladin_idx >= g->player_count) return 0;
    Player* p = &g->players[paladin_idx];
    if(p->hero_id != HERO_PALADIN) return 0;
    if(amount <= 0) return 0;

    if(p->shield > 0)
    {
        int shield_damage = (amount < p->shield) ? amount : p->shield;
        int old_shield = p->shield;
        p->shield -= shield_damage;
        if(p->shield < 0) p->shield = 0;

        game_log(g, "【神圣护盾】%s有盾，伤害优先扣盾（%d→%d），不扣体力",
                 p->name, old_shield, p->shield);

        /* 盾数变化摸牌 */
        int delta = p->shield - old_shield;
        paladin_shield_changed(g, paladin_idx, delta);

        /* 剩余伤害（如果盾不够） */
        int remain = amount - shield_damage;
        if(remain > 0)
        {
            game_log(g, "【神圣护盾】%s盾已耗尽，剩余%d点伤害扣体力", p->name, remain);
            p->hp -= remain;
            p->damage_taken_count++;
            if(p->hp <= 0) p->hp = 0;
        }
        return shield_damage;
    }
    return 0;
}


/* ================================================================
 * 神圣护盾：执行选项效果
 * option: 1=流失1体力+2盾, 2=弃1牌+1盾, 3=弃1牌-1盾, 4=受1伤害-2盾
 * ================================================================ */
static void paladin_execute_option(GameState* g, int paladin_idx,
                                    int turn_player_idx, int option)
{
    if(!g || paladin_idx < 0 || paladin_idx >= g->player_count) return;
    if(turn_player_idx < 0 || turn_player_idx >= g->player_count) return;
    Player* paladin = &g->players[paladin_idx];
    Player* turn_p = &g->players[turn_player_idx];
    if(paladin->hero_id != HERO_PALADIN) return;

    int old_shield = paladin->shield;
    int skip_shield_change = 0;  /* 选项4：伤害已自动摸牌，跳过末尾重复摸牌 */

    switch(option)
    {
    case 1: /* 流失1体力，圣骑士+2盾 */
        if(turn_p->hp > 0)
        {
            paladin->shield += 2;
            if(paladin->hero && paladin->shield > paladin->hero->max_shield)
                paladin->shield = paladin->hero->max_shield;
            else if(!paladin->hero && paladin->shield > MAX_SHIELD)
                paladin->shield = MAX_SHIELD;
            game_log(g, "【神圣护盾】%s获得2盾（%d→%d），%s流失1体力",
                     paladin->name, old_shield, paladin->shield, turn_p->name);
            /* 用game_lose_hp流失体力，不触发伤害相关技能 */
            game_lose_hp(g, turn_player_idx, 1);
        }
        else
        {
            game_log(g, "【神圣护盾】%s体力为0，无法选择选项1", turn_p->name);
            return;
        }
        break;

    case 2: /* 弃1牌，圣骑士+1盾 */
        if(turn_p->hand_count > 0)
        {
            if(turn_p->is_ai)
            {
                /* AI自动弃牌：随机弃一张 */
                int discard_idx = rand() % turn_p->hand_count;
                Card* discarded = player_remove_hand(turn_p, discard_idx);
                discard_add(&g->discard, discarded);
                paladin->shield += 1;
                if(paladin->hero && paladin->shield > paladin->hero->max_shield)
                    paladin->shield = paladin->hero->max_shield;
                else if(!paladin->hero && paladin->shield > MAX_SHIELD)
                    paladin->shield = MAX_SHIELD;
                game_log(g, "【神圣护盾】%s弃置【%s】，%s获得1盾（%d→%d）",
                         turn_p->name, card_get_full_name(discarded),
                         paladin->name, old_shield, paladin->shield);
            }
            else
            {
                /* 玩家选择：使用通用弃牌函数 */
                game_start_generic_discard(g, turn_player_idx, 1, 100 + 2);  /* source=102表示选项2 */
                game_log(g, "【神圣护盾】请选择要弃置的手牌（选项2：弃牌+1盾）");
                return;  /* 等待玩家选择，不执行后续逻辑 */
            }
        }
        else
        {
            game_log(g, "【神圣护盾】%s没有手牌，无法选择选项2", turn_p->name);
            return;
        }
        break;

    case 3: /* 弃1牌，圣骑士-1盾 */
        if(turn_p->hand_count > 0 && paladin->shield > 0)
        {
            if(turn_p->is_ai)
            {
                /* AI自动弃牌：随机弃一张 */
                int discard_idx = rand() % turn_p->hand_count;
                Card* discarded = player_remove_hand(turn_p, discard_idx);
                discard_add(&g->discard, discarded);
                paladin->shield -= 1;
                if(paladin->shield < 0) paladin->shield = 0;
                game_log(g, "【神圣护盾】%s弃置【%s】，%s失去1盾（%d→%d）",
                         turn_p->name, card_get_full_name(discarded),
                         paladin->name, old_shield, paladin->shield);
            }
            else
            {
                /* 玩家选择：使用通用弃牌函数 */
                game_start_generic_discard(g, turn_player_idx, 1, 100 + 3);  /* source=103表示选项3 */
                game_log(g, "【神圣护盾】请选择要弃置的手牌（选项3：弃牌-1盾）");
                return;  /* 等待玩家选择，不执行后续逻辑 */
            }
        }
        else
        {
            game_log(g, "【神圣护盾】条件不满足，无法选择选项3");
            return;
        }
        break;

    case 4: /* 受1伤害，圣骑士-2盾 */
        if(paladin->shield >= 2)
        {
            /* 先受到1点伤害（有盾时优先扣盾，paladin_take_damage会自动摸牌） */
            game_log(g, "【神圣护盾】%s受到1点伤害，%s失去2盾",
                     turn_p->name, paladin->name);
            g->current_damage_source = DMG_SRC_OTHER;
            game_deal_damage(g, turn_player_idx, 1, paladin_idx, DMG_NORMAL);

            /* 再减少2个盾 */
            int shield_after_damage = paladin->shield;
            paladin->shield -= 2;
            if(paladin->shield < 0) paladin->shield = 0;
            game_log(g, "【神圣护盾】%s失去2盾（%d→%d）",
                     paladin->name, shield_after_damage, paladin->shield);

            /* 手动处理减2盾的摸牌（伤害部分已由paladin_take_damage处理） */
            int delta_2 = paladin->shield - shield_after_damage;
            if(delta_2 != 0)
                paladin_shield_changed(g, paladin_idx, delta_2);

            skip_shield_change = 1;  /* 跳过末尾的总盾数变化计算，避免重复摸牌 */
        }
        else
        {
            game_log(g, "【神圣护盾】%s盾不足2，无法选择选项4", paladin->name);
            return;
        }
        break;

    default:
        return;
    }

    /* 盾数变化摸牌（选项4跳过，因为伤害和减盾已分别处理） */
    if(!skip_shield_change)
    {
        int delta = paladin->shield - old_shield;
        if(delta != 0)
            paladin_shield_changed(g, paladin_idx, delta);
    }

    /* 破灭护盾响应模式：恢复原响应状态 */
    paladin_pomie_resume_response(g);

    game_check_victory(g);
}


/* ================================================================
 * 弃牌选择：点击手牌时选中/取消选中/切换选中
 * ================================================================ */
void paladin_select_discard_card(GameState* g, int hand_index)
{
    if(!g) return;
    if(g->resp_state != RESPONSE_NEED_PALADIN_DISCARD) return;
    if(hand_index < 0) return;

    int turn_idx = g->paladin_choice_turn_idx;
    Player* turn_p = &g->players[turn_idx];
    if(hand_index >= turn_p->hand_count) return;

    if(g->paladin_discard_selected == hand_index)
    {
        /* 再次点击已选中的手牌 → 取消选中 */
        g->paladin_discard_selected = -1;
        game_log(g, "【神圣护盾】取消选中手牌");
    }
    else
    {
        /* 选中新手牌（或切换选中） */
        g->paladin_discard_selected = hand_index;
        Card* c = turn_p->hand[hand_index];
        game_log(g, "【神圣护盾】选中【%s】，点击确定弃牌", card_get_full_name(c));
    }
}


/* ================================================================
 * 玩家点击确定后执行弃牌（使用 paladin_discard_selected）
 * ================================================================ */
void paladin_confirm_discard(GameState* g)
{
    if(!g) return;
    if(g->resp_state != RESPONSE_NEED_PALADIN_DISCARD) return;
    if(g->paladin_discard_selected < 0) return;  /* 未选中手牌 */

    int paladin_idx = g->paladin_choice_paladin_idx;
    int turn_idx = g->paladin_choice_turn_idx;
    int option = g->paladin_discard_option;
    int hand_index = g->paladin_discard_selected;

    Player* turn_p = &g->players[turn_idx];
    Player* paladin = &g->players[paladin_idx];

    if(hand_index >= turn_p->hand_count) return;
    Card* discarded = player_remove_hand(turn_p, hand_index);
    if(!discarded) return;
    discard_add(&g->discard, discarded);

    int old_shield = paladin->shield;

    if(option == 2)  /* 弃1牌，圣骑士+1盾 */
    {
        paladin->shield += 1;
        if(paladin->hero && paladin->shield > paladin->hero->max_shield)
            paladin->shield = paladin->hero->max_shield;
        else if(!paladin->hero && paladin->shield > MAX_SHIELD)
            paladin->shield = MAX_SHIELD;
        game_log(g, "【神圣护盾】%s弃置【%s】，%s获得1盾（%d→%d）",
                 turn_p->name, card_get_full_name(discarded),
                 paladin->name, old_shield, paladin->shield);
    }
    else if(option == 3)  /* 弃1牌，圣骑士-1盾 */
    {
        paladin->shield -= 1;
        if(paladin->shield < 0) paladin->shield = 0;
        game_log(g, "【神圣护盾】%s弃置【%s】，%s失去1盾（%d→%d）",
                 turn_p->name, card_get_full_name(discarded),
                 paladin->name, old_shield, paladin->shield);
    }

    /* 盾数变化摸牌 */
    int delta = paladin->shield - old_shield;
    if(delta != 0)
        paladin_shield_changed(g, paladin_idx, delta);

    /* 重置状态 */
    g->resp_state = RESPONSE_NONE;
    g->paladin_discard_option = -1;
    g->paladin_discard_selected = -1;

    game_check_victory(g);
}


/* ================================================================
 * 玩家选择选项后执行（从GameState获取索引，执行后重置状态）
 * ================================================================ */
void paladin_choose_option(GameState* g, int option)
{
    if(!g) return;
    if(g->resp_state != RESPONSE_NEED_PALADIN_CHOICE) return;
    if(option < 1 || option > 4) return;

    int paladin_idx = g->paladin_choice_paladin_idx;
    int turn_idx = g->paladin_choice_turn_idx;

    paladin_execute_option(g, paladin_idx, turn_idx, option);

    /* 如果进入了通用弃牌选择状态（选项2/3），不要重置resp_state */
    if(g->resp_state != RESPONSE_NEED_GENERIC_DISCARD)
    {
        g->resp_state = RESPONSE_NONE;
        g->paladin_choice_paladin_idx = -1;
        g->paladin_choice_turn_idx = -1;

        /* 神圣护盾选择完成后，继续推进阶段（如果还在准备阶段） */
        if(g->phase == PHASE_PREPARE && !g->game_over)
        {
            g->phase = PHASE_JUDGE;
            game_log(g, "%s 的回合 - 判定阶段", g->players[turn_idx].name);
            game_next_phase(g);
        }
    }
}


/* ================================================================
 * 判断某个选项是否可用（根据当前盾量）
 * 选项1：+2盾，盾>=4时不可用
 * 选项2：+1盾，盾>=5时不可用
 * 选项3：-1盾，盾<=0时不可用
 * 选项4：-2盾，盾<=1时不可用
 * ================================================================ */
int paladin_option_available(GameState* g, int option)
{
    if(!g) return 0;
    if(option < 1 || option > 4) return 0;

    int paladin_idx = g->paladin_choice_paladin_idx;
    if(paladin_idx < 0 || paladin_idx >= g->player_count) return 0;
    Player* paladin = &g->players[paladin_idx];
    if(paladin->hero_id != HERO_PALADIN) return 0;

    int shield = paladin->shield;

    switch(option)
    {
        case 1:  /* +2盾，盾>=4时不可用（4+2=6>5） */
            return (shield < 4);
        case 2:  /* +1盾，盾>=5时不可用（5+1=6>5） */
            return (shield < 5);
        case 3:  /* -1盾，盾<=0时不可用 */
            return (shield > 0);
        case 4:  /* -2盾，盾<=1时不可用（1-2=-1<0） */
            return (shield > 1);
    }
    return 0;
}


/* ================================================================
 * AI自动选择神圣护盾选项
 * 简单策略：盾少就加盾，盾多就减盾
 * ================================================================ */
void paladin_ai_choose(GameState* g, int paladin_idx, int turn_player_idx)
{
    if(!g || paladin_idx < 0 || paladin_idx >= g->player_count) return;
    Player* paladin = &g->players[paladin_idx];
    Player* turn_p = &g->players[turn_player_idx];
    if(paladin->hero_id != HERO_PALADIN) return;

    int option = 0;

    if(paladin->shield <= 1)
    {
        /* 盾少：优先加盾 */
        if(turn_p->hp > 1)
            option = 1;  /* 流失1体力+2盾 */
        else if(turn_p->hand_count > 0)
            option = 2;  /* 弃1牌+1盾 */
        else
            option = 1;  /* 没牌也只能选1 */
    }
    else if(paladin->shield >= 3)
    {
        /* 盾多：优先减盾 */
        if(turn_p->hand_count > 0)
            option = 3;  /* 弃1牌-1盾 */
        else
            option = 4;  /* 受1伤害-2盾 */
    }
    else
    {
        /* 盾中等：随机 */
        option = (rand() % 2 == 0) ? 2 : 3;
        if(option == 2 && turn_p->hand_count == 0) option = 1;
        if(option == 3 && (turn_p->hand_count == 0 || paladin->shield == 0)) option = 1;
    }

    /* 确保选择的选项可用，如果不可用则找第一个可用的 */
    g->paladin_choice_paladin_idx = paladin_idx;
    g->paladin_choice_turn_idx = turn_player_idx;
    if(!paladin_option_available(g, option))
    {
        for(int i = 1; i <= 4; i++)
        {
            if(paladin_option_available(g, i))
            {
                option = i;
                break;
            }
        }
    }

    paladin_execute_option(g, paladin_idx, turn_player_idx, option);
}


/* ================================================================
 * 每名角色回合开始时触发神圣护盾
 * ================================================================ */
void paladin_on_any_turn_start(GameState* g, int paladin_idx, int turn_player_idx)
{
    if(!g || paladin_idx < 0 || paladin_idx >= g->player_count) return;
    Player* p = &g->players[paladin_idx];
    if(p->hero_id != HERO_PALADIN) return;
    if(!p->alive) return;

    game_log(g, "【神圣护盾】%s的回合开始，进行神圣护盾选择",
             g->players[turn_player_idx].name);

    /* 如果选择者是AI，自动选择 */
    if(g->players[turn_player_idx].is_ai)
    {
        paladin_ai_choose(g, paladin_idx, turn_player_idx);
    }
    else
    {
        /* 玩家选择：设置状态，显示2*2选项按钮 */
        g->resp_state = RESPONSE_NEED_PALADIN_CHOICE;
        g->paladin_choice_paladin_idx = paladin_idx;
        g->paladin_choice_turn_idx = turn_player_idx;
        game_log(g, "【神圣护盾】请点击选择一个选项（2*2按钮）");
    }
}


/* ================================================================
 * 破灭护盾：响应牌时触发神圣护盾选择，或主动发动选择牌名打出虚拟牌
 * ================================================================ */
int paladin_can_use_pomie(GameState* g, int paladin_idx)
{
    if(!g || paladin_idx < 0 || paladin_idx >= g->player_count) return 0;
    Player* p = &g->players[paladin_idx];
    if(p->hero_id != HERO_PALADIN) return 0;
    if(!p->alive) return 0;

    /* 每名角色回合限1次，用skill_used[1]记录 */
    return (p->skill_used[1] < 1);
}


void paladin_use_pomie(GameState* g, int paladin_idx)
{
    if(!g || paladin_idx < 0 || paladin_idx >= g->player_count) return;
    Player* p = &g->players[paladin_idx];
    if(p->hero_id != HERO_PALADIN) return;
    if(!paladin_can_use_pomie(g, paladin_idx)) return;

    p->skill_used[1]++;
    game_log(g, "【破灭护盾】%s发动破灭护盾（第%d次）", p->name, p->skill_used[1]);

    /* 判断当前状态：响应时 or 主动发动时 */
    if(g->resp_state == RESPONSE_NEED_BASIC || g->resp_state == RESPONSE_NEED_WUXIE)
    {
        /* 响应时：保存原响应状态，然后进行神圣护盾选择 */
        g->pomie_mode = 1; /* 响应模式 */
        g->pomie_saved_resp_state = g->resp_state;
        g->pomie_saved_trigger_card = g->resp_trigger_card;
        g->pomie_saved_source_player = g->resp_source_player;
        g->pomie_saved_target_player = g->resp_target_player;
        g->pomie_saved_required_basic = g->resp_required_basic;
        g->pomie_saved_duel_turn = g->duel_turn;
        game_log(g, "【破灭护盾】响应模式，保存原响应状态，进行神圣护盾选择");
        paladin_on_any_turn_start(g, paladin_idx, g->current_player);
    }
    else if(g->resp_state == RESPONSE_NONE && g->current_player == paladin_idx && g->phase == PHASE_PLAY)
    {
        if(p->is_ai)
        {
            /* AI自动发动：随机选一个进攻牌名打出虚拟牌 */
            const char* ai_choices[] = {"杀", "决斗", "火攻", "过河拆桥", "顺手牵羊", "无中生有"};
            int choice = rand() % 6;
            const char* name = ai_choices[choice];
            game_log(g, "【破灭护盾】%s（AI）主动打出虚拟牌【%s】", p->name, name);

            Card* vc = (Card*)malloc(sizeof(Card));
            memset(vc, 0, sizeof(Card));
            static int virtual_card_id = 100000;
            vc->id = virtual_card_id++;
            vc->is_valid = 1;
            vc->card_nature = CARD_NATURE_VIRTUAL;
            vc->suit = SUIT_NONE;
            vc->rank = 0;
            vc->color = COLOR_NONE;

            int needs_target = 0;
            if(strcmp(name, "杀") == 0) {
                vc->type = CARD_BASIC; vc->sub.basic.basic_type = BASIC_SHA;
                vc->sub.basic.sha_element = SHA_NORMAL; needs_target = 1;
            } else if(strcmp(name, "决斗") == 0) {
                vc->type = CARD_TRICK; vc->sub.trick.trick_type = TRICK_JUEDOU; needs_target = 1;
            } else if(strcmp(name, "火攻") == 0) {
                vc->type = CARD_TRICK; vc->sub.trick.trick_type = TRICK_HUOGONG; needs_target = 1;
            } else if(strcmp(name, "过河拆桥") == 0) {
                vc->type = CARD_TRICK; vc->sub.trick.trick_type = TRICK_GUOHE; needs_target = 1;
            } else if(strcmp(name, "顺手牵羊") == 0) {
                vc->type = CARD_TRICK; vc->sub.trick.trick_type = TRICK_SHUNSHOU; needs_target = 1;
            } else {
                vc->type = CARD_TRICK; vc->sub.trick.trick_type = TRICK_WUZHONG; needs_target = 0;
            }

            player_draw_card(p, vc);
            int hand_idx = p->hand_count - 1;

            if(needs_target)
            {
                int enemy_idx = (paladin_idx == 0) ? 1 : 0;
                game_use_card(g, paladin_idx, hand_idx, enemy_idx);
            }
            else
            {
                game_use_card_internal(g, paladin_idx, hand_idx, -1);
            }
        }
        else
        {
            /* 玩家主动发动：进入牌名选择界面 */
            g->pomie_mode = 2; /* 主动模式 */
            g->pomie_selected_card_name[0] = '\0';
            g->resp_state = RESPONSE_NEED_PALADIN_POMIE_CARD;
            game_log(g, "【破灭护盾】主动模式，请选择要打出的牌名");
        }
    }
}


/* ================================================================
 * 破灭护盾：牌名选择相关
 * ================================================================ */

/* 破灭护盾可选择的牌名列表（基本牌 + 非延时类锦囊牌） */
static const char* pomie_card_names[] = {
    "杀", "闪", "桃", "酒",
    "无中生有", "过河拆桥", "顺手牵羊", "决斗", "火攻",
    "南蛮入侵", "万箭齐发", "桃园结义", "五谷丰登", "无懈可击", "铁索连环"
};
static const int pomie_card_count = 15;

/* 获取破灭护盾可选择的牌名数量 */
int paladin_pomie_get_card_count(void)
{
    return pomie_card_count;
}

/* 获取指定索引的牌名 */
const char* paladin_pomie_get_card_name(int idx)
{
    if(idx < 0 || idx >= pomie_card_count) return NULL;
    return pomie_card_names[idx];
}

/* 破灭护盾：选择一张牌名 */
void paladin_pomie_select_card(GameState* g, int card_idx)
{
    if(!g || g->resp_state != RESPONSE_NEED_PALADIN_POMIE_CARD) return;
    if(card_idx < 0 || card_idx >= pomie_card_count) return;

    const char* name = pomie_card_names[card_idx];
    strncpy(g->pomie_selected_card_name, name, sizeof(g->pomie_selected_card_name) - 1);
    g->pomie_selected_card_name[sizeof(g->pomie_selected_card_name) - 1] = '\0';
    game_log(g, "【破灭护盾】选中【%s】，点击确认打出，点击取消返回", name);
}

/* 破灭护盾：确认打出虚拟牌 */
void paladin_pomie_confirm(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_PALADIN_POMIE_CARD)
    {
        return;
    }
    if(g->pomie_selected_card_name[0] == '\0')
    {
        game_log(g, "【破灭护盾】请先选择一张牌名");
        return;
    }

    Player* p = &g->players[g->current_player]; /* 当前回合角色 */
    const char* name = g->pomie_selected_card_name;
    game_log(g, "【破灭护盾】%s打出虚拟牌【%s】", p->name, name);

    /* 创建虚拟牌 */
    Card* vc = (Card*)malloc(sizeof(Card));
    memset(vc, 0, sizeof(Card));
    static int virtual_card_id = 100000;
    vc->id = virtual_card_id++; /* 虚拟牌ID，动态分配 */
    vc->is_valid = 1;
    vc->card_nature = CARD_NATURE_VIRTUAL; /* 标记为虚拟牌 */
    vc->suit = SUIT_NONE; /* 无色 */
    vc->rank = 0; /* 无点 */
    vc->color = COLOR_NONE; /* 无色 */

    int needs_target = 0; /* 是否需要选目标 */

    /* 根据牌名设置牌的类型和子类型 */
    if(strcmp(name, "杀") == 0)
    {
        vc->type = CARD_BASIC;
        vc->sub.basic.basic_type = BASIC_SHA;
        vc->sub.basic.sha_element = SHA_NORMAL;
        needs_target = 1;
    }
    else if(strcmp(name, "闪") == 0)
    {
        vc->type = CARD_BASIC;
        vc->sub.basic.basic_type = BASIC_SHAN;
        game_log(g, "【破灭护盾】闪只能在响应时使用，取消");
        free(vc);
        g->resp_state = RESPONSE_NONE;
        g->pomie_mode = 0;
        g->pomie_selected_card_name[0] = '\0';
        return;
    }
    else if(strcmp(name, "桃") == 0)
    {
        vc->type = CARD_BASIC;
        vc->sub.basic.basic_type = BASIC_TAO;
        needs_target = 0;
    }
    else if(strcmp(name, "酒") == 0)
    {
        vc->type = CARD_BASIC;
        vc->sub.basic.basic_type = BASIC_JIU;
        needs_target = 0;
    }
    else if(strcmp(name, "无中生有") == 0)
    {
        vc->type = CARD_TRICK;
        vc->sub.trick.trick_type = TRICK_WUZHONG;
        needs_target = 0;
    }
    else if(strcmp(name, "过河拆桥") == 0)
    {
        vc->type = CARD_TRICK;
        vc->sub.trick.trick_type = TRICK_GUOHE;
        needs_target = 1;
    }
    else if(strcmp(name, "顺手牵羊") == 0)
    {
        vc->type = CARD_TRICK;
        vc->sub.trick.trick_type = TRICK_SHUNSHOU;
        needs_target = 1;
    }
    else if(strcmp(name, "决斗") == 0)
    {
        vc->type = CARD_TRICK;
        vc->sub.trick.trick_type = TRICK_JUEDOU;
        needs_target = 1;
    }
    else if(strcmp(name, "火攻") == 0)
    {
        vc->type = CARD_TRICK;
        vc->sub.trick.trick_type = TRICK_HUOGONG;
        needs_target = 1;
    }
    else if(strcmp(name, "南蛮入侵") == 0)
    {
        vc->type = CARD_TRICK;
        vc->sub.trick.trick_type = TRICK_NANMAN;
        needs_target = 0;
    }
    else if(strcmp(name, "万箭齐发") == 0)
    {
        vc->type = CARD_TRICK;
        vc->sub.trick.trick_type = TRICK_WANJIAN;
        needs_target = 0;
    }
    else if(strcmp(name, "桃园结义") == 0)
    {
        vc->type = CARD_TRICK;
        vc->sub.trick.trick_type = TRICK_TAOYUAN;
        needs_target = 0;
    }
    else if(strcmp(name, "五谷丰登") == 0)
    {
        vc->type = CARD_TRICK;
        vc->sub.trick.trick_type = TRICK_WUGU;
        needs_target = 0;
    }
    else if(strcmp(name, "无懈可击") == 0)
    {
        game_log(g, "【破灭护盾】无懈可击只能在响应锦囊时使用，取消");
        free(vc);
        g->resp_state = RESPONSE_NONE;
        g->pomie_mode = 0;
        g->pomie_selected_card_name[0] = '\0';
        return;
    }
    else if(strcmp(name, "铁索连环") == 0)
    {
        vc->type = CARD_TRICK;
        vc->sub.trick.trick_type = TRICK_TIESUO;
        needs_target = 1;
    }
    else
    {
        game_log(g, "【破灭护盾】未知牌名【%s】，取消", name);
        free(vc);
        g->resp_state = RESPONSE_NONE;
        g->pomie_mode = 0;
        g->pomie_selected_card_name[0] = '\0';
        return;
    }

    /* 把虚拟牌加入手牌 */
    player_draw_card(p, vc);
    int hand_idx = p->hand_count - 1;

    /* 重置破灭护盾状态 */
    g->resp_state = RESPONSE_NONE;
    g->pomie_mode = 0;
    g->pomie_selected_card_name[0] = '\0';

    /* 根据是否需要选目标来打出 */
    if(needs_target)
    {
        /* 需要选目标：进入选目标状态 */
        game_start_target_select(g, hand_idx);
    }
    else
    {
        /* 不需要选目标：直接调用 internal，绕过 game_use_card 的 on_card_used 回调 */
        game_use_card_internal(g, g->current_player, hand_idx, -1);
    }
}

/* 破灭护盾：取消选择（恢复技能使用次数） */
void paladin_pomie_cancel(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_PALADIN_POMIE_CARD) return;

    /* 恢复技能使用次数：主动发动时在use_pomie中已+1，取消时退回 */
    Player* p = &g->players[g->current_player];
    if(p->hero_id == HERO_PALADIN && p->skill_used[1] > 0) {
        p->skill_used[1]--;
    }

    game_log(g, "【破灭护盾】取消选择，技能次数已恢复");
    g->resp_state = RESPONSE_NONE;
    g->pomie_mode = 0;
    g->pomie_selected_card_name[0] = '\0';
}

/* 破灭护盾响应模式：神圣护盾选择完成后，恢复原响应状态继续响应 */
void paladin_pomie_resume_response(GameState* g)
{
    if(!g) return;
    if(g->pomie_mode != 1) return;  /* 只在响应模式下恢复 */

    game_log(g, "【破灭护盾】神圣护盾选择完成，恢复原响应状态继续响应");

    /* 恢复原响应状态 */
    g->resp_state = g->pomie_saved_resp_state;
    g->resp_trigger_card = g->pomie_saved_trigger_card;
    g->resp_source_player = g->pomie_saved_source_player;
    g->resp_target_player = g->pomie_saved_target_player;
    g->resp_required_basic = g->pomie_saved_required_basic;
    g->duel_turn = g->pomie_saved_duel_turn;

    /* 清除保存的状态 */
    g->pomie_mode = 0;
    g->pomie_saved_resp_state = 0;
    g->pomie_saved_trigger_card = NULL;
    g->pomie_saved_source_player = -1;
    g->pomie_saved_target_player = -1;
    g->pomie_saved_required_basic = 0;
    g->pomie_saved_duel_turn = -1;
}


/* ================================================================
 * AI自动使用技能（破灭护盾）
 * 策略：血量低用桃，能击杀用杀，否则用无中生有
 * ================================================================ */
int paladin_ai_use_skill(GameState* g, int paladin_idx)
{
    if(!g || paladin_idx < 0 || paladin_idx >= g->player_count) return 0;
    Player* p = &g->players[paladin_idx];
    if(p->hero_id != HERO_PALADIN) return 0;
    if(!p->alive) return 0;
    if(g->phase != PHASE_PLAY || g->current_player != paladin_idx) return 0;
    if(g->resp_state != RESPONSE_NONE) return 0;

    /* 检查破灭护盾是否可用（每回合限1次） */
    if(!paladin_can_use_pomie(g, paladin_idx)) return 0;

    int enemy_idx = (paladin_idx == 0) ? 1 : 0;
    Player* enemy = &g->players[enemy_idx];
    if(!enemy->alive) return 0;

    int dist = game_calc_distance(g, paladin_idx, enemy_idx);
    int range = player_attack_range(p);
    int can_sha = (dist <= range && p->sha_used < 1);

    /* 选择要打出的虚拟牌牌名 */
    const char* card_name = NULL;
    int needs_target = 0;

    if(p->hp <= 1 && p->shield <= 1)
    {
        /* 血量和盾都很低，优先吃桃 */
        card_name = "桃";
        needs_target = 0;
    }
    else if(can_sha && enemy->hp <= 2 && enemy->shield <= 0)
    {
        /* 能击杀对手，用杀 */
        card_name = "杀";
        needs_target = 1;
    }
    else if(p->hand_count <= 2)
    {
        /* 手牌少，用无中生有摸牌 */
        card_name = "无中生有";
        needs_target = 0;
    }
    else
    {
        /* 其他情况不发动破灭护盾，保留次数 */
        return 0;
    }

    /* 发动破灭护盾 */
    p->skill_used[1]++;
    game_log(g, "【破灭护盾】%s发动破灭护盾，打出虚拟牌【%s】", p->name, card_name);

    /* 创建虚拟牌 */
    Card* vc = (Card*)malloc(sizeof(Card));
    memset(vc, 0, sizeof(Card));
    static int virtual_card_id = 200000;
    vc->id = virtual_card_id++;
    vc->is_valid = 1;
    vc->card_nature = CARD_NATURE_VIRTUAL;
    vc->suit = SUIT_NONE; /* 无色 */
    vc->rank = 0; /* 无点 */
    vc->color = COLOR_NONE; /* 无色 */

    if(strcmp(card_name, "杀") == 0)
    {
        vc->type = CARD_BASIC;
        vc->sub.basic.basic_type = BASIC_SHA;
        vc->sub.basic.sha_element = SHA_NORMAL;
    }
    else if(strcmp(card_name, "桃") == 0)
    {
        vc->type = CARD_BASIC;
        vc->sub.basic.basic_type = BASIC_TAO;
    }
    else if(strcmp(card_name, "无中生有") == 0)
    {
        vc->type = CARD_TRICK;
        vc->sub.trick.trick_type = TRICK_WUZHONG;
    }

    /* 将虚拟牌加入手牌，然后打出 */
    player_draw_card(p, vc);
    int hand_idx = p->hand_count - 1;

    if(needs_target)
    {
        /* 需要选目标的牌：直接打出，指定对手为目标 */
        game_use_card(g, paladin_idx, hand_idx, enemy_idx);
    }
    else
    {
        /* 不需要选目标的牌：直接打出 */
        game_use_card(g, paladin_idx, hand_idx, -1);
    }

    return 1;
}
