#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "jingliu.h"
#include "game.h"
#include "player.h"
#include "card.h"
#include "audio.h"

void jingliu_register(Hero* h)
{
    if (!h) return;
    memset(h, 0, sizeof(Hero));
    h->id = HERO_JINGLIU;
    strncpy(h->name, "镜流", 31);
    h->max_hp = 4;
    h->initial_hp = 3;
    h->max_shield = 5;
    h->initial_shield = 2;
    h->skill_count = 4;

    /* 一技能：狂乱 锁定技 */
    strncpy(h->skills[0].name, "狂乱", 31);
    strncpy(h->skills[0].desc,
        "锁定技，你的手牌数大于4时，将手牌弃置到4并获得等同弃置牌数量的标记；摸牌阶段摸牌数增加X+1（X为标记数）；过河拆桥对你无效。",
        255);
    h->skills[0].type = SKILL_LOCKED;
    h->skills[0].max_uses = -1;

    /* 二技能：无罅飞光 主动 */
    strncpy(h->skills[1].name, "无罅飞光", 31);
    strncpy(h->skills[1].desc,
        "出牌阶段，展示你所有手牌。花色≥1：你的杀最多多指定X个目标(X为标记数)；花色≥2：杀造成伤害+1；花色≥3：可指定目标一个区域弃置一张牌；花色≥4：你的下一张杀无法被响应。",
        255);
    h->skills[1].type = SKILL_ACTIVE;
    h->skills[1].allowed_phases = HERO_PHASE_PLAY;
    h->skills[1].max_uses = 1;

    /* 三技能：古镜照神 主动 */
    strncpy(h->skills[2].name, "古镜照神", 31);
    strncpy(h->skills[2].desc,
        "出牌阶段各限1次：摸3并获得所有人一张牌；或摸5并视为对所有人出一张杀。回合外每名角色阶段限1次：摸一张牌并视为使用需要的牌。",
        255);
    h->skills[2].type = SKILL_ACTIVE;
    h->skills[2].allowed_phases = HERO_PHASE_ALL;
    h->skills[2].max_uses = -1;

    /* 四技能：魔阴 锁定技 */
    strncpy(h->skills[3].name, "魔阴", 31);
    strncpy(h->skills[3].desc,
        "锁定技，你濒死且无人打出桃时进行判定：红，回满体力，失去狂乱获得登仙；黑，体力上限改为6回满，失去全部原有技能获得入魔。",
        255);
    h->skills[3].type = SKILL_LOCKED;
    h->skills[3].max_uses = -1;

    h->draw_bonus = jingliu_draw_bonus;
    h->on_turn_start = jingliu_on_turn_start;
    h->on_round_start = jingliu_on_round_start;
    h->on_card_used = jingliu_on_card_used;
    h->can_use_skill = jingliu_can_use_skill;
    h->use_skill = jingliu_use_skill;
    h->ai_use_skill = jingliu_ai_use_skill;
}

/* 狂乱摸牌加成：X+1 X=薨标记；登仙失去狂乱返回0 */
int jingliu_draw_bonus(const Player* p)
{
    if (!p || p->hero_id != HERO_JINGLIU) return 0;
    if (p->jingliu.transformation != JINGLIU_FORM_NORMAL) return 0;
    return p->jingliu.hong_marks + 1;
}

/* 狂乱免疫过河拆桥 */
int jingliu_cannot_be_guohe(const Player* p)
{
    if (!p || p->hero_id != HERO_JINGLIU) return 0;
    return (p->jingliu.transformation == JINGLIU_FORM_NORMAL);
}

