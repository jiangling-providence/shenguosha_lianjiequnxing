#include <stdio.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "input.h"
#include "game.h"
#include "heroes/hero.h"
#include "heroes/paladin/paladin.h"
#include "heroes/liuying/liuying.h"
#include "heroes/linyuxia/linyuxia.h"
#include "heroes/zhaoyun/zhaoyun.h"
#include "heroes/jingliu/jingliu.h"
#include "heroes/yudie/yudie.h"


/* ================================================================
 * 技能列表点击检测
 * 技能列表位置和 render.c 保持一致：
 *   skill_x = feixiao_x - 130 - 20
 *   skill_y = feixiao_y - 80
 *   skill_w = 130, skill_h = 20
 * 返回技能下标，-1表示没点中
 * ================================================================ */
int input_hit_skill(GameState* game, int mx, int my)
{
    if(!game) return -1;
    Player* me = &game->players[0];
    if(!me->hero || me->hero->skill_count <= 0) return -1;

    int feixiao_x = WINDOW_WIDTH - HERO_WIDTH - 40;
    int feixiao_y = WINDOW_HEIGHT - HERO_HEIGHT - 40;
    int skill_w = 100;
    int skill_h = 18;
    int skill_x = feixiao_x - skill_w - 20;
    int skill_y = feixiao_y - 80;

    /* 统计主动技能和被动技能 */
    int active_skills[10], active_count = 0;
    int passive_skills[10], passive_count = 0;
    for(int s = 0; s < me->hero->skill_count; s++)
    {
        if(me->hero->skills[s].type == SKILL_ACTIVE)
            active_skills[active_count++] = s;
        else
            passive_skills[passive_count++] = s;
    }

    int cur_y = skill_y;

    /* 主动技组 */
    if(active_count > 0)
    {
        cur_y += 18;  /* 跳过标题 */
        for(int i = 0; i < active_count; i++)
        {
            if(mx >= skill_x && mx <= skill_x + skill_w &&
               my >= cur_y && my <= cur_y + skill_h)
                return active_skills[i];
            cur_y += skill_h + 3;
        }
        cur_y += 8;  /* 组间间距 */
    }

    /* 被动技组（不可点击，只检测位置但返回-1） */
    if(passive_count > 0)
    {
        cur_y += 18;  /* 跳过标题 */
        for(int i = 0; i < passive_count; i++)
        {
            cur_y += skill_h + 3;
        }
    }

    return -1;
}


/* ================================================================
 * 技能列表长按检测（包括主动技和被动技）
 * 和 input_hit_skill 的区别：被动技也可以检测到，用于长按显示描述
 * 返回技能下标，-1表示没点中
 * ================================================================ */
int input_hit_skill_for_long_press(GameState* game, int mx, int my)
{
    if(!game) return -1;
    Player* me = &game->players[0];
    if(!me->hero || me->hero->skill_count <= 0) return -1;

    int feixiao_x = WINDOW_WIDTH - HERO_WIDTH - 40;
    int feixiao_y = WINDOW_HEIGHT - HERO_HEIGHT - 40;
    int skill_w = 100;
    int skill_h = 18;
    int skill_x = feixiao_x - skill_w - 20;
    int skill_y = feixiao_y - 80;

    /* 统计主动技能和被动技能 */
    int active_skills[10], active_count = 0;
    int passive_skills[10], passive_count = 0;
    for(int s = 0; s < me->hero->skill_count; s++)
    {
        if(me->hero->skills[s].type == SKILL_ACTIVE)
            active_skills[active_count++] = s;
        else
            passive_skills[passive_count++] = s;
    }

    int cur_y = skill_y;

    /* 主动技组 */
    if(active_count > 0)
    {
        cur_y += 18;  /* 跳过标题 */
        for(int i = 0; i < active_count; i++)
        {
            if(mx >= skill_x && mx <= skill_x + skill_w &&
               my >= cur_y && my <= cur_y + skill_h)
                return active_skills[i];
            cur_y += skill_h + 3;
        }
        cur_y += 8;  /* 组间间距 */
    }

    /* 被动技组（长按也可以检测到） */
    if(passive_count > 0)
    {
        cur_y += 18;  /* 跳过标题 */
        for(int i = 0; i < passive_count; i++)
        {
            if(mx >= skill_x && mx <= skill_x + skill_w &&
               my >= cur_y && my <= cur_y + skill_h)
                return passive_skills[i];
            cur_y += skill_h + 3;
        }
    }

    return -1;
}


void input_init(InputState* state)
{
    if (!state) return;

    state->last_click_time = 0;
    state->last_click_x = -1;
    state->last_click_y = -1;

    state->selected_hand_index = -1;
    state->mouse_x = 0;
    state->mouse_y = 0;
    state->mouse_down_time = 0;
    state->mouse_down_x = 0;
    state->mouse_down_y = 0;
    state->long_press_skill_idx = -1;

    /* 日志弹窗拖动 */
    state->log_dragging = 0;
    state->log_drag_start_y = 0;
    state->log_drag_start_scroll = 0;
}


void input_destroy(InputState* state)
{
    if(!state) return;
    state->selected_hand_index = -1;
}


static int calc_hand_start_x(GameState* game)
{
    (void)game;
    /* 手牌从日志右边开始排列，避开左下角日志区域 */
    return 320;
}


static int calc_hand_y(void)
{
    int feixiao_y = WINDOW_HEIGHT - HERO_HEIGHT - 40;
    return feixiao_y + HERO_HEIGHT - CARD_HEIGHT + 10;
}


/* ================================================================
 * 每帧调用，检测长按（约2秒）
 * 长按技能按钮时，在屏幕中心显示技能描述
 * ================================================================ */
void input_update(InputState* state, GameState* game)
{
    if(!state || !game) return;

    /* 鼠标未按下：不清除技能描述（点击描述框外才关闭） */
    if(state->mouse_down_time == 0)
    {
        return;
    }

    /* 已经触发长按，保持显示 */
    if(state->long_press_skill_idx >= 0)
    {
        game->long_press_skill_idx = state->long_press_skill_idx;
        return;
    }

    /* 检测是否按住超过1秒（更容易触发） */
    Uint32 now = SDL_GetTicks();
    if(now - state->mouse_down_time >= 1000)
    {
        /* 检测按下位置是否在技能按钮上（包括主动技和被动技） */
        int skill_idx = input_hit_skill_for_long_press(game, state->mouse_down_x, state->mouse_down_y);
        if(skill_idx >= 0)
        {
            state->long_press_skill_idx = skill_idx;
            game->long_press_skill_idx = skill_idx;
            Player* me = &game->players[0];
            if(me->hero && skill_idx < me->hero->skill_count)
            {
                game_log(game, "【长按查看技能】%s：%s",
                         me->hero->skills[skill_idx].name,
                         me->hero->skills[skill_idx].desc);
            }
        }
    }
}


int input_hit_hand_card(GameState* game, RenderContext* ctx, int mx, int my)
{
    (void)ctx;
    if (!game) return -1;
    Player* me = &game->players[0];
    int start_x = calc_hand_start_x(game);
    int hand_y = calc_hand_y();
    for (int i = 0; i < me->hand_count; i++) {
        int cx = start_x + i * (CARD_WIDTH + 5);
        if (mx >= cx && mx <= cx + CARD_WIDTH &&
            my >= hand_y && my <= hand_y + CARD_HEIGHT) {
            return i;
        }
    }
    return -1;
}


/* ================================================================
 * 检测点击的角色下标
 * 玩家0(feixiao)：右下角
 * 玩家1(zhaoyun)：左上角
 * 返回 -1 表示没点中
 * ================================================================ */
int input_hit_player(GameState* game, int mx, int my)
{
    if(!game) return -1;

    /* 检测区域扩大30像素容错，处理DPI缩放和坐标偏差 */
    int pad = 30;

    /* 通用：遍历所有玩家，使用通用位置计算函数 */
    for(int i = 0; i < game->player_count; i++)
    {
        if(!game->players[i].alive) continue;
        int px, py;
        game_get_player_position(i, game->player_count, &px, &py);
        if(mx >= px - pad && mx <= px + HERO_WIDTH + pad &&
           my >= py - pad && my <= py + HERO_HEIGHT + pad)
            return i;
    }

    return -1;
}


/* ================================================================
 * 检测点击的是否是玩家0装备区的贯石斧
 * 贯石斧渲染位置：(hand_start_x, hand_y - CARD_HEIGHT - 25)
 * 返回1=点中贯石斧，0=没点中
 * ================================================================ */
int input_hit_guanshi(GameState* game, int mx, int my)
{
    if(!game) return 0;
    Player* me = &game->players[0];

    /* 必须装备贯石斧 */
    if(player_weapon_type(me) != WEAPON_GUANSHI)
        return 0;

    int weapon_x = calc_hand_start_x(game);
    int weapon_y = calc_hand_y() - CARD_HEIGHT - 25;

    if(mx >= weapon_x && mx <= weapon_x + CARD_WIDTH &&
       my >= weapon_y && my <= weapon_y + CARD_HEIGHT)
        return 1;

    return 0;
}


/* ================================================================
 * 寒冰剑相关点击检测
 * ================================================================ */

/* 检测点击的是否是玩家0装备区的寒冰剑 */
int input_hit_hanbing(GameState* game, int mx, int my)
{
    if(!game) return 0;
    Player* me = &game->players[0];
    if(player_weapon_type(me) != WEAPON_HANBING)
        return 0;
    int weapon_x = calc_hand_start_x(game);
    int weapon_y = calc_hand_y() - CARD_HEIGHT - 25;
    if(mx >= weapon_x && mx <= weapon_x + CARD_WIDTH &&
       my >= weapon_y && my <= weapon_y + CARD_HEIGHT)
        return 1;
    return 0;
}

/* 对方手牌起始x坐标（和render.c里的渲染位置一致） */
static int calc_enemy_hand_start_x(GameState* game)
{
    Player* enemy = &game->players[1];
    int zhaoyun_x = (WINDOW_WIDTH - HERO_WIDTH) / 2;
    int total_width = enemy->hand_count * (CARD_WIDTH + 5);
    return zhaoyun_x + HERO_WIDTH / 2 - total_width / 2;
}

/* 对方手牌y坐标（和render.c里的渲染位置一致） */
static int calc_enemy_hand_y(void)
{
    int zhaoyun_y = 60;
    return zhaoyun_y + HERO_HEIGHT + 85;
}

/* 检测点击的是否是对方（玩家1）的手牌，返回下标，-1表示没点中 */
int input_hit_enemy_hand_card(GameState* game, int mx, int my)
{
    if(!game) return -1;
    Player* enemy = &game->players[1];
    int start_x = calc_enemy_hand_start_x(game);
    int hand_y = calc_enemy_hand_y();
    for(int i = 0; i < enemy->hand_count; i++)
    {
        int cx = start_x + i * (CARD_WIDTH + 5);
        if(mx >= cx && mx <= cx + CARD_WIDTH &&
           my >= hand_y && my <= hand_y + CARD_HEIGHT)
            return i;
    }
    return -1;
}

/* 检测点击的是否是对方（玩家1）的装备区的牌
 * 返回：1=武器,2=防具,3=进攻马,4=防御马,0=没点中
 * 通过 card_index 返回装备下标（目前都是0，因为每个装备槽只有一张）
 */
int input_hit_enemy_equip(GameState* game, int mx, int my, int* out_type)
{
    if(!game || !out_type) return 0;
    Player* enemy = &game->players[1];
    int zhaoyun_x = (WINDOW_WIDTH - HERO_WIDTH) / 2;
    int equip_x = zhaoyun_x - 4 * (CARD_WIDTH + 10) - 20;
    int equip_y = 60 + 30;

    /* 装备区从左到右：武器、防具、进攻马、防御马 */
    Card* equips[4] = {enemy->equip.weapon, enemy->equip.armor,
                        enemy->equip.horse_atk, enemy->equip.horse_def};
    int types[4] = {1, 2, 3, 4};

    for(int i = 0; i < 4; i++)
    {
        if(!equips[i]) continue;
        int cx = equip_x + i * (CARD_WIDTH + 10);
        if(mx >= cx && mx <= cx + CARD_WIDTH &&
           my >= equip_y && my <= equip_y + CARD_HEIGHT)
        {
            *out_type = types[i];
            return 1;
        }
    }
    return 0;
}