void jingliu_kuangluan_check(GameState* g, int player_idx)
{
    if (!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if (p->hero_id != HERO_JINGLIU) return;
    if (p->jingliu.transformation != JINGLIU_FORM_NORMAL) return;
    if (p->hand_count <= 4) return;

    int discard_count = p->hand_count - 4;
    game_log(g,"【狂乱】%s手牌%d>4，需弃置%d张",p->name,p->hand_count,discard_count);

    if(p->is_ai)
    {
        /* AI：自动弃置前discard_count张 */
        for(int i=0;i<discard_count && p->hand_count>4;i++)
        {
            Card* c = player_remove_hand(p,0);
            if(c) discard_add(&g->discard,c);
        }
        p->jingliu.hong_marks += discard_count;
        game_log(g,"【狂乱】获得%d薨标记，当前：%d",discard_count,p->jingliu.hong_marks);
    }
    else
    {
        /* 玩家：启动通用主动弃牌流程，source=300表示狂乱弃牌 */
        game_start_generic_discard(g, player_idx, discard_count, 300);
    }
}

/* 狂乱弃牌完成后的处理（由game.c回调） */
void jingliu_kuangluan_discard_done(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_JINGLIU) return;
    int discard_count = g->generic_discard_need;
    p->jingliu.hong_marks += discard_count;
    game_log(g,"【狂乱】弃牌完成，获得%d薨标记，当前：%d",discard_count,p->jingliu.hong_marks);
}

void jingliu_wuxia_use(GameState* g, int player_idx)
{
    if (!g || player_idx <0 || player_idx>=g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id!=HERO_JINGLIU) return;
    if(p->jingliu.transformation != JINGLIU_FORM_NORMAL) return;
    if(p->hand_count <=0) return;

    int suits[4]={0};
    for(int i=0;i<p->hand_count;i++)
    {
        Card* c = p->hand[i];
        if(c && c->suit>=0 && c->suit<4) suits[c->suit]=1;
    }
    int suit_cnt = suits[0]+suits[1]+suits[2]+suits[3];
    p->jingliu.wuxia_suit_count = suit_cnt;
    p->jingliu.wuxia_used = 1;

    game_log(g,"【无罅飞光】%s展示全部手牌，花色数量：%d",p->name,suit_cnt);

    if(suit_cnt >=1){
        p->jingliu.sha_extra_target = p->jingliu.hong_marks;
        game_log(g,"【无罅飞光】花色≥1：杀可额外指定%d个目标",p->jingliu.sha_extra_target);
    }
    if(suit_cnt >=2){
        p->jingliu.sha_damage_plus = 1;
        game_log(g,"【无罅飞光】花色≥2：杀伤害+1");
    }
    if(suit_cnt >=3){
        p->jingliu.allow_zone_card = 1;
        game_log(g,"【无罅飞光】花色≥3：可以指定目标区域弃置牌");
    }
    if(suit_cnt >=4){
        p->jingliu.next_sha_unblockable = 1;
        game_log(g,"【无罅飞光】花色≥4：下一张杀不可被响应");
    }
}

/* 无罅飞光花色3：指定区域弃牌 zone:0手牌，1武器，2防具，3进攻马，4防御马，5判定区 */
int jingliu_wuxia_discard_zone(GameState* g, int src_idx, int tgt_idx, int zone)
{
    if(!g) return 0;
    Player* src = &g->players[src_idx];
    Player* tgt = &g->players[tgt_idx];
    if(!src->jingliu.allow_zone_card) return 0;

    Card* del = NULL;
    switch(zone)
    {
        case 0:
            if(tgt->hand_count <=0) return 0;
            del = player_remove_hand(tgt,0);
            break;
        case 1: del = tgt->equip.weapon; tgt->equip.weapon=NULL; break;
        case 2: del = tgt->equip.armor; tgt->equip.armor=NULL; break;
        case 3: del = tgt->equip.horse_atk; tgt->equip.horse_atk=NULL; break;
        case 4: del = tgt->equip.horse_def; tgt->equip.horse_def=NULL; break;
        case 5:
            /* 规格：只允许手牌/装备区，不包括判定区 */
            return 0;
        default: return 0;
    }
    if(del){
        discard_add(&g->discard,del);
        game_log(g,"【无罅飞光】%s弃置%s一个区域的牌",src->name,tgt->name);
        return 1;
    }
    return 0;
}

void jingliu_gujing_use(GameState* g, int player_idx, int option, int target_idx)
{
    if(!g || player_idx<0||player_idx>=g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id!=HERO_JINGLIU) return;
    if(p->jingliu.transformation == JINGLIU_FORM_RUMO) return;

    if(option == GUJING_OPT_GET_ALL)
    {
        /* 出牌阶段：摸3，从每名角色主动选一张手牌/装备区牌 */
        if(g->current_player != player_idx) return;
        if(p->jingliu.gujing_play_opt1) return;
        if(p->jingliu.hong_marks < 3){
            game_log(g, "【古镜照神】标记不足3个（当前%d），无法使用选项1", p->jingliu.hong_marks);
            return;
        }
        p->jingliu.hong_marks -= 3;
        game_log(g, "【古镜照神】%s失去3个薨标记（剩余%d）", p->name, p->jingliu.hong_marks);
        p->jingliu.gujing_play_opt1 = 1;
        game_draw_cards(g,player_idx,3);
        game_log(g,"【古镜照神】%s摸3张，从每名角色主动选一张牌",p->name);

        /* 收集所有其他存活角色，倒序存入（回调时从末尾取） */
        g->gujing_pick_source = player_idx;
        g->gujing_pick_remaining_count = 0;
        for(int i = g->player_count - 1; i >= 0; i--)
        {
            if(i == player_idx) continue;
            if(!g->players[i].alive) continue;
            g->gujing_pick_remaining[g->gujing_pick_remaining_count++] = i;
        }

        /* 对第一个角色进入选牌流程（action=1获得） */
        if(g->gujing_pick_remaining_count > 0)
        {
            int first_target = g->gujing_pick_remaining[--g->gujing_pick_remaining_count];
            Player* ft = &g->players[first_target];
            int has_card = ft->hand_count > 0 || ft->equip.weapon || ft->equip.armor
                          || ft->equip.horse_atk || ft->equip.horse_def;
            if(has_card)
            {
                game_log(g,"【古镜照神】请从%s选择一张牌获得",ft->name);
                g->pick_enemy_callback_type = 2;
                game_start_pick_enemy_card(g, player_idx, first_target, 1);
                return;
            }
        }
        game_log(g,"【古镜照神】没有可选择的角色");
    }
    else if(option == GUJING_OPT_KILL_ALL)
    {
        /* 出牌阶段：摸5，视为对全体出杀（逐个目标结算，可闪） */
        if(g->current_player != player_idx) return;
        if(p->jingliu.gujing_play_opt2) return;
        if(p->jingliu.hong_marks < 5){
            game_log(g, "【古镜照神】标记不足5个（当前%d），无法使用选项2", p->jingliu.hong_marks);
            return;
        }
        p->jingliu.hong_marks -= 5;
        game_log(g, "【古镜照神】%s失去5个薨标记（剩余%d）", p->name, p->jingliu.hong_marks);
        p->jingliu.gujing_play_opt2 = 1;
        game_draw_cards(g,player_idx,5);
        game_log(g,"【古镜照神】%s摸5张，视为对其余所有人打出杀",p->name);

        /* 逐个目标结算：AI自动出闪/受伤，玩家进入响应状态 */
        for(int i=0;i<g->player_count;i++)
        {
            if(i==player_idx) continue;
            if(!g->players[i].alive) continue;
            Player* tp = &g->players[i];
            if(tp->is_ai)
            {
                /* AI目标：自动出闪 */
                int shan_idx = -1;
                for(int h=0;h<tp->hand_count;h++)
                {
                    if(tp->hand[h]->type==CARD_BASIC &&
                       tp->hand[h]->sub.basic.basic_type==BASIC_SHAN)
                    {
                        shan_idx = h; break;
                    }
                }
                if(shan_idx >= 0)
                {
                    Card* sc = player_remove_hand(tp, shan_idx);
                    discard_add(&g->discard, sc);
                    game_log(g,"%s 打出【闪】响应虚拟杀",tp->name);
                }
                else
                {
                    int dmg = game_calc_sha_damage(g, player_idx, i);
                    g->current_damage_source = DMG_SRC_SHA;
                    game_deal_damage(g, i, dmg, player_idx, DMG_NORMAL);
                }
            }
            else
            {
                /* 玩家目标：进入响应状态询问出闪 */
                g->resp_state = RESPONSE_NEED_BASIC;
                g->resp_trigger_card = NULL;
                g->resp_source_player = player_idx;
                g->resp_target_player = i;
                g->resp_required_basic = BASIC_SHAN;
                g->duel_turn = -1;
                game_log(g,"【古镜照神】虚拟杀指向 %s，请点击闪牌选中，点击确认打出，点击取消放弃",tp->name);
                return;  /* 中断，等待玩家响应 */
            }
        }
    }
    else if(option == GUJING_OPT_RESP)
    {
        /* 回合外每名角色阶段限1次，整轮最多3次：摸1，视为需要的牌 */
        if(p->jingliu.gujing_resp_used) return;
        if(p->jingliu.gujing_resp_round_count >= 3) return;
        p->jingliu.gujing_resp_used = 1;
        p->jingliu.gujing_resp_round_count++;
        game_draw_cards(g,player_idx,1);
        game_log(g,"【古镜照神】回合外响应，%s摸一张，视为使用对应需要的牌（本轮%d/3）",
                 p->name,p->jingliu.gujing_resp_round_count);
        /* 卡牌转化逻辑交给 jingliu_card_convert，上层响应逻辑调用 */
    }
}