/* 检测点击的是否是对方（玩家1）的延时锦囊区的牌
 * 返回1表示点中，card_index 返回延时锦囊下标
 */
int input_hit_enemy_judge(GameState* game, int mx, int my, int* out_index)
{
    if(!game || !out_index) return 0;
    Player* enemy = &game->players[1];
    int zhaoyun_x = (WINDOW_WIDTH - HERO_WIDTH) / 2;
    int judge_x = zhaoyun_x + HERO_WIDTH + 20;
    int judge_y = 60 + 30;

    for(int i = 0; i < enemy->judge.count; i++)
    {
        if(!enemy->judge.cards[i]) continue;
        int cx = judge_x + i * (CARD_WIDTH + 10);
        if(mx >= cx && mx <= cx + CARD_WIDTH &&
           my >= judge_y && my <= judge_y + CARD_HEIGHT)
        {
            *out_index = i;
            return 1;
        }
    }
    return 0;
}

/* 检测点击的是否是玩家0装备区的朱雀羽扇 */
int input_hit_zhuque(GameState* game, int mx, int my)
{
    if(!game) return 0;
    Player* me = &game->players[0];
    if(player_weapon_type(me) != WEAPON_ZHUQUE)
        return 0;
    int weapon_x = calc_hand_start_x(game);
    int weapon_y = calc_hand_y() - CARD_HEIGHT - 25;
    if(mx >= weapon_x && mx <= weapon_x + CARD_WIDTH &&
       my >= weapon_y && my <= weapon_y + CARD_HEIGHT)
        return 1;
    return 0;
}

/* 检测点击的是否是玩家0装备区的丈八蛇矛 */
int input_hit_zhangba(GameState* game, int mx, int my)
{
    if(!game) return 0;
    Player* me = &game->players[0];
    if(player_weapon_type(me) != WEAPON_ZHANGBA)
        return 0;
    int weapon_x = calc_hand_start_x(game);
    int weapon_y = calc_hand_y() - CARD_HEIGHT - 25;
    if(mx >= weapon_x && mx <= weapon_x + CARD_WIDTH &&
       my >= weapon_y && my <= weapon_y + CARD_HEIGHT)
        return 1;
    return 0;
}

/* 检测点击的是否是玩家0装备区的八卦阵 */
int input_hit_bagua(GameState* game, int mx, int my)
{
    if(!game) return 0;
    Player* me = &game->players[0];
    if(player_armor_type(me) != ARMOR_BAGUA)
        return 0;
    /* 防具在武器右边 */
    int armor_x = calc_hand_start_x(game) + CARD_WIDTH + 10;
    int armor_y = calc_hand_y() - CARD_HEIGHT - 25;
    if(mx >= armor_x && mx <= armor_x + CARD_WIDTH &&
       my >= armor_y && my <= armor_y + CARD_HEIGHT)
        return 1;
    return 0;
}


/* ================================================================
 * 双按钮：确定（右）+ 取消（左）
 * 按钮位置：屏幕中心下边一点，手牌上方
 * ================================================================ */
#define BTN_WIDTH   140
#define BTN_HEIGHT  45
#define BTN_Y       (WINDOW_HEIGHT / 2 + 100)
#define CONFIRM_BTN_X  (WINDOW_WIDTH / 2 + 10)   /* 确定按钮：右边 */
#define CANCEL_BTN_X   (WINDOW_WIDTH / 2 - BTN_WIDTH - 10)  /* 取消按钮：左边 */
#define CHONGZHU_BTN_X (WINDOW_WIDTH / 2 - BTN_WIDTH * 2 - 30)  /* 重铸按钮：最左边 */

/* 检测点击的是否是确定按钮 */
int input_hit_confirm_button(int mx, int my)
{
    if(mx >= CONFIRM_BTN_X && mx <= CONFIRM_BTN_X + BTN_WIDTH &&
       my >= BTN_Y && my <= BTN_Y + BTN_HEIGHT)
        return 1;
    return 0;
}

/* 检测点击的是否是取消按钮 */
int input_hit_cancel_button(int mx, int my)
{
    if(mx >= CANCEL_BTN_X && mx <= CANCEL_BTN_X + BTN_WIDTH &&
       my >= BTN_Y && my <= BTN_Y + BTN_HEIGHT)
        return 1;
    return 0;
}

/* 检测点击的是否是重铸按钮（铁索连环） */
int input_hit_chongzhu_button(int mx, int my)
{
    if(mx >= CHONGZHU_BTN_X && mx <= CHONGZHU_BTN_X + BTN_WIDTH &&
       my >= BTN_Y && my <= BTN_Y + BTN_HEIGHT)
        return 1;
    return 0;
}

/* 兼容旧代码：检测任意一个按钮（确定或取消） */
int input_hit_shan_button(int mx, int my)
{
    return input_hit_confirm_button(mx, my) || input_hit_cancel_button(mx, my);
}

/* ================================================================
 * 圣骑士神圣护盾：2*2选项按钮点击检测
 * 返回选项编号1-4，0表示没点中
 * ================================================================ */
int input_hit_paladin_option(int mx, int my)
{
    int btn_w = 220;
    int btn_h = 90;
    int gap = 20;
    int total_w = btn_w * 2 + gap;
    int start_x = WINDOW_WIDTH / 2 - total_w / 2;
    int start_y = WINDOW_HEIGHT / 2 - btn_h - gap / 2;

    for(int i = 0; i < 4; i++)
    {
        int col = i % 2;
        int row = i / 2;
        int bx = start_x + col * (btn_w + gap);
        int by = start_y + row * (btn_h + gap);
        if(mx >= bx && mx <= bx + btn_w &&
           my >= by && my <= by + btn_h)
            return i + 1;
    }
    return 0;
}

/* ================================================================
 * 镜流古镜照神：选项按钮点击检测
 * 返回1=选项1, 2=选项2, 3=取消, 0=没点中
 * ================================================================ */
int input_hit_jingliu_gujing_option(int mx, int my)
{
    int btn_w = 240;
    int btn_h = 100;
    int gap = 30;
    int total_w = btn_w * 2 + gap;
    int start_x = WINDOW_WIDTH / 2 - total_w / 2;
    int start_y = WINDOW_HEIGHT / 2 - btn_h / 2 - 30;

    /* 选项1 */
    if(mx >= start_x && mx <= start_x + btn_w &&
       my >= start_y && my <= start_y + btn_h)
        return 1;
    /* 选项2 */
    if(mx >= start_x + btn_w + gap && mx <= start_x + btn_w * 2 + gap &&
       my >= start_y && my <= start_y + btn_h)
        return 2;
    /* 取消按钮 */
    int cancel_w = 120;
    int cancel_h = 50;
    int cancel_x = WINDOW_WIDTH / 2 - cancel_w / 2;
    int cancel_y = start_y + btn_h + 30;
    if(mx >= cancel_x && mx <= cancel_x + cancel_w &&
       my >= cancel_y && my <= cancel_y + cancel_h)
        return 3;
    return 0;
}

/* ================================================================
 * 化形①：2*2花色按钮点击检测
 * 返回花色0-3，-1表示没点中，-2表示点了结束按钮
 * ================================================================ */
int input_hit_huaxing_suit(int mx, int my)
{
    int btn_w = 180;
    int btn_h = 100;
    int gap = 25;
    int total_w = btn_w * 2 + gap;
    int start_x = WINDOW_WIDTH / 2 - total_w / 2;
    int start_y = WINDOW_HEIGHT / 2 - btn_h - gap / 2;

    for(int i = 0; i < 4; i++)
    {
        int col = i % 2;
        int row = i / 2;
        int bx = start_x + col * (btn_w + gap);
        int by = start_y + row * (btn_h + gap);
        if(mx >= bx && mx <= bx + btn_w &&
           my >= by && my <= by + btn_h)
            return i;
    }

    /* 结束按钮 */
    int end_bx = WINDOW_WIDTH / 2 - 60;
    int end_by = start_y + 2 * btn_h + gap + 20;
    if(mx >= end_bx && mx <= end_bx + 120 &&
       my >= end_by && my <= end_by + 40)
        return -2;

    return -1;
}

/* ================================================================
 * 化形①：锦囊牌名按钮点击检测
 * 返回锦囊索引，-1表示没点中
 * ================================================================ */
int input_hit_huaxing_trick(GameState* game, int mx, int my)
{
    if(!game) return -1;
    Player* me = &game->players[0];

    /* 获取可用锦囊数量 */
    char tricks[10][32];
    int trick_count = 0;
    for(int i = 0; i < me->yudie.huaxing_record_count && trick_count < 10; i++)
    {
        int dup = 0;
        for(int j = 0; j < trick_count; j++)
            if(strcmp(tricks[j], me->yudie.huaxing_record_names[i]) == 0) { dup = 1; break; }
        for(int j = 0; j < me->yudie.huaxing_played_name_count && !dup; j++)
            if(strcmp(me->yudie.huaxing_played_names[j], me->yudie.huaxing_record_names[i]) == 0) { dup = 1; break; }
        if(!dup) { strncpy(tricks[trick_count], me->yudie.huaxing_record_names[i], 31); tricks[trick_count][31]='\0'; trick_count++; }
    }

    int cols = 4;
    int btn_w = 140;
    int btn_h = 50;
    int gap_x = 12;
    int gap_y = 12;
    int rows = (trick_count + cols - 1) / cols;
    int total_w = cols * btn_w + (cols - 1) * gap_x;
    int start_x = WINDOW_WIDTH / 2 - total_w / 2;
    int start_y = WINDOW_HEIGHT / 2 - (rows * btn_h + (rows-1) * gap_y) / 2;

    for(int i = 0; i < trick_count; i++)
    {
        int col = i % cols;
        int row = i / cols;
        int bx = start_x + col * (btn_w + gap_x);
        int by = start_y + row * (btn_h + gap_y);
        if(mx >= bx && mx <= bx + btn_w &&
           my >= by && my <= by + btn_h)
            return i;
    }
    return -1;
}

/* 检测玩家手牌中是否有当前响应需要的牌，返回下标，-1表示没有 */
int input_find_response_card(GameState* game)
{
    if(!game) return -1;
    Player* me = &game->players[0];

    for(int i = 0; i < me->hand_count; i++)
    {
        Card* c = me->hand[i];
        if(!c) continue;

        /* 无懈可击响应 */
        if(game->resp_state == RESPONSE_NEED_WUXIE)
        {
            if(c->type == CARD_TRICK && c->sub.trick.trick_type == TRICK_WUXIE)
                return i;
        }
        /* 基本牌响应：杀或闪 */
        else if(game->resp_state == RESPONSE_NEED_BASIC)
        {
            if(c->type == CARD_BASIC &&
               c->sub.basic.basic_type == game->resp_required_basic)
                return i;
        }
    }
    return -1;
}

/* 判断一张牌是否是当前响应需要的牌 */
int input_is_response_card(GameState* game, Card* card)
{
    if(!game || !card) return 0;

    if(game->resp_state == RESPONSE_NEED_WUXIE)
    {
        return (card->type == CARD_TRICK && card->sub.trick.trick_type == TRICK_WUXIE);
    }
    else if(game->resp_state == RESPONSE_NEED_MULTI_WUXIE)
    {
        return (card->type == CARD_TRICK && card->sub.trick.trick_type == TRICK_WUXIE);
    }
    else if(game->resp_state == RESPONSE_NEED_BASIC)
    {
        if(card->type == CARD_BASIC &&
           card->sub.basic.basic_type == game->resp_required_basic)
            return 1;

        /* 赵云龙胆：杀当闪、闪当杀（龙胆模式下始终可用，玩家自由选择用原生牌还是转换牌） */
        Player* me = &game->players[0];
        if(me->hero_id == HERO_ZHAOYUN && me->longdan_active &&
           card->type == CARD_BASIC)
        {
            BasicType bt = card->sub.basic.basic_type;
            /* 需要闪时，杀可以当闪；需要杀时，闪可以当杀 */
            if((game->resp_required_basic == BASIC_SHAN && bt == BASIC_SHA) ||
               (game->resp_required_basic == BASIC_SHA && bt == BASIC_SHAN))
            {
                return 1;
            }
        }
    }
    return 0;
}

/* 获取当前响应需要的牌名 */
const char* input_get_response_card_name(GameState* game)
{
    if(!game) return "";
    if(game->resp_state == RESPONSE_NEED_WUXIE) return "无懈可击";
    if(game->resp_state == RESPONSE_NEED_MULTI_WUXIE) return "无懈可击";
    if(game->resp_state == RESPONSE_NEED_BASIC)
    {
        if(game->resp_required_basic == BASIC_SHA) return "杀";
        if(game->resp_required_basic == BASIC_SHAN) return "闪";
    }
    return "";
}