void jingliu_moyin_judge(GameState* g, int player_idx)
{
    if(!g||player_idx<0||player_idx>=g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id!=HERO_JINGLIU) return;
    if(p->jingliu.transformation != JINGLIU_FORM_NORMAL) return;
    if(p->jingliu.dying_judged) return;

    p->jingliu.dying_judged = 1;
    game_log(g,"【魔阴】%s濒死无人出桃，执行魔阴判定",p->name);

    /* 从牌堆顶翻一张判定牌 */
    Card* judge_card = deck_draw(&g->deck);
    if(!judge_card)
    {
        game_log(g,"【魔阴】牌堆已空，视为黑色");
        jingliu_transform(g,player_idx,JINGLIU_FORM_RUMO);
        return;
    }
    g->central_show_card = judge_card;
    const char* color_str = (judge_card->color == COLOR_RED) ? "红色" : "黑色";
    game_log(g,"【魔阴】判定牌：【%s】（%s）",card_get_full_name(judge_card),color_str);

    if(judge_card->color == COLOR_RED){
        game_log(g,"【魔阴】判定红色 → 登仙");
        jingliu_transform(g,player_idx,JINGLIU_FORM_DENGXIAN);
    }else{
        game_log(g,"【魔阴】判定黑色 → 入魔");
        jingliu_transform(g,player_idx,JINGLIU_FORM_RUMO);
    }
    discard_add(&g->discard, judge_card);
}

void jingliu_transform(GameState* g, int player_idx, int form)
{
    if(!g||player_idx<0||player_idx>=g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id!=HERO_JINGLIU) return;
    p->jingliu.transformation = form;

    if(form == JINGLIU_FORM_DENGXIAN)
    {
        game_log(g,"【登仙】失去【狂乱】；基本牌视为桃，非基本牌视为桃园结义，回满体力");
        p->hp = p->max_hp;
    }
    else if(form == JINGLIU_FORM_RUMO)
    {
        game_log(g,"【入魔】失去狂乱、无罅飞光、古镜照神；体力上限改为6；桃酒只能当杀，全部锦囊视为万箭齐发");
        p->max_hp = 6;
        p->hp = p->max_hp;
        /* 清空本回合全部旧技能标记 */
        p->jingliu.wuxia_used = 0;
        p->jingliu.gujing_play_opt1 = 0;
        p->jingliu.gujing_play_opt2 = 0;
        p->jingliu.gujing_resp_used = 0;
    }
}