/* 判断当前是否是玩家自己需要响应的状态 */
int input_is_player_response(GameState* game)
{
    if(!game) return 0;
    if(game->resp_state == RESPONSE_NEED_WUXIE)
    {
        /* 无懈可击：resp_target_player 是需要响应的玩家 */
        return (game->resp_target_player == 0);
    }
    if(game->resp_state == RESPONSE_NEED_BASIC)
    {
        /* 决斗状态：duel_turn 是当前需要出杀的玩家 */
        if(game->duel_turn != -1)
            return (game->duel_turn == 0);
        /* 普通基本牌响应：resp_target_player 是需要响应的玩家 */
        return (game->resp_target_player == 0);
    }
    if(game->resp_state == RESPONSE_NEED_MULTI_WUXIE)
    {
        /* 多目标锦囊无懈可击：当前询问的角色是玩家时 */
        if(game->multi_wuxie_stack_depth > 0)
        {
            int asker = game->multi_wuxie_stack[game->multi_wuxie_stack_depth - 1].asker_idx;
            return (asker == 0);
        }
    }
    return 0;
}



/* ===== 角色选择：检测鼠标点在哪个角色按钮上 ===== */
int input_hit_character_button(int mx, int my)
{
    int cols = 4, rows = 2;
    int btn_w = 180, btn_h = 90;
    int gap_x = 25, gap_y = 25;
    int expand_x = gap_x / 2;
    int expand_y = gap_y / 2;
    int total_w = cols * btn_w + (cols-1) * gap_x;
    int start_x = (WINDOW_WIDTH - total_w) / 2;
    int start_y = (WINDOW_HEIGHT - (rows*btn_h + (rows-1)*gap_y)) / 2 + 20;

    for(int i = 0; i < 8; i++)
    {
        int row = i / cols;
        int col = i % cols;
        int x = start_x + col * (btn_w + gap_x);
        int y = start_y + row * (btn_h + gap_y);
        int hit_x1 = x - (col > 0 ? expand_x : 0);
        int hit_y1 = y - (row > 0 ? expand_y : 0);
        int hit_x2 = x + btn_w + (col < cols-1 ? expand_x : 0);
        int hit_y2 = y + btn_h + (row < rows-1 ? expand_y : 0);
        if(mx >= hit_x1 && mx <= hit_x2 && my >= hit_y1 && my <= hit_y2)
            return i;
    }
    return -1;
}



/* ===== 圣骑士破灭护盾：检测点击牌名按钮 ===== */
int input_hit_pomie_card_button(int mx, int my)
{
    int card_count = paladin_pomie_get_card_count();
    int cols = 5;
    int rows = (card_count + cols - 1) / cols;
    int btn_w = 130;
    int btn_h = 50;
    int gap_x = 10;
    int gap_y = 10;
    int total_w = cols * btn_w + (cols - 1) * gap_x;
    int start_x = WINDOW_WIDTH / 2 - total_w / 2;
    int start_y = WINDOW_HEIGHT / 2 - (rows * btn_h + (rows - 1) * gap_y) / 2 - 30;

    for(int i = 0; i < card_count; i++)
    {
        int col = i % cols;
        int row = i / cols;
        int bx = start_x + col * (btn_w + gap_x);
        int by = start_y + row * (btn_h + gap_y);
        if(mx >= bx && mx <= bx + btn_w && my >= by && my <= by + btn_h)
            return i;
    }
    return -1;
}

/* ===== 圣骑士破灭护盾：检测点击确定按钮 ===== */
int input_hit_pomie_confirm_button(GameState* game, int mx, int my)
{
    if(!game || game->pomie_selected_card_name[0] == '\0') return 0;
    int card_count = paladin_pomie_get_card_count();
    int cols = 5;
    int rows = (card_count + cols - 1) / cols;
    int btn_w = 130;
    int btn_h = 50;
    int gap_x = 10;
    int gap_y = 10;
    int total_w = cols * btn_w + (cols - 1) * gap_x;
    int start_x = WINDOW_WIDTH / 2 - total_w / 2;
    int start_y = WINDOW_HEIGHT / 2 - (rows * btn_h + (rows - 1) * gap_y) / 2 - 30;

    int confirm_w = 100;
    int confirm_h = 40;
    int btn_gap = 30;
    int total_btn_w = confirm_w * 2 + btn_gap;
    int btn_start_x = WINDOW_WIDTH / 2 - total_btn_w / 2;
    int btn_y = start_y + rows * (btn_h + gap_y) + 20;

    return (mx >= btn_start_x && mx <= btn_start_x + confirm_w &&
            my >= btn_y && my <= btn_y + confirm_h);
}

/* ===== 圣骑士破灭护盾：检测点击取消按钮 ===== */
int input_hit_pomie_cancel_button(GameState* game, int mx, int my)
{
    if(!game) return 0;  /* 取消按钮始终可点，不需要先选中牌 */
    int card_count = paladin_pomie_get_card_count();
    int cols = 5;
    int rows = (card_count + cols - 1) / cols;
    int btn_w = 130;
    int btn_h = 50;
    int gap_x = 10;
    int gap_y = 10;
    int total_w = cols * btn_w + (cols - 1) * gap_x;
    int start_x = WINDOW_WIDTH / 2 - total_w / 2;
    int start_y = WINDOW_HEIGHT / 2 - (rows * btn_h + (rows - 1) * gap_y) / 2 - 30;

    int confirm_w = 100;
    int confirm_h = 40;
    int btn_gap = 30;
    int total_btn_w = confirm_w * 2 + btn_gap;
    int btn_start_x = WINDOW_WIDTH / 2 - total_btn_w / 2;
    int btn_y = start_y + rows * (btn_h + gap_y) + 20;

    int cancel_x = btn_start_x + confirm_w + btn_gap;
    return (mx >= cancel_x && mx <= cancel_x + confirm_w &&
            my >= btn_y && my <= btn_y + confirm_h);
}



/* ================================================================
 * 鼠标事件处理
 * 火攻两阶段接入点（都在普通出牌之前，命中后 return）：
 *   SHOW 状态：左键点手牌 → input_handle_huogong_show
 *   PICK 状态：左键点手牌 → input_handle_huogong_pick
 *              右键       → input_handle_huogong_cancel
 * 选目标接入点：
 *   TARGET 状态：左键点角色 → game_select_target
 *                左键点手牌 → 切换杀 或 取消后普通出牌
 *                右键/ESC   → game_cancel_target_select
 * ================================================================ */