/* 卡牌转化：登仙/入魔 */
int jingliu_card_convert(const Player* p, const Card* src_card, int* out_type, int* out_sub)
{
    if(!p || !src_card || !out_type || !out_sub) return 0;
    if(p->hero_id != HERO_JINGLIU) return 0;

    if(p->jingliu.transformation == JINGLIU_FORM_DENGXIAN)
    {
        if(src_card->type == CARD_BASIC){
            *out_type = CARD_BASIC;
            *out_sub = BASIC_TAO;
            return 1;
        }else{
            *out_type = CARD_TRICK;
            *out_sub = TRICK_TAOYUAN;
            return 1;
        }
    }
    if(p->jingliu.transformation == JINGLIU_FORM_RUMO)
    {
        if(src_card->type == CARD_BASIC
            && (src_card->sub.basic.basic_type == BASIC_TAO || src_card->sub.basic.basic_type == BASIC_JIU))
        {
            *out_type = CARD_BASIC;
            *out_sub = BASIC_SHA;
            return 1;
        }
        if(src_card->type == CARD_TRICK)
        {
            *out_type = CARD_TRICK;
            *out_sub = TRICK_WANJIAN;
            return 1;
        }
    }
    return 0;
}

void jingliu_on_turn_start(GameState* g, int player_idx)
{
    if(!g||player_idx<0||player_idx>=g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id!=HERO_JINGLIU) return;

    p->jingliu.wuxia_used = 0;
    p->jingliu.wuxia_suit_count = 0;
    p->jingliu.gujing_play_opt1 = 0;
    p->jingliu.gujing_play_opt2 = 0;
    p->jingliu.gujing_resp_used = 0;
    p->jingliu.sha_extra_target = 0;
    p->jingliu.sha_damage_plus = 0;
    p->jingliu.allow_zone_card = 0;
    p->jingliu.next_sha_unblockable = 0;

    if(p->jingliu.transformation == JINGLIU_FORM_NORMAL){
        jingliu_kuangluan_check(g,player_idx);
    }
}

void jingliu_on_round_start(GameState* g, int player_idx)
{
    if(!g||player_idx<0||player_idx>=g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id!=HERO_JINGLIU) return;
    p->jingliu.dying_judged = 0;
    p->jingliu.gujing_resp_used = 0;
    p->jingliu.gujing_resp_round_count = 0;
}

void jingliu_on_card_used(GameState* g, int player_idx, Card* card)
{
    if(!g || !card || player_idx<0||player_idx>=g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id!=HERO_JINGLIU) return;
    /* next_sha_unblockable：打出杀之后消耗标记 */
    if(card->type == CARD_BASIC && card->sub.basic.basic_type == BASIC_SHA){
        p->jingliu.next_sha_unblockable = 0;
    }
}

int jingliu_can_use_skill(GameState* g, int player_idx, int skill_idx)
{
    if(!g||player_idx<0||player_idx>=g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id!=HERO_JINGLIU) return 0;
    if(p->jingliu.transformation == JINGLIU_FORM_RUMO) return 0;

    if(skill_idx == 1) /* 无罅飞光 */
    {
        return (g->phase == PHASE_PLAY && g->current_player == player_idx
            && p->jingliu.wuxia_used == 0 && p->hand_count>0);
    }
    if(skill_idx == 2) /* 古镜照神 */
    {
        if(g->phase == PHASE_PLAY && g->current_player == player_idx)
        {
            return (!p->jingliu.gujing_play_opt1 || !p->jingliu.gujing_play_opt2);
        }else{
            /* 回合外：每名角色回合限1次，整轮最多3次 */
            return (!p->jingliu.gujing_resp_used && p->jingliu.gujing_resp_round_count < 3);
        }
    }
    return 0;
}