void input_handle_event(InputState* state, GameState* game,
                        RenderContext* ctx, SDL_Event* e)
{
    if (!state || !game || !e) return;
    switch (e->type) {
    case SDL_MOUSEMOTION:
        state->mouse_x = e->motion.x;
        state->mouse_y = e->motion.y;

        /* 角色选择阶段：更新悬停 */
        if(game->phase == PHASE_CHARACTER_SELECT)
        {
            game->select_hover = input_hit_character_button(e->motion.x, e->motion.y);
        }

        /* 同步到 game 状态，用于悬停高亮 */
        if(game) {
            game->mouse_x = e->motion.x;
            game->mouse_y = e->motion.y;
            /* 飞舞拖拽：更新拖拽位置 */
            if(game->feiwuu_dragging)
            {
                game_feiwuu_update_drag(game, e->motion.x, e->motion.y);
            }
            /* 日志弹窗拖动：更新滚动位置 */
            if(state->log_dragging && game->log_panel_open)
            {
                int delta_y = e->motion.y - state->log_drag_start_y;
                /* 向上拖动（鼠标Y减小）→ 查看更早的日志 → scroll增加 */
                int new_scroll = state->log_drag_start_scroll + delta_y / 3;
                if(new_scroll < 0) new_scroll = 0;
                /* 可见行数约24行（content_h=440, line_h=18） */
                int visible_lines = 440 / 18;
                int max_scroll = game->log_count - visible_lines;
                if(max_scroll < 0) max_scroll = 0;
                if(new_scroll > max_scroll) new_scroll = max_scroll;
                game->log_scroll = new_scroll;
            }
        }
        break;

    case SDL_MOUSEBUTTONDOWN:
        /* 记录鼠标按下时间和位置（用于长按检测） */
        state->mouse_down_time = SDL_GetTicks();
        state->mouse_down_x = e->button.x;
        state->mouse_down_y = e->button.y;

        /* 日志弹窗：点击外部关闭，点击内容区域开始拖动 */
        if(game->log_panel_open)
        {
            int panel_w = 800;
            int panel_h = 500;
            int panel_x = (WINDOW_WIDTH - panel_w) / 2;
            int panel_y = (WINDOW_HEIGHT - panel_h) / 2;
            int content_y = panel_y + 50;
            int content_h = panel_h - 60;

            if(e->button.x < panel_x || e->button.x > panel_x + panel_w ||
               e->button.y < panel_y || e->button.y > panel_y + panel_h)
            {
                /* 点击外部：关闭弹窗 */
                game->log_panel_open = 0;
                state->log_dragging = 0;
                return;
            }

            /* 左键点击内容区域：开始拖动 */
            if(e->button.button == SDL_BUTTON_LEFT &&
               e->button.y >= content_y && e->button.y <= content_y + content_h)
            {
                state->log_dragging = 1;
                state->log_drag_start_y = e->button.y;
                state->log_drag_start_scroll = game->log_scroll;
                return;
            }

            /* 点击在弹窗内其他区域（标题栏等），不做任何操作 */
            return;
        }

        /* 日志按钮点击 */
        if(e->button.button == SDL_BUTTON_LEFT)
        {
            int btn_w = 60;
            int btn_h = 22;
            int btn_x = 120;
            int btn_y = WINDOW_HEIGHT - 182;
            if(e->button.x >= btn_x && e->button.x <= btn_x + btn_w &&
               e->button.y >= btn_y && e->button.y <= btn_y + btn_h)
            {
                game->log_panel_open = !game->log_panel_open;
                if(game->log_panel_open)
                {
                    /* 打开时滚动到最新日志 */
                    game->log_scroll = 0;
                }
                return;
            }
        }

        /* 如果技能描述正在显示，点击描述框之外的区域就关闭 */
        if(game->long_press_skill_idx >= 0)
        {
            int box_w = 600;
            int box_h = 280;
            int box_x = WINDOW_WIDTH / 2 - box_w / 2;
            int box_y = WINDOW_HEIGHT / 2 - box_h / 2;
            /* 点击在描述框之外，关闭描述 */
            if(e->button.x < box_x || e->button.x > box_x + box_w ||
               e->button.y < box_y || e->button.y > box_y + box_h)
            {
                game->long_press_skill_idx = -1;
                state->long_press_skill_idx = -1;
                state->mouse_down_time = 0;  /* 重置长按状态 */
                return;
            }
            /* 点击在描述框内，不做任何事 */
            state->mouse_down_time = 0;  /* 重置长按状态，避免触发新的长按 */
            return;
        }

        state->long_press_skill_idx = -1;

        /* 右键：火攻 PICK 放弃 / 选目标取消 / 弃牌快速弃完 / 贯石斧取消 */
        if (e->button.button == SDL_BUTTON_RIGHT) {
            if (game->resp_state == RESPONSE_NEED_HUOGONG_PICK) {
                input_handle_huogong_cancel(game);
            } else if (game->resp_state == RESPONSE_NEED_TARGET) {
                game_cancel_target_select(game);
            } else if (game->resp_state == RESPONSE_NEED_GUANSHI) {
                /* 贯石斧：右键点击贯石斧取消发动 */
                if(input_hit_guanshi(game, e->button.x, e->button.y)) {
                    game_guanshi_cancel(game);
                }
            } else if (game->resp_state == RESPONSE_NEED_GENERIC_DISCARD && game->guanshi_active) {
                /* 贯石斧通用弃牌阶段：右键取消贯石斧（同时取消弃牌） */
                game_guanshi_cancel(game);
            } else if (game->resp_state == RESPONSE_NEED_HANBING) {
                /* 寒冰剑：右键点击寒冰剑取消发动，正常造成伤害 */
                if(input_hit_hanbing(game, e->button.x, e->button.y)) {
                    game_hanbing_cancel(game);
                }
            } else if (game->resp_state == RESPONSE_NEED_PICK_ENEMY_CARD) {
                /* 过河拆桥/顺手牵羊：右键取消选择 */
                game_cancel_pick_enemy_card(game);
            } else if (game->resp_state == RESPONSE_NEED_ZHUQUE) {
                /* 朱雀羽扇：右键取消发动，普通杀继续结算 */
                game_zhuque_cancel(game);
            } else if (game->resp_state == RESPONSE_NEED_ZHANGBA) {
                /* 丈八蛇矛：右键取消发动，退出选牌模式 */
                game_zhangba_cancel(game);
            } else if (game->resp_state == RESPONSE_NEED_FEIWUU_PICK) {
                /* 雨蝶飞舞：右键取消选择 */
                game_feiwuu_cancel(game);
            } else if (game->resp_state == RESPONSE_NEED_GENERIC_DISCARD) {
                /* 通用主动弃牌：右键取消（弃牌阶段source=201不能取消） */
                if(game->generic_discard_source != 201)
                    game_generic_discard_cancel(game);
            } else if (game->resp_state == RESPONSE_NEED_CONFIRM_PLAY) {
                /* 确认出牌：右键取消 */
                game_cancel_confirm_play(game);
            } else if (game->resp_state == RESPONSE_NEED_TIESUO_TARGET) {
                /* 铁索连环选目标：右键取消 */
                game_tiesuo_cancel(game);
            } else if (game->resp_state == RESPONSE_NEED_LINYUXIA_YUZHAN) {
                /* 林雨霞玉盏：右键取消发动 */
                linyuxia_yuzhan_cancel(game);
            } else if (game->resp_state == RESPONSE_NEED_FEIWUU_DRAG) {
                /* 飞舞拖拽：右键取消拖拽放置 */
                game_feiwuu_drag_cancel(game);
            } else if (game->resp_state == RESPONSE_NEED_GILGAMESH_SUOJIAN ||
                       game->resp_state == RESPONSE_NEED_GILGAMESH_GUAILI ||
                       game->resp_state == RESPONSE_NEED_GILGAMESH_TIANPI_TARGET ||
                       game->resp_state == RESPONSE_NEED_GILGAMESH_TIANPI_RANK) {
                /* 吉尔伽美什技能选择：右键取消 */
                game_gilgamesh_cancel(game);
            } else if (game->resp_state == RESPONSE_NEED_BAGUA) {
                /* 八卦阵：右键取消发动，继续结算杀 */
                game_bagua_cancel(game);
            }
            return;
        }

        if (e->button.button == SDL_BUTTON_LEFT) {



                    /* ===== 角色选择阶段：第一下选中发亮，第二下确定选择 ===== */
            if(game->phase == PHASE_CHARACTER_SELECT)
            {
                int idx = input_hit_character_button(e->button.x, e->button.y);
                if(idx >= 0)
                {
                    if(game->selected_hero_idx == idx)
                    {
                        /* 第二下点击同一个角色：确定选择 */
                        int ai_idx;
                        do {
                            ai_idx = rand() % 8;
                        } while(ai_idx == idx);

                        game_log(game, "你选择了 %s，AI选择了 %s",
                                 hero_get(idx)->name, hero_get(ai_idx)->name);
                        game_start_with_heroes(game, idx, ai_idx);
                        game->selected_hero_idx = -1;
                    }
                    else
                    {
                        /* 第一下点击：选中该角色，发亮 */
                        game->selected_hero_idx = idx;
                        game_log(game, "选中 %s，再次点击确定选择", hero_get(idx)->name);
                    }
                }
                /* 点击空白处：不取消选中（避免误触），保持当前选中状态 */
                return;  /* 选择阶段不处理其他点击 */
            }






            /* 圣骑士神圣护盾：选择2*2选项（最优先处理） */
            if(game->resp_state == RESPONSE_NEED_PALADIN_CHOICE)
            {
                int opt = input_hit_paladin_option(e->button.x, e->button.y);
                if(opt > 0)
                {
                    if(paladin_option_available(game, opt))
                    {
                        game_log(game, "【神圣护盾】你选择了选项%d", opt);
                        paladin_choose_option(game, opt);
                    }
                    else
                    {
                        game_log(game, "【神圣护盾】选项%d当前不可用（盾量不足或已满）", opt);
                    }
                }
                return;  /* 点空白处不做任何事，必须选一个 */
            }

            /* 镜流古镜照神：选择选项1/选项2/取消 */
            if(game->resp_state == RESPONSE_NEED_JINGLIU_GUJING)
            {
                int opt = input_hit_jingliu_gujing_option(e->button.x, e->button.y);
                if(opt == 1 || opt == 2){
                    jingliu_gujing_choose_option(game, 0, opt);
                }else if(opt == 3){
                    jingliu_gujing_cancel(game);
                }
                return;
            }

            /* 圣骑士破灭护盾：选择牌名 */
            if(game->resp_state == RESPONSE_NEED_PALADIN_POMIE_CARD)
            {
                /* 点击牌名按钮 */
                int card_idx = input_hit_pomie_card_button(e->button.x, e->button.y);
                if(card_idx >= 0)
                {
                    paladin_pomie_select_card(game, card_idx);
                    return;
                }

                /* 点击确定按钮 */
                if(input_hit_pomie_confirm_button(game, e->button.x, e->button.y))
                {
                    paladin_pomie_confirm(game);
                    return;
                }

                /* 点击取消按钮 */
                if(input_hit_pomie_cancel_button(game, e->button.x, e->button.y))
                {
                    paladin_pomie_cancel(game);
                    return;
                }

                return;  /* 点空白处不做任何事 */
            }

            /* 化形①：选择花色 */
            if(game->resp_state == RESPONSE_NEED_HUAXING_SUIT)
            {
                int suit = input_hit_huaxing_suit(e->button.x, e->button.y);
                if(suit >= 0 && suit < 4)
                {
                    Player* me = &game->players[0];
                    /* 检查是否可用 */
                    int available = 0;
                    for(int j = 0; j < me->hand_count; j++)
                    {
                        if(me->hand[j] && me->hand[j]->suit == suit) { available = 1; break; }
                    }
                    if(available && !(me->yudie.huaxing_used_suits & (1 << suit)))
                    {
                        game_log(game, "【化形】选择花色%d", suit);
                        yudie_huaxing_pick_suit(game, 0, suit);
                    }
                    else
                    {
                        game_log(game, "【化形】该花色不可用");
                    }
                }
                else if(suit == -2)  /* 结束按钮 */
                {
                    game_log(game, "【化形】结束化形①");
                    yudie_huaxing_end(game, 0);
                }
                return;
            }

            /* 化形①：选择手牌 */
            if(game->resp_state == RESPONSE_NEED_HUAXING_HAND)
            {
                int hand_idx = input_hit_hand_card(game, ctx, e->button.x, e->button.y);
                if(hand_idx >= 0)
                {
                    yudie_huaxing_pick_hand(game, 0, hand_idx);
                }
                /* 取消按钮 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    game->resp_state = RESPONSE_NEED_HUAXING_SUIT;
                    game_log(game, "【化形】返回到选花色");
                }
                return;
            }

            /* 化形①：选择锦囊牌名 */
            if(game->resp_state == RESPONSE_NEED_HUAXING_TRICK)
            {
                int trick_idx = input_hit_huaxing_trick(game, e->button.x, e->button.y);
                if(trick_idx >= 0)
                {
                    game_log(game, "【化形】选择锦囊%d", trick_idx);
                    yudie_huaxing_pick_trick(game, 0, trick_idx);
                }
                /* 取消按钮 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    game->resp_state = RESPONSE_NEED_HUAXING_HAND;
                    game_log(game, "【化形】返回到选手牌");
                }
                return;
            }

            /* 化形②：选择目标 */
            if(game->resp_state == RESPONSE_NEED_HUAXING_TARGET)
            {
                int target = input_hit_player(game, e->button.x, e->button.y);
                if(target >= 0 && game->players[target].alive)
                {
                    game_log(game, "【化形②】指定目标%d", target);
                    yudie_huaxing_set_target(game, 0, target);
                }
                /* 取消按钮 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    game->resp_state = RESPONSE_NONE;
                    game_log(game, "【化形②】取消指定目标");
                }
                return;
            }

            /* 主动技能点击：自己回合无响应时，或自己需要响应基本牌/无懈可击时（龙胆、破灭护盾等） */
            if((game->resp_state == RESPONSE_NONE && game->current_player == 0) ||
               (game->resp_state == RESPONSE_NEED_BASIC && game->resp_target_player == 0) ||
               (game->resp_state == RESPONSE_NEED_WUXIE && game->resp_target_player == 0))
            {
                int skill_idx = input_hit_skill(game, e->button.x, e->button.y);
                if(skill_idx >= 0)
                {
                    Player* me = &game->players[0];
                    if(me->hero && me->hero->use_skill)
                    {
                        if(hero_skill_can_use(game, 0, skill_idx))
                        {
                            game_log(game, "点击技能【%s】", me->hero->skills[skill_idx].name);
                            me->hero->use_skill(game, 0, skill_idx);
                        }
                        else
                        {
                            game_log(game, "【%s】当前无法使用（阶段不符或次数已用完）",
                                     me->hero->skills[skill_idx].name);
                        }
                    }
                    return;
                }
            }

            /* 龙胆模式：出牌阶段点击取消按钮退出龙胆模式 */
            {
                Player* me = &game->players[0];
                if(me->hero_id == HERO_ZHAOYUN && me->longdan_active &&
                   game->resp_state == RESPONSE_NONE && game->current_player == 0)
                {
                    if(input_hit_shan_button(e->button.x, e->button.y))
                    {
                        me->longdan_active = 0;
                        game_log(game, "%s 取消【龙胆】模式", me->name);
                        return;
                    }
                }
            }

            /* 五谷丰登选牌：优先处理 */
            if(game->resp_state == RESPONSE_NEED_WUGU_PICK)
            {
                int count = game->group_wugu_count;
                int start_x = WINDOW_WIDTH/2 - (count*(CARD_WIDTH+10))/2;
                int wugu_y = 140;
                for(int i=0;i<count;i++)
                {
                    int cx = start_x + i*(CARD_WIDTH+10);
                    if(e->button.x >= cx && e->button.x <= cx+CARD_WIDTH &&
                       e->button.y >= wugu_y && e->button.y <= wugu_y+CARD_HEIGHT)
                    {
                        Card* picked = game->group_wugu_pile[i];
                        for(int s=i;s<count-1;s++)
                            game->group_wugu_pile[s] = game->group_wugu_pile[s+1];
                        game->group_wugu_count--;
                        player_draw_card(&game->players[0], picked);
                        game_log(game, "你选择了【%s】", card_get_full_name(picked));
                        /* 取消倒计时 */
                        game->countdown.active = 0;
                        game->countdown.remaining = 0;
                        game->resp_state = RESPONSE_NONE;
                        /* 推进到下一个人 */
                        game_group_advance(game);
                        return;
                    }
                }
                return; /* 点空白处不做任何事 */
            }

            /* 选目标状态：先检测是否点击了取消按钮，再检测是否点击了角色 */
            if(game->resp_state == RESPONSE_NEED_TARGET)
            {
                /* 点击取消按钮 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    game_cancel_target_select(game);
                    return;
                }

                int player_idx = input_hit_player(game, e->button.x, e->button.y);
                if(player_idx >= 0) {
                    game_select_target(game, player_idx);
                    state->selected_hand_index = -1;
                    return;
                }
                /* 没点中角色，继续检测是否点中了手牌 */
            }

            /* 多目标选择状态：点击头像切换选中，点击确定确认，点击取消取消 */
            if(game->resp_state == RESPONSE_NEED_MULTI_TARGET)
            {
                /* 点击确定按钮 */
                if(input_hit_confirm_button(e->button.x, e->button.y))
                {
                    game_multi_target_confirm(game);
                    return;
                }
                /* 点击取消按钮 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    game_multi_target_cancel(game);
                    return;
                }
                /* 点击头像切换选中 */
                int player_idx = input_hit_player(game, e->button.x, e->button.y);
                if(player_idx >= 0) {
                    game_multi_target_toggle(game, player_idx);
                    return;
                }
                return; /* 点空白处不做任何事 */
            }

            /* 玉盏目标调整选择：点击三个选项按钮 */
            if(game->resp_state == RESPONSE_NEED_YUZHAN_TARGET)
            {
                int cx = WINDOW_WIDTH / 2;
                int cy = WINDOW_HEIGHT / 2;
                int btn_w = 160;
                int btn_h = 50;
                int gap = 15;

                /* 按钮1：增加目标 */
                SDL_Rect btn1 = {cx - btn_w*1.5 - gap, cy, btn_w, btn_h};
                /* 按钮2：减少目标 */
                SDL_Rect btn2 = {cx - btn_w/2, cy, btn_w, btn_h};
                /* 按钮3：不调整 */
                SDL_Rect btn3 = {cx + btn_w/2 + gap, cy, btn_w, btn_h};
                /* 取消按钮 */
                SDL_Rect btn_cancel = {cx - 60, cy + 80, 120, 40};

                if(e->button.x >= btn1.x && e->button.x <= btn1.x + btn1.w &&
                   e->button.y >= btn1.y && e->button.y <= btn1.y + btn1.h)
                {
                    game_yuzhan_target_choose(game, 0);
                    return;
                }
                if(e->button.x >= btn2.x && e->button.x <= btn2.x + btn2.w &&
                   e->button.y >= btn2.y && e->button.y <= btn2.y + btn2.h)
                {
                    game_yuzhan_target_choose(game, 1);
                    return;
                }
                if(e->button.x >= btn3.x && e->button.x <= btn3.x + btn3.w &&
                   e->button.y >= btn3.y && e->button.y <= btn3.y + btn3.h)
                {
                    game_yuzhan_target_choose(game, 2);
                    return;
                }
                if(e->button.x >= btn_cancel.x && e->button.x <= btn_cancel.x + btn_cancel.w &&
                   e->button.y >= btn_cancel.y && e->button.y <= btn_cancel.y + btn_cancel.h)
                {
                    game_yuzhan_target_cancel(game);
                    return;
                }
                return;
            }

            /* 流萤迸发：选择火属性/雷属性 */
            if(game->resp_state == RESPONSE_NEED_LIUYING_BENGFA)
            {
                int cx = WINDOW_WIDTH / 2;
                int cy = WINDOW_HEIGHT / 2;
                int btn_w = 160;
                int btn_h = 60;
                int gap = 30;

                /* 火属性按钮 */
                SDL_Rect btn1 = {cx - btn_w - gap/2, cy, btn_w, btn_h};
                /* 雷属性按钮 */
                SDL_Rect btn2 = {cx + gap/2, cy, btn_w, btn_h};
                /* 取消按钮 */
                SDL_Rect btn_cancel = {cx - 60, cy + 90, 120, 40};

                if(e->button.x >= btn1.x && e->button.x <= btn1.x + btn1.w &&
                   e->button.y >= btn1.y && e->button.y <= btn1.y + btn1.h)
                {
                    liuying_bengfa_confirm(game, 0, 1);  /* 1=火属性 */
                    return;
                }
                if(e->button.x >= btn2.x && e->button.x <= btn2.x + btn2.w &&
                   e->button.y >= btn2.y && e->button.y <= btn2.y + btn2.h)
                {
                    liuying_bengfa_confirm(game, 0, 2);  /* 2=雷属性 */
                    return;
                }
                if(e->button.x >= btn_cancel.x && e->button.x <= btn_cancel.x + btn_cancel.w &&
                   e->button.y >= btn_cancel.y && e->button.y <= btn_cancel.y + btn_cancel.h)
                {
                    liuying_bengfa_cancel(game);
                    return;
                }
                return;
            }

            /* 登仙牌型转换选择：点击三个选项按钮 */
            if(game->resp_state == RESPONSE_NEED_DENGXIAN_CONVERT)
            {
                int cx = WINDOW_WIDTH / 2;
                int cy = WINDOW_HEIGHT / 2;
                int btn_w = 180;
                int btn_h = 50;
                int gap = 20;

                /* 按钮1：当原牌 */
                SDL_Rect btn1 = {cx - btn_w - gap/2, cy, btn_w, btn_h};
                /* 按钮2：当桃 */
                SDL_Rect btn2 = {cx - btn_w/2, cy, btn_w, btn_h};
                /* 按钮3：当桃园结义 */
                SDL_Rect btn3 = {cx + gap/2, cy, btn_w, btn_h};
                /* 取消按钮 */
                SDL_Rect btn_cancel = {cx - 60, cy + 80, 120, 40};

                if(e->button.x >= btn1.x && e->button.x <= btn1.x + btn1.w &&
                   e->button.y >= btn1.y && e->button.y <= btn1.y + btn1.h)
                {
                    game_dengxian_convert_choose(game, 0);
                    return;
                }
                if(e->button.x >= btn2.x && e->button.x <= btn2.x + btn2.w &&
                   e->button.y >= btn2.y && e->button.y <= btn2.y + btn2.h)
                {
                    game_dengxian_convert_choose(game, 1);
                    return;
                }
                if(e->button.x >= btn3.x && e->button.x <= btn3.x + btn3.w &&
                   e->button.y >= btn3.y && e->button.y <= btn3.y + btn3.h)
                {
                    game_dengxian_convert_choose(game, 2);
                    return;
                }
                if(e->button.x >= btn_cancel.x && e->button.x <= btn_cancel.x + btn_cancel.w &&
                   e->button.y >= btn_cancel.y && e->button.y <= btn_cancel.y + btn_cancel.h)
                {
                    game_dengxian_convert_cancel(game);
                    return;
                }
                return; /* 点空白处不做任何事 */
            }

            /* 贯石斧状态：左键点击贯石斧进入选牌阶段 */
            if(game->resp_state == RESPONSE_NEED_GUANSHI)
            {
                if(input_hit_guanshi(game, e->button.x, e->button.y)) {
                    game_guanshi_click_weapon(game);
                    return;
                }
            }

            /* 寒冰剑状态：左键点击寒冰剑进入选牌阶段 */
            if(game->resp_state == RESPONSE_NEED_HANBING)
            {
                if(input_hit_hanbing(game, e->button.x, e->button.y)) {
                    game_hanbing_click_weapon(game);
                    return;
                }
                /* 寒冰剑选牌阶段：左键点击对方手牌 */
                if(game->hanbing_picking)
                {
                    int enemy_idx = input_hit_enemy_hand_card(game, e->button.x, e->button.y);
                    if(enemy_idx >= 0) {
                        game_hanbing_pick_card(game, 0, enemy_idx);
                        return;
                    }
                    /* 寒冰剑选牌阶段：左键点击对方装备区的牌 */
                    int equip_type = 0;
                    if(input_hit_enemy_equip(game, e->button.x, e->button.y, &equip_type)) {
                        game_hanbing_pick_card(game, equip_type, 0);
                        return;
                    }
                }
            }

            /* 过河拆桥/顺手牵羊：选择对方的一张牌 */
            if(game->resp_state == RESPONSE_NEED_PICK_ENEMY_CARD)
            {
                /* 点击对方手牌 */
                int enemy_hand_idx = input_hit_enemy_hand_card(game, e->button.x, e->button.y);
                if(enemy_hand_idx >= 0)
                {
                    /* 如果已选中同一张牌，确认；否则选中 */
                    if(game->pick_enemy_card_type == 0 && game->pick_enemy_card_index == enemy_hand_idx)
                    {
                        game_confirm_pick_enemy_card(game);
                    }
                    else
                    {
                        game_pick_enemy_card(game, 0, enemy_hand_idx);
                    }
                    return;
                }

                /* 点击对方装备区 */
                int equip_type = 0;
                if(input_hit_enemy_equip(game, e->button.x, e->button.y, &equip_type))
                {
                    /* 如果已选中同一张牌，确认；否则选中 */
                    if(game->pick_enemy_card_type == equip_type)
                    {
                        game_confirm_pick_enemy_card(game);
                    }
                    else
                    {
                        game_pick_enemy_card(game, equip_type, 0);
                    }
                    return;
                }

                /* 点击对方延时锦囊区 */
                int judge_idx = -1;
                if(input_hit_enemy_judge(game, e->button.x, e->button.y, &judge_idx))
                {
                    /* 如果已选中同一张牌，确认；否则选中 */
                    if(game->pick_enemy_card_type == 5 && game->pick_enemy_card_index == judge_idx)
                    {
                        game_confirm_pick_enemy_card(game);
                    }
                    else
                    {
                        game_pick_enemy_card(game, 5, judge_idx);
                    }
                    return;
                }

                return; /* 点空白处不做任何事 */
            }

            /* 朱雀羽扇：打出杀时，选择是否将杀变成火杀 */
            if(game->resp_state == RESPONSE_NEED_ZHUQUE)
            {
                /* 点击朱雀羽扇 = 选中 */
                if(input_hit_zhuque(game, e->button.x, e->button.y))
                {
                    game_zhuque_click_weapon(game);
                    return;
                }

                /* 点击确定按钮 */
                if(input_hit_confirm_button(e->button.x, e->button.y))
                {
                    if(game->zhuque_selected == 1)
                    {
                        /* 已选中：点击"确定" = 发动，杀变成火杀 */
                        game_zhuque_confirm(game);
                    }
                    return;
                }

                /* 点击取消按钮 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    if(game->zhuque_selected == 1)
                    {
                        /* 已选中：点击"取消" = 取消选中 */
                        game->zhuque_selected = 0;
                        game_log(game, "【朱雀羽扇】取消选中");
                    }
                    else
                    {
                        /* 未选中：点击"取消" = 不发动，普通杀继续结算 */
                        game_zhuque_cancel(game);
                    }
                    return;
                }

                return; /* 点空白处不做任何事 */
            }

            /* 丈八蛇矛：选两张手牌当杀打出 */
            if(game->resp_state == RESPONSE_NEED_ZHANGBA)
            {
                /* 点击手牌：选中/取消选中 */
                int hand_idx = input_hit_hand_card(game, ctx, e->button.x, e->button.y);
                if(hand_idx >= 0)
                {
                    game_zhangba_pick_card(game, hand_idx);
                    return;
                }

                /* 点击确定按钮 */
                if(input_hit_confirm_button(e->button.x, e->button.y))
                {
                    if(game->zhangba_selected_count == 2)
                    {
                        /* 已选中2张：点击"确定" = 把两张牌当杀打出 */
                        game_zhangba_confirm(game);
                    }
                    return;
                }

                /* 点击取消按钮 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    /* 点击"取消" = 退出选牌模式 */
                    game_zhangba_cancel(game);
                    return;
                }

                return; /* 点空白处不做任何事 */
            }

            /* 雨蝶飞舞：选择要置入装备区的手牌（0-4张） */
            if(game->resp_state == RESPONSE_NEED_FEIWUU_PICK)
            {
                /* 点击手牌：选中/取消选中 */
                int hand_idx = input_hit_hand_card(game, ctx, e->button.x, e->button.y);
                if(hand_idx >= 0)
                {
                    game_feiwuu_pick_card(game, hand_idx);
                    return;
                }

                /* 点击确定按钮：确认选择 */
                if(input_hit_confirm_button(e->button.x, e->button.y))
                {
                    game_feiwuu_confirm(game);
                    return;
                }

                /* 点击取消按钮：取消选择 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    game_feiwuu_cancel(game);
                    return;
                }

                return; /* 点空白处不做任何事 */
            }

            /* 雨蝶飞舞拖拽：点击待放置的牌开始拖拽 */
            if(game->resp_state == RESPONSE_NEED_FEIWUU_DRAG)
            {
                /* 待放置的牌显示在屏幕中间下方，横向排列 */
                int card_w = CARD_WIDTH;
                int card_h = CARD_HEIGHT;
                int total_w = game->feiwuu_drag_count * (card_w + 10);
                int start_x = WINDOW_WIDTH / 2 - total_w / 2;
                int start_y = WINDOW_HEIGHT / 2 + 150;

                for(int i = 0; i < game->feiwuu_drag_count; i++)
                {
                    if(!game->feiwuu_drag_cards[i]) continue;
                    int cx = start_x + i * (card_w + 10);
                    if(e->button.x >= cx && e->button.x <= cx + card_w &&
                       e->button.y >= start_y && e->button.y <= start_y + card_h)
                    {
                        game_feiwuu_begin_drag(game, i, e->button.x, e->button.y);
                        return;
                    }
                }
                return; /* 点空白处不做任何事 */
            }

            /* 通用主动弃牌：选择要弃置的手牌 */
            if(game->resp_state == RESPONSE_NEED_GENERIC_DISCARD)
            {
                /* 点击手牌：选中/取消选中 */
                int hand_idx = input_hit_hand_card(game, ctx, e->button.x, e->button.y);
                if(hand_idx >= 0)
                {
                    game_generic_discard_pick(game, hand_idx);
                    return;
                }

                /* 点击确定按钮 */
                if(input_hit_confirm_button(e->button.x, e->button.y))
                {
                    if(game->generic_discard_selected_count == game->generic_discard_need)
                    {
                        /* 已选满：点击"确定" = 弃牌 */
                        game_generic_discard_confirm(game);
                    }
                    return;
                }

                /* 点击取消按钮 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    /* 弃牌阶段（source=201）不能取消，必须弃牌 */
                    if(game->generic_discard_source != 201)
                    {
                        game_generic_discard_cancel(game);
                    }
                    return;
                }

                return; /* 点空白处不做任何事 */
            }

            /* 确认出牌：点击确定打出牌，点击取消取消出牌 */
            if(game->resp_state == RESPONSE_NEED_CONFIRM_PLAY)
            {
                /* 点击确定按钮 → 打出牌 */
                if(input_hit_confirm_button(e->button.x, e->button.y))
                {
                    game_confirm_play(game);
                    return;
                }

                /* 点击取消按钮 → 取消出牌 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    game_cancel_confirm_play(game);
                    return;
                }

                return; /* 点空白处不做任何事 */
            }

            /* 火攻选弃牌阶段：点击确定确认弃置，点击取消放弃火攻 */
            if(game->resp_state == RESPONSE_NEED_HUOGONG_PICK)
            {
                /* 点击确定按钮 → 确认弃置并造成伤害 */
                if(input_hit_confirm_button(e->button.x, e->button.y))
                {
                    if(game->huogong_picked_hand >= 0)
                    {
                        input_handle_huogong_confirm(game);
                    }
                    return;
                }

                /* 点击取消按钮 → 放弃火攻 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    input_handle_huogong_cancel(game);
                    return;
                }
                /* 点击手牌 → 选择弃置的牌（在后面的通用手牌处理里） */
            }

            /* 林雨霞玉盏：确认是否发动 */
            if(game->resp_state == RESPONSE_NEED_LINYUXIA_YUZHAN)
            {
                /* 点击确定按钮 → 发动玉盏 */
                if(input_hit_confirm_button(e->button.x, e->button.y))
                {
                    linyuxia_yuzhan_confirm(game);
                    return;
                }

                /* 点击取消按钮 → 取消发动 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    linyuxia_yuzhan_cancel(game);
                    return;
                }

                return; /* 点空白处不做任何事 */
            }

            /* 铁索连环选目标：点击头像切换选中，点击确定确认，点击重铸重铸 */
            if(game->resp_state == RESPONSE_NEED_TIESUO_TARGET)
            {
                /* 点击重铸按钮 → 重铸铁索连环 */
                if(input_hit_chongzhu_button(e->button.x, e->button.y))
                {
                    game_tiesuo_chongzhu(game);
                    return;
                }

                /* 点击确定按钮 → 确认选择（至少选1人） */
                if(input_hit_confirm_button(e->button.x, e->button.y))
                {
                    if(game->tiesuo_target_count > 0)
                    {
                        game_tiesuo_confirm(game);
                    }
                    return;
                }

                /* 点击角色头像 → 切换选中状态 */
                int player_idx = input_hit_player(game, e->button.x, e->button.y);
                if(player_idx >= 0)
                {
                    game_tiesuo_toggle_target(game, player_idx);
                    return;
                }

                return; /* 点空白处不做任何事 */
            }

            /* 吉尔伽美什·所见：2*2选择牌类型 */
            if(game->resp_state == RESPONSE_NEED_GILGAMESH_SUOJIAN)
            {
                int btn_w = 220, btn_h = 90, gap = 20;
                int total_w = btn_w * 2 + gap;
                int start_x = WINDOW_WIDTH / 2 - total_w / 2;
                int start_y = WINDOW_HEIGHT / 2 - btn_h - gap / 2;

                for(int i = 0; i < 4; i++)
                {
                    int col = i % 2, row = i / 2;
                    int bx = start_x + col * (btn_w + gap);
                    int by = start_y + row * (btn_h + gap);
                    if(e->button.x >= bx && e->button.x <= bx + btn_w &&
                       e->button.y >= by && e->button.y <= by + btn_h)
                    {
                        game_gilgamesh_select_type(game, i);
                        return;
                    }
                }
                return; /* 点空白处不做任何事 */
            }

            /* 吉尔伽美什·乖离：2*2选择花色 */
            if(game->resp_state == RESPONSE_NEED_GILGAMESH_GUAILI)
            {
                int btn_w = 220, btn_h = 90, gap = 20;
                int total_w = btn_w * 2 + gap;
                int start_x = WINDOW_WIDTH / 2 - total_w / 2;
                int start_y = WINDOW_HEIGHT / 2 - btn_h - gap / 2;

                for(int i = 0; i < 4; i++)
                {
                    int col = i % 2, row = i / 2;
                    int bx = start_x + col * (btn_w + gap);
                    int by = start_y + row * (btn_h + gap);
                    if(e->button.x >= bx && e->button.x <= bx + btn_w &&
                       e->button.y >= by && e->button.y <= by + btn_h)
                    {
                        game_gilgamesh_select_suit(game, i);
                        return;
                    }
                }
                return; /* 点空白处不做任何事 */
            }

            /* 吉尔伽美什·天辟：选择目标角色 */
            if(game->resp_state == RESPONSE_NEED_GILGAMESH_TIANPI_TARGET)
            {
                int player_idx = input_hit_player(game, e->button.x, e->button.y);
                if(player_idx >= 0)
                {
                    game_gilgamesh_select_tianpi_target(game, player_idx);
                }
                return;
            }

            /* 吉尔伽美什·天辟：点击手牌选择点数 */
            if(game->resp_state == RESPONSE_NEED_GILGAMESH_TIANPI_RANK)
            {
                int idx = input_hit_hand_card(game, ctx, e->button.x, e->button.y);
                if(idx >= 0)
                {
                    game_gilgamesh_select_rank(game, idx);
                }
                return;
            }

            /* 八卦阵：需要出闪时，选择是否判定 */
            if(game->resp_state == RESPONSE_NEED_BAGUA)
            {
                /* 点击八卦阵 = 选中 */
                if(input_hit_bagua(game, e->button.x, e->button.y))
                {
                    game_bagua_click_armor(game);
                    return;
                }

                /* 点击确定按钮 */
                if(input_hit_confirm_button(e->button.x, e->button.y))
                {
                    if(game->bagua_selected == 1)
                    {
                        /* 已选中：点击"确定" = 发动，进行判定 */
                        game_bagua_confirm(game);
                    }
                    return;
                }

                /* 点击取消按钮 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    if(game->bagua_selected == 1)
                    {
                        /* 已选中：点击"取消" = 取消选中 */
                        game->bagua_selected = 0;
                        game_log(game, "【八卦阵】取消选中");
                    }
                    else
                    {
                        /* 未选中：点击"取消" = 不发动，继续结算杀 */
                        game_bagua_cancel(game);
                    }
                    return;
                }

                return; /* 点空白处不做任何事 */
            }

            /* 通用响应：鼠标点击交互（无懈可击/杀/闪，双按钮） */
            if(input_is_player_response(game))
            {
                /* 点击确定按钮 */
                if(input_hit_confirm_button(e->button.x, e->button.y))
                {
                    if(game->response_pick_selected == 1)
                    {
                        /* 已选中：点击"确定" = 打出响应牌 */
                        const char* card_name = input_get_response_card_name(game);
                        game_log(game, "你打出了【%s】", card_name);
                        game->response_pick_selected = 0;
                        if(game->resp_state == RESPONSE_NEED_WUXIE)
                            input_handle_wuxie_y(game);
                        else if(game->resp_state == RESPONSE_NEED_MULTI_WUXIE)
                            game_multi_wuxie_use(game, 0);
                        else
                            input_handle_response_y(game);
                    }
                    return;
                }

                /* 点击取消按钮 */
                if(input_hit_cancel_button(e->button.x, e->button.y))
                {
                    const char* card_name = input_get_response_card_name(game);
                    if(game->response_pick_selected == 1)
                    {
                        /* 已选中：点击"取消" = 取消选中 */
                        game->response_pick_selected = 0;
                        game->response_pick_index = -1;
                        game_log(game, "取消选中【%s】", card_name);
                    }
                    else
                    {
                        /* 未选中：点击"取消" = 不响应 */
                        game_log(game, "你选择不使用【%s】", card_name);
                        game->response_pick_selected = 0;
                        game->response_pick_index = -1;
                        if(game->resp_state == RESPONSE_NEED_WUXIE)
                            input_handle_wuxie_n(game);
                        else if(game->resp_state == RESPONSE_NEED_MULTI_WUXIE)
                            game_multi_wuxie_pass(game);
                        else
                            input_handle_response_n(game);
                    }
                    return;
                }
            }

            int idx = input_hit_hand_card(game, ctx, e->button.x, e->button.y);
            if (idx >= 0) {
                state->selected_hand_index = idx;

                /* 通用响应：未选中时点击响应牌 = 选中（记录索引） */
                if(input_is_player_response(game) && game->response_pick_selected == 0)
                {
                    Player* me = &game->players[0];
                    Card* card = me->hand[idx];
                    if(input_is_response_card(game, card))
                    {
                        const char* card_name = input_get_response_card_name(game);
                        game->response_pick_selected = 1;
                        game->response_pick_index = idx;
                        game_log(game, "选中【%s】，点击确认打出，或点击其他区域取消", card_name);
                        state->selected_hand_index = -1;
                        return;
                    }
                }
                /* 通用响应：已选中时点击其他响应牌 = 切换选中 */
                else if(input_is_player_response(game) && game->response_pick_selected == 1)
                {
                    Player* me = &game->players[0];
                    Card* card = me->hand[idx];
                    if(input_is_response_card(game, card) && idx != game->response_pick_index)
                    {
                        game->response_pick_index = idx;
                        game_log(game, "切换选中【%s】", card_get_full_name(card));
                        state->selected_hand_index = -1;
                        return;
                    }
                    /* 点击已选中的牌 = 取消选中 */
                    if(idx == game->response_pick_index)
                    {
                        game->response_pick_selected = 0;
                        game->response_pick_index = -1;
                        game_log(game, "取消选中");
                        state->selected_hand_index = -1;
                        return;
                    }
                }

                /* 火攻阶段1：目标(玩家)左键点击一张手牌展示 */
                if (game->resp_state == RESPONSE_NEED_HUOGONG_SHOW) {
                    input_handle_huogong_show(game, idx);
                    state->selected_hand_index = -1;
                    return;
                }

                /* 火攻阶段2：使用者(玩家)左键点击一张手牌弃置 */
                if (game->resp_state == RESPONSE_NEED_HUOGONG_PICK) {
                    input_handle_huogong_pick(game, idx);
                    state->selected_hand_index = -1;
                    return;
                }

                /* 选目标状态下点击手牌 */
                if (game->resp_state == RESPONSE_NEED_TARGET) {
                    Player* me = &game->players[0];
                    Card* card = me->hand[idx];
                    if(card && card_needs_target(card)) {
                        /* 切换待使用的牌 */
                        game->pending_hand_index = idx;
                        game->pending_card = card;
                        game_log(game, "切换为【%s】，请选择目标", card_get_full_name(card));
                    } else {
                        /* 取消选目标，继续往下走普通出牌逻辑 */
                        game_cancel_target_select(game);
                    }
                    state->selected_hand_index = -1;
                    /* 不 return，继续往下走普通出牌逻辑（取消后 resp_state==NONE） */
                }

                /* 普通出牌（仅 resp_state==NONE 时生效） */
                if (game->game_over == 0 && game->resp_state == RESPONSE_NONE &&
                    game->phase == PHASE_PLAY && game->current_player == 0)
                {
                    Player* me = &game->players[0];
                    Card* card = me->hand[idx];
                    if (card) {

                        /* 龙胆模式：闪当杀打出（直接设置选目标状态，不调用game_start_target_select，因为闪不需要选目标） */
                        if(me->hero_id == HERO_ZHAOYUN && me->longdan_active &&
                           card->type == CARD_BASIC && card->sub.basic.basic_type == BASIC_SHAN)
                        {
                            game_log(game, "【龙胆】将【闪】当【杀】打出，请选择目标");
                            game->pending_hand_index = idx;
                            game->pending_card = card;
                            game->resp_state = RESPONSE_NEED_TARGET;
                            state->selected_hand_index = -1;
                            return;
                        }
                        /* 龙胆模式：杀当闪打出（闪不能主动使用，提示） */
                        if(me->hero_id == HERO_ZHAOYUN && me->longdan_active &&
                           card->type == CARD_BASIC && card->sub.basic.basic_type == BASIC_SHA)
                        {
                            game_log(game, "【龙胆】【杀】当【闪】，但闪不能在出牌阶段主动使用");
                            state->selected_hand_index = -1;
                            return;
                        }

                        /* 镜流登仙：点击牌时先进入牌型转换选择 */
                        if(me->hero_id == HERO_JINGLIU &&
                           me->jingliu.transformation == JINGLIU_FORM_DENGXIAN)
                        {
                            game_log(game, "【登仙】请选择牌的使用方式");
                            game_start_dengxian_convert(game, idx);
                            state->selected_hand_index = -1;
                            return;
                        }

                        /* 林雨霞玉盏：已发动且牌需要目标时，先进入目标调整选择 */
                        if(me->hero_id == HERO_LINYUXIA && me->yuzhan_active &&
                           card_needs_target(card))
                        {
                            game_log(game, "【玉盏】请选择目标调整方式");
                            game_start_yuzhan_target(game, idx);
                            state->selected_hand_index = -1;
                            return;
                        }

                        if (card->type == CARD_EQUIP) {
                            /* 雨蝶破茧：装备牌仅能重铸（弃置并摸一张牌） */
                            Player* me_p = &game->players[0];
                            if(me_p->hero_id == HERO_YUDIE && !me_p->yudie.chengdie)
                            {
                                Card* equip_card = player_remove_hand(me_p, idx);
                                discard_add(&game->discard, equip_card);
                                game_log(game, "【破茧】%s重铸【%s】，弃置并摸一张牌",
                                         me_p->name, card_get_full_name(equip_card));
                                if(game->deck.count > 0)
                                {
                                    Card* draw = deck_draw(&game->deck);
                                    if(draw) player_draw_card(me_p, draw);
                                }
                            }
                            else
                            {
                                /* 装备牌：进入确认出牌状态 */
                                game_log(game, "点击确定装备【%s】", card_get_full_name(card));
                                game_start_confirm_play(game, idx, -1);
                            }
                        } else if (card->type == CARD_TRICK &&
                                   card->sub.trick.trick_type == TRICK_WUXIE) {
                            /* 无懈可击：不能在出牌阶段主动使用，只能响应锦囊时使用 */
                            game_log(game, "【无懈可击】只能在响应锦囊时使用，不能主动打出");
                            state->selected_hand_index = -1;
                            return;
                        } else if (card_needs_target(card)) {
                            /* 需要选目标的牌：杀、过河拆桥、顺手牵羊、决斗、火攻、延时锦囊 */
                            game_start_target_select(game, idx);
                        } else {
                            /* 群体锦囊和其他牌：进入确认出牌状态 */
                            game_log(game, "点击确定打出【%s】", card_get_full_name(card));
                            game_start_confirm_play(game, idx, -1);
                        }
                    }
                    state->selected_hand_index = -1;
                }
            }
            else
            {
                /* 没点中手牌：检测是否点击了装备区的丈八蛇矛 */
                if(game->game_over == 0 && game->resp_state == RESPONSE_NONE &&
                   game->phase == PHASE_PLAY && game->current_player == 0)
                {
                    if(input_hit_zhangba(game, e->button.x, e->button.y))
                    {
                        game_start_zhangba(game);
                        return;
                    }
                }
            }
        }
        break;

    case SDL_MOUSEBUTTONUP:
        /* 日志弹窗拖动：鼠标松开时结束拖动 */
        if(state->log_dragging)
        {
            state->log_dragging = 0;
        }
        /* 飞舞拖拽：鼠标松开时结束拖拽 */
        if(game && game->resp_state == RESPONSE_NEED_FEIWUU_DRAG && game->feiwuu_dragging)
        {
            game_feiwuu_end_drag(game, e->button.x, e->button.y);
            return;
        }
        /* 鼠标松开：只清除长按检测状态，不清除技能描述（点击描述框外才关闭） */
        state->mouse_down_time = 0;
        break;

    case SDL_MOUSEWHEEL:
        /* 日志弹窗：滚轮滚动日志 */
        if(game->log_panel_open)
        {
            if(e->wheel.y > 0)
            {
                /* 向上滚动：查看更早的日志 */
                game->log_scroll += 3;
            }
            else if(e->wheel.y < 0)
            {
                /* 向下滚动：查看更新的日志 */
                game->log_scroll -= 3;
                if(game->log_scroll < 0) game->log_scroll = 0;
            }
        }
        break;

    case SDL_KEYDOWN:
        if (e->key.keysym.sym == SDLK_SPACE) {
            if (!game->game_over && game->resp_state == RESPONSE_NONE)
            {
                if(game->judge_active)
                    game_judge_advance(game);
                else if(game->phase == PHASE_PLAY && game->current_player == 0)
                {
                    /* 玩家出牌阶段按空格：结束出牌阶段，进入弃牌阶段 */
                    game_log(game, "%s 结束出牌阶段", game->players[0].name);
                    game->phase = PHASE_DISCARD;
                    game_next_phase(game);
                }
                else
                    game_next_phase(game);
            }
        } else if (e->key.keysym.sym == SDLK_r) {
            game_restart(game);
            state->selected_hand_index = -1;
        } else if (e->key.keysym.sym == SDLK_q) {
            /* Q键：发动主动技能（先试第2个技能skill_idx=1，失败试第1个） */
            if(!game->game_over && game->resp_state == RESPONSE_NONE &&
               game->phase == PHASE_PLAY && game->current_player == 0)
            {
                Player* me = &game->players[0];
                if(me->hero && me->hero->can_use_skill)
                {
                    if(me->hero->can_use_skill(game, 0, 1))
                        game_use_active_skill(game, 0, 1);
                    else if(me->hero->can_use_skill(game, 0, 0))
                        game_use_active_skill(game, 0, 0);
                }
            }
        } else if (e->key.keysym.sym == SDLK_ESCAPE) {
            /* ESC：选目标状态取消 */
            if(game->resp_state == RESPONSE_NEED_TARGET) {
                game_cancel_target_select(game);
            }
            state->selected_hand_index = -1;
        }
        break;
    }
}


/* ========== 无懈可击 ========== */
void input_handle_wuxie_y(GameState* g)
{
    if (!g || g->resp_state != RESPONSE_NEED_WUXIE) return;

    /* 铁索连环无懈可击：玩家使用无懈可击抵消对自己的效果 */
    if(g->tiesuo_wuxie_index < g->tiesuo_target_count && g->tiesuo_target_count > 0)
    {
        Player* me = &g->players[0];
        int wuxie_idx = -1;
        /* 玩家已选中响应牌：使用选中的牌 */
        if(g->response_pick_selected && g->response_pick_index >= 0 &&
           g->response_pick_index < me->hand_count)
        {
            Card* sel = me->hand[g->response_pick_index];
            if(sel && sel->type == CARD_TRICK && sel->sub.trick.trick_type == TRICK_WUXIE)
                wuxie_idx = g->response_pick_index;
        }
        /* 未选中：找第一张无懈可击 */
        if(wuxie_idx == -1)
        {
            for(int i = 0; i < me->hand_count; i++)
            {
                if(me->hand[i] && me->hand[i]->type == CARD_TRICK &&
                   me->hand[i]->sub.trick.trick_type == TRICK_WUXIE)
                {
                    wuxie_idx = i;
                    break;
                }
            }
        }
        if(wuxie_idx != -1)
        {
            Card* w = player_remove_hand(me, wuxie_idx);
            discard_add(&g->discard, w);
            g->central_show_card = w;
        }
        game_tiesuo_wuxie_result(g, 1);
        return;
    }

    Player* me = &g->players[0];
    Player* ai = &g->players[1];
    int wuxie_idx = -1;
    /* 玩家已选中响应牌：使用选中的牌 */
    if(g->response_pick_selected && g->response_pick_index >= 0 &&
       g->response_pick_index < me->hand_count)
    {
        Card* sel = me->hand[g->response_pick_index];
        if(sel && sel->type == CARD_TRICK && sel->sub.trick.trick_type == TRICK_WUXIE)
            wuxie_idx = g->response_pick_index;
    }
    /* 未选中：找第一张无懈可击 */
    if(wuxie_idx == -1)
    {
        for(int i=0;i<me->hand_count;i++)
            if(me->hand[i]->type==CARD_TRICK &&
               me->hand[i]->sub.trick.trick_type==TRICK_WUXIE)
            { wuxie_idx=i; break; }
    }

    if(wuxie_idx != -1)
    {
        Card* w = player_remove_hand(me, wuxie_idx);
        discard_add(&g->discard, w);
        g->central_show_card = w;
        game_log(g, "你打出【%s】！", card_get_full_name(w));

        if(g->group_active)
        {
            /* ===== 反无懈阶段：AI之前打了无懈，玩家反无懈 ===== */
            if(g->group_wuxie_counter_from != -1)
            {
                int from = g->group_wuxie_counter_from;
                /* 反无懈成功：清除AI的无懈标记，两个无懈相互抵消 */
                g->group_wuxie_mask &= ~(1 << from);
                g->group_wuxie_counter_from = -1;
                game_log(g, "你的无懈可击抵消了%s的无懈可击！", g->players[from].name);
                g->resp_state = RESPONSE_NONE;
                return;
            }

            /* ===== 普通打无懈阶段 ===== */
            g->group_wuxie_mask |= (1 << g->resp_target_player);
            game_log(g, "%s 被无懈抵消", g->players[g->resp_target_player].name);

            /* AI反无懈：如果AI手里有无懈，有50%概率反无懈 */
            int ai_wuxie_idx = -1;
            for(int i=0;i<ai->hand_count;i++)
                if(ai->hand[i]->type==CARD_TRICK &&
                   ai->hand[i]->sub.trick.trick_type==TRICK_WUXIE)
                { ai_wuxie_idx=i; break; }

            if(ai_wuxie_idx != -1 && rand()%10 < 5)
            {
                Card* aw = player_remove_hand(ai, ai_wuxie_idx);
                discard_add(&g->discard, aw);
                g->central_show_card = aw;
                /* 反无懈成功：清除玩家的无懈标记，两个无懈相互抵消 */
                g->group_wuxie_mask &= ~(1 << g->resp_target_player);
                game_log(g, "%s 打出【无懈可击】，抵消了你的无懈可击！", ai->name);
            }
            else
            {
                game_log(g, "%s 不使用无懈可击", ai->name);
            }

            g->group_wuxie_counter_from = -1;
            g->resp_state = RESPONSE_NONE;
        }
        else
        {
            /* 单体锦囊：无懈可击直接抵消 */
            game_log(g, "锦囊被抵消！");
            if(g->single_trick_pending && g->single_trick_card)
            {
                /* 弃置被抵消的锦囊牌 */
                discard_add(&g->discard, g->single_trick_card);
                g->single_trick_card = NULL;
            }
            g->single_trick_pending = 0;
            g->resp_state = RESPONSE_NONE;
            g->resp_trigger_card = NULL;
            g->resp_source_player = -1;
            g->resp_target_player = -1;
            g->duel_turn = -1;
            g->group_active = 0;
        }
    }
    else
    {
        game_log(g, "你没有无懈可击！");
        input_handle_wuxie_n(g);
    }
}