void jingliu_use_skill(GameState* g, int player_idx, int skill_idx)
{
    if(!g||player_idx<0||player_idx>=g->player_count) return;
    Player* p = &g->players[player_idx];
    if(p->hero_id!=HERO_JINGLIU) return;
    if(p->jingliu.transformation == JINGLIU_FORM_RUMO) return;

    if(skill_idx == 1){
        jingliu_wuxia_use(g,player_idx);
    }
    else if(skill_idx == 2){
        /* 出牌阶段：进入选项选择UI */
        if(g->phase == PHASE_PLAY && g->current_player == player_idx){
            g->resp_state = RESPONSE_NEED_JINGLIU_GUJING;
            game_log(g, "【古镜照神】请选择：选项1(失去3标记,摸3+拿牌) / 选项2(失去5标记,摸5+全体杀) / 取消");
            return;
        }
        /* 回合外：直接使用响应选项 */
        jingliu_gujing_use(g,player_idx,GUJING_OPT_RESP,-1);
    }
}


/* ================================================================
 * AI自动使用技能
 * 策略：古镜照神优先用全体伤害，无罅飞光花色多时使用
 * ================================================================ */
int jingliu_ai_use_skill(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_JINGLIU) return 0;
    if(!p->alive) return 0;
    if(g->phase != PHASE_PLAY || g->current_player != player_idx) return 0;
    if(g->resp_state != RESPONSE_NONE) return 0;
    if(p->jingliu.transformation != JINGLIU_FORM_NORMAL) return 0;

    /* 古镜照神：优先用选项2（摸5+全体伤害） */
    if(!p->jingliu.gujing_play_opt2)
    {
        jingliu_gujing_use(g, player_idx, GUJING_OPT_KILL_ALL, -1);
        return 1;
    }

    /* 古镜照神：选项1（摸3+拿所有人一张） */
    if(!p->jingliu.gujing_play_opt1)
    {
        jingliu_gujing_use(g, player_idx, GUJING_OPT_GET_ALL, -1);
        return 1;
    }

    /* 无罅飞光：手牌花色>=2时使用 */
    if(!p->jingliu.wuxia_used && p->hand_count >= 2)
    {
        int suits[4] = {0};
        for(int i = 0; i < p->hand_count; i++)
        {
            Card* c = p->hand[i];
            if(c && c->suit >= 0 && c->suit < 4) suits[c->suit] = 1;
        }
        int suit_cnt = suits[0] + suits[1] + suits[2] + suits[3];
        if(suit_cnt >= 2)
        {
            jingliu_wuxia_use(g, player_idx);
            return 1;
        }
    }

    return 0;
}

/* ================================================================
 * 古镜照神选项UI
 * ================================================================ */
int jingliu_gujing_option_available(GameState* g, int player_idx, int option)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(p->hero_id != HERO_JINGLIU) return 0;
    if(p->jingliu.transformation == JINGLIU_FORM_RUMO) return 0;

    if(option == 1){
        return (!p->jingliu.gujing_play_opt1 && p->jingliu.hong_marks >= 3);
    }
    if(option == 2){
        return (!p->jingliu.gujing_play_opt2 && p->jingliu.hong_marks >= 5);
    }
    return 0;
}

void jingliu_gujing_choose_option(GameState* g, int player_idx, int option)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    if(g->resp_state != RESPONSE_NEED_JINGLIU_GUJING) return;

    if(option == 1 || option == 2){
        if(jingliu_gujing_option_available(g, player_idx, option)){
            g->resp_state = RESPONSE_NONE;
            jingliu_gujing_use(g, player_idx,
                (option == 1) ? GUJING_OPT_GET_ALL : GUJING_OPT_KILL_ALL, -1);
        } else {
            game_log(g, "【古镜照神】选项%d当前不可用", option);
        }
    }
}

void jingliu_gujing_cancel(GameState* g)
{
    if(!g) return;
    if(g->resp_state != RESPONSE_NEED_JINGLIU_GUJING) return;
    game_log(g, "【古镜照神】取消选择");
    g->resp_state = RESPONSE_NONE;
}