void input_handle_wuxie_n(GameState* g)
{
    if (!g || g->resp_state != RESPONSE_NEED_WUXIE) return;

    /* 铁索连环无懈可击：玩家不使用无懈可击 */
    if(g->tiesuo_wuxie_index < g->tiesuo_target_count && g->tiesuo_target_count > 0)
    {
        game_tiesuo_wuxie_result(g, 0);
        return;
    }

    if(g->group_active)
    {
        /* 反无懈阶段：玩家选择不反无懈，AI的无懈生效 */
        if(g->group_wuxie_counter_from != -1)
        {
            game_log(g, "你选择不抵消%s的无懈可击", g->players[g->group_wuxie_counter_from].name);
            g->group_wuxie_counter_from = -1;
            g->resp_state = RESPONSE_NONE;
            return;
        }
        /* 普通打无懈阶段：玩家选择不打无懈 */
        game_log(g, "你选择不使用无懈可击");
        g->resp_state = RESPONSE_NONE;
    }
    else
    {
        /* 通用单体锦囊：不打无懈，续接执行锦囊效果 */
        game_log(g, "你选择不使用无懈可击");

        if(g->single_trick_pending)
        {
            int trick_type = g->single_trick_type;
            int source = g->single_trick_source;
            int target = g->single_trick_target;
            Card* card = g->single_trick_card;

            g->single_trick_pending = 0;
            g->single_trick_card = NULL;

            if(trick_type == TRICK_WUZHONG)
            {
                /* 无中生有：摸2张 */
                game_draw_cards(g, source, 2);
                discard_add(&g->discard, card);
                game_log(g, "%s 使用无中生有，摸2张牌", g->players[source].name);
                g->resp_state = RESPONSE_NONE;
            }
            else if(trick_type == TRICK_GUOHE)
            {
                /* 过河拆桥：进入选牌状态 */
                discard_add(&g->discard, card);
                game_start_pick_enemy_card(g, source, target, 0);
            }
            else if(trick_type == TRICK_SHUNSHOU)
            {
                /* 顺手牵羊：进入选牌状态 */
                discard_add(&g->discard, card);
                game_start_pick_enemy_card(g, source, target, 1);
            }
            else if(trick_type == TRICK_HUOGONG)
            {
                /* 火攻：进入火攻状态机 */
                g->huogong_active = 1;
                g->huogong_source = source;
                g->huogong_target = target;
                g->huogong_show_card = NULL;
                g->huogong_need_suit = 0;
                g->resp_state = RESPONSE_NEED_HUOGONG_SHOW;
                /* 火攻牌先不弃置，等火攻结算完再弃 */
                g->single_trick_card = card;  /* 暂时保留引用 */
            }
            else
            {
                /* 其他情况：当作决斗处理 */
                g->resp_state = RESPONSE_NEED_BASIC;
                g->duel_turn = g->resp_target_player;
                g->group_active = 0;
                game_log(g, "【决斗】请点击【杀】选中，点击确认打出，点击取消放弃");
            }
        }
        else
        {
            /* 决斗：不打无懈，进入决斗状态机 */
            g->resp_state = RESPONSE_NEED_BASIC;
            g->duel_turn = g->resp_target_player;
            g->group_active = 0;
            game_log(g, "【决斗】请点击【杀】选中，点击确认打出，点击取消放弃");
        }
    }
}


/* ========== 基本牌响应 ========== */
void input_handle_response_y(GameState* g)
{
    if (!g || g->resp_state != RESPONSE_NEED_BASIC) return;

    if(g->group_active && g->duel_turn == -1)
    {
        int target = g->resp_target_player;
        Player* tp = &g->players[target];
        int need = g->resp_required_basic;
        int found = -1;
        int longdan_used = 0;

        /* 先找原生牌 */
        for(int i=0;i<tp->hand_count;i++)
            if(tp->hand[i]->type==CARD_BASIC &&
               tp->hand[i]->sub.basic.basic_type==need)
            { found=i; break; }

        /* 找不到原生牌且龙胆模式已激活：找转换牌 */
        if(found == -1 && tp->hero_id == HERO_ZHAOYUN && tp->longdan_active)
        {
            BasicType convert_type = (need == BASIC_SHA) ? BASIC_SHAN : BASIC_SHA;
            for(int i=0;i<tp->hand_count;i++)
                if(tp->hand[i]->type==CARD_BASIC &&
                   tp->hand[i]->sub.basic.basic_type==convert_type)
                { found=i; longdan_used=1; break; }
        }

        if(found != -1)
        {
            Card* rc = player_remove_hand(tp, found);
            discard_add(&g->discard, rc);
            g->central_show_card = rc;
            if(longdan_used)
            {
                game_log(g, "【龙胆】%s将【%s】当【%s】打出",
                         tp->name, card_get_name(rc),
                         (need == BASIC_SHA) ? "杀" : "闪");
                tp->longdan_active = 0;
                zhaoyun_huwei(g, target);
            }
            else
            {
                game_log(g, "你打出了【%s】", card_get_full_name(rc));
            }
        }
        else
        {
            game_log(g, "你没有对应响应牌！");
            game_deal_damage(g, target, 1, g->resp_source_player, DMG_NORMAL);
        }
        g->resp_state = RESPONSE_NONE;
        return;
    }

    if(g->duel_turn != -1)
    {
        int cur_idx = g->duel_turn;
        Player* cur = &g->players[cur_idx];
        int sha_idx = -1;
        int longdan_used = 0;

        /* 玩家（0号）且已选中响应牌：使用选中的牌 */
        if(cur_idx == 0 && g->response_pick_selected && g->response_pick_index >= 0 &&
           g->response_pick_index < cur->hand_count)
        {
            Card* sel = cur->hand[g->response_pick_index];
            if(sel && sel->type == CARD_BASIC)
            {
                if(sel->sub.basic.basic_type == BASIC_SHA)
                {
                    sha_idx = g->response_pick_index;
                }
                else if(sel->sub.basic.basic_type == BASIC_SHAN &&
                        cur->hero_id == HERO_ZHAOYUN && cur->longdan_active)
                {
                    sha_idx = g->response_pick_index;
                    longdan_used = 1;
                }
            }
        }

        /* 未选中或选中无效：AI或玩家找牌 */
        if(sha_idx == -1)
        {
            /* 先找原生杀 */
            for(int i=0;i<cur->hand_count;i++)
                if(cur->hand[i]->type==CARD_BASIC &&
                   cur->hand[i]->sub.basic.basic_type==BASIC_SHA)
                { sha_idx=i; break; }

            /* 找不到原生杀且龙胆模式已激活：闪当杀 */
            if(sha_idx == -1 && cur->hero_id == HERO_ZHAOYUN && cur->longdan_active)
            {
                for(int i=0;i<cur->hand_count;i++)
                    if(cur->hand[i]->type==CARD_BASIC &&
                       cur->hand[i]->sub.basic.basic_type==BASIC_SHAN)
                    { sha_idx=i; longdan_used=1; break; }
            }
        }

        if(sha_idx!=-1)
        {
            Card* sha=player_remove_hand(cur,sha_idx);
            discard_add(&g->discard,sha);
            g->central_show_card=sha;
            if(longdan_used)
            {
                game_log(g,"【龙胆】%s将【闪】当【杀】响应决斗",cur->name);
                cur->longdan_active = 0;
                zhaoyun_huwei(g, cur_idx);
            }
            else
            {
                game_log(g,"%s 打出【杀】响应决斗",cur->name);
            }
            g->duel_turn = (cur_idx==g->resp_target_player) ?
                            g->resp_source_player : g->resp_target_player;
        }
        else
        {
            game_log(g,"%s 没有杀，受到决斗1点伤害",cur->name);
            g->current_damage_source = DMG_SRC_JUEDOU;
            game_deal_damage(g,cur_idx,1,g->resp_source_player,DMG_NORMAL);
            g->resp_state=RESPONSE_NONE;
            g->duel_turn=-1;
            game_check_victory(g);
        }
        return;
    }

    {
        int target = g->resp_target_player;
        int source = g->resp_source_player;
        Player* tp = &g->players[target];
        int found = -1;
        int longdan_used = 0;  /* 是否使用了龙胆转换 */

        /* 玩家0且已选中响应牌：使用选中的牌 */
        if(target == 0 && g->response_pick_selected && g->response_pick_index >= 0 &&
           g->response_pick_index < tp->hand_count)
        {
            found = g->response_pick_index;
            Card* sel = tp->hand[found];
            if(sel && sel->type == CARD_BASIC &&
               sel->sub.basic.basic_type != g->resp_required_basic)
            {
                /* 选中的不是原生响应牌，说明是龙胆转换牌 */
                longdan_used = 1;
            }
        }
        else
        {
            /* AI或未选中：先找原生响应牌 */
            for(int i=0;i<tp->hand_count;i++)
                if(tp->hand[i]->type==CARD_BASIC &&
                   tp->hand[i]->sub.basic.basic_type==g->resp_required_basic)
                { found=i; break; }

            /* 找不到原生牌且龙胆模式已激活：找转换牌 */
            if(found == -1 && tp->hero_id == HERO_ZHAOYUN && tp->longdan_active)
            {
                BasicType want = g->resp_required_basic;
                BasicType conv = (want == BASIC_SHAN) ? BASIC_SHA : BASIC_SHAN;
                for(int i=0;i<tp->hand_count;i++)
                    if(tp->hand[i]->type==CARD_BASIC &&
                       tp->hand[i]->sub.basic.basic_type==conv)
                    { found=i; longdan_used=1; break; }
            }
        }

        if(found!=-1)
        {
            Card* rc=player_remove_hand(tp,found);
            discard_add(&g->discard,rc);
            g->central_show_card=rc;
            if(longdan_used)
            {
                game_log(g,"【龙胆】%s将【杀】当【闪】打出", tp->name);
                tp->longdan_active = 0;  /* 退出龙胆模式 */
                zhaoyun_huwei(g, target);  /* 虎威：摸2张牌 */
            }
            else
            {
                game_log(g,"你打出了【%s】", card_get_full_name(rc));
            }
            /* 贯石斧：杀被闪后，如果攻击者装备贯石斧，进入贯石斧发动阶段 */
            int dmg = game_calc_sha_damage(g, source, target);
            game_start_guanshi(g, source, target, dmg);
            if(g->guanshi_active) return;  /* 贯石斧发动中，不重置状态 */

            /* 流萤·过载：杀被闪（且贯石斧未发动），失去1点体力 */
            liuying_guozai_on_shan(g, source);
        }
        else
        {
            game_log(g,"你没有闪！");
            input_handle_response_n(g);
            return;
        }
        g->resp_state=RESPONSE_NONE;
        g->resp_trigger_card=NULL;
        g->resp_source_player=-1;
        g->resp_target_player=-1;
        g->response_pick_selected = 0;
        g->response_pick_index = -1;
    }
}


void input_handle_response_n(GameState* g)
{
    if (!g || g->resp_state != RESPONSE_NEED_BASIC) return;

    if(g->group_active && g->duel_turn == -1)
    {
        int target = g->resp_target_player;
        game_log(g, "你选择不响应，受到1点伤害");
        /* 根据锦囊类型设置伤害来源（用于藤甲判断） */
        if(g->group_trick_type == TRICK_NANMAN)
            g->current_damage_source = DMG_SRC_NANMAN;
        else if(g->group_trick_type == TRICK_WANJIAN)
            g->current_damage_source = DMG_SRC_WANJIAN;
        else
            g->current_damage_source = DMG_SRC_OTHER;
        game_deal_damage(g, target, 1, g->resp_source_player, DMG_NORMAL);
        g->resp_state = RESPONSE_NONE;
        return;
    }

    if(g->duel_turn != -1)
    {
        int cur_idx = g->duel_turn;
        game_log(g, "%s 放弃出杀，受到决斗1点伤害", g->players[cur_idx].name);
        g->current_damage_source = DMG_SRC_JUEDOU;
        game_deal_damage(g, cur_idx, 1, g->resp_source_player, DMG_NORMAL);
        g->resp_state=RESPONSE_NONE;
        g->duel_turn=-1;
        g->resp_source_player = -1;
        g->resp_target_player = -1;
        g->resp_trigger_card = NULL;
        g->group_active = 0;
        game_check_victory(g);
        return;
    }

    {
        int target = g->resp_target_player;
        int source = g->resp_source_player;
        Player* attacker = &g->players[source];
        Player* tp = &g->players[target];
        int dmg = game_calc_sha_damage(g, source, target);
        game_log(g, "你选择不出闪，受到%d点伤害", dmg);
        /* 寒冰剑：杀造成伤害前，如果攻击者装备寒冰剑，进入寒冰剑发动阶段 */
        game_start_hanbing(g, source, target, dmg);
        if(!g->hanbing_active) {
            /* 没有发动寒冰剑，正常造成伤害 */
            g->current_damage_source = DMG_SRC_SHA;
            game_deal_damage(g, target, dmg, source, DMG_NORMAL);
        }
        g->resp_state=RESPONSE_NONE;
        g->resp_trigger_card=NULL;
        g->resp_source_player=-1;
        g->resp_target_player=-1;
    }
}
