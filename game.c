#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "game.h"
#include "render.h"
#include "heroes/hero.h"
#include "heroes/feixiao/feixiao.h"
#include "heroes/zhaoyun/zhaoyun.h"
#include "heroes/gilgamesh/gilgamesh.h"
#include "heroes/linyuxia/linyuxia.h"
#include "heroes/paladin/paladin.h"
#include "heroes/yudie/yudie.h"
#include "heroes/liuying/liuying.h"
#include "heroes/jingliu/jingliu.h"
#include "nn_bridge.h"
#include "state_encoder.h"
#include "mcts.h"

/* 神经网络AI开关 */
static int g_nn_ai_enabled = 0;
static int g_nn_ai_inited = 0;


/* ================================================================
 * 火攻重写版（新思路）：
 *   game_update 里完全没有火攻逻辑，杜绝"每帧自动弃牌"
 *   AI 自动结算全部在函数内部同步完成：
 *     1. 玩家发动火攻，目标是AI → game_use_card 里AI自动随机展示，进入PICK等玩家选牌
 *     2. AI发动火攻，目标是玩家 → 进入SHOW等玩家选牌展示
 *        玩家选牌后 → input_handle_huogong_show 里AI使用者自动同步结算（找同花色弃牌或放弃）
 *     3. 玩家发动火攻，目标是玩家 → 不可能（火攻不能指定自己，2人局目标只能是对方）
 * ================================================================ */


static void game_reset_turn_state(Player* p)
{
    p->sha_used = 0;
    p->jiu_used = 0;
    p->skip_draw = 0;
    p->skip_play = 0;
}


void game_log(GameState* g, const char* fmt, ...)
{
    if (!g) return;
    if (g->log_count >= 200) {
        for (int i = 0; i < 199; i++) {
            strncpy(g->log_buf[i], g->log_buf[i + 1], sizeof(g->log_buf[i]) - 1);
            g->log_buf[i][sizeof(g->log_buf[i]) - 1] = '\0';
        }
        g->log_count = 199;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->log_buf[g->log_count], sizeof(g->log_buf[g->log_count]), fmt, ap);
    va_end(ap);
    g->log_count++;
}


/* ================================================================
 * 通用展示函数：在屏幕中心向所有人展示一张牌
 * card：要展示的牌
 * who：展示者名字（如"雨蝶"）
 * 展示持续180帧（约3秒，60fps）
 * ================================================================ */
void game_show_card(GameState* g, Card* card, const char* who)
{
    if(!g || !card) return;
    g->show_card_center = card;
    if(who)
        strncpy(g->show_card_who, who, sizeof(g->show_card_who) - 1);
    else
        g->show_card_who[0] = '\0';
    g->show_card_timer = 180;  /* 3秒 */
    game_log(g, "【展示】%s展示【%s】", who ? who : "", card_get_full_name(card));
}


/* ================================================================
 * 通用文字提示：在屏幕中心显示一段文字
 * message：要显示的文字
 * duration_frames：持续帧数（60fps，180=3秒）
 * ================================================================ */
void game_show_message(GameState* g, const char* message, int duration_frames)
{
    if(!g || !message) return;
    strncpy(g->center_message, message, sizeof(g->center_message) - 1);
    g->center_message[sizeof(g->center_message) - 1] = '\0';
    g->center_message_timer = duration_frames > 0 ? duration_frames : 180;
    game_log(g, "【提示】%s", message);
}


/* ================================================================
 * 通用批量展示牌：在屏幕中心展示多张牌
 * cards：要展示的牌数组
 * count：牌的数量
 * who：谁展示的（用于日志）
 * duration_frames：持续帧数（60fps，180=3秒）
 * ================================================================ */
void game_show_cards(GameState* g, Card** cards, int count, const char* who, int duration_frames)
{
    if(!g || !cards || count <= 0) return;

    /* 清空之前的展示 */
    for(int i = 0; i < 10; i++)
        g->show_cards_center[i] = NULL;

    /* 复制牌指针（最多10张） */
    int copy_count = count < 10 ? count : 10;
    for(int i = 0; i < copy_count; i++)
        g->show_cards_center[i] = cards[i];

    g->show_cards_count = copy_count;
    g->show_cards_total = duration_frames > 0 ? duration_frames : 180;
    g->show_cards_timer = g->show_cards_total;

    if(who)
        strncpy(g->show_cards_who, who, sizeof(g->show_cards_who) - 1);
    else
        g->show_cards_who[0] = '\0';

    /* 日志 */
    char card_names[512] = "";
    for(int i = 0; i < copy_count; i++)
    {
        if(cards[i])
        {
            if(i > 0) strncat(card_names, "、", sizeof(card_names) - strlen(card_names) - 1);
            strncat(card_names, card_get_full_name(cards[i]), sizeof(card_names) - strlen(card_names) - 1);
        }
    }
    game_log(g, "【亮出】%s亮出%d张牌：%s", who ? who : "", copy_count, card_names);
}


void game_init(GameState* g)
{
    memset(g, 0, sizeof(GameState));
    g->player_count = 2;

    /* 初始化所有武将 */
    hero_init_table();
    /* 角色选择阶段：不初始化玩家、不发牌 */
    g->player_hero_id = -1;
    g->ai_hero_id = -1;
    g->select_hover = -1;
    g->selected_hero_idx = -1;

    g->phase = PHASE_CHARACTER_SELECT;
    g->game_over = 0;
    g->winner_id = -1;
    g->turn_count = 1;

    game_log(g, "请选择你的角色（点击选中，再次点击确认）");
}
/* ================================================================
 * 通用玩家初始化：支持任意数量玩家
 * hero_ids和is_ai_flags数组长度为player_count
 * ================================================================ */
void game_init_players(GameState* g, int* hero_ids, int* is_ai_flags, int player_count)
{
    if (!g || !hero_ids || !is_ai_flags || player_count <= 0) return;
    if (player_count > MAX_PLAYERS) player_count = MAX_PLAYERS;

    g->player_count = player_count;

    for (int i = 0; i < player_count; i++) {
        Hero* h = hero_get(hero_ids[i]);
        if (!h) {
            fprintf(stderr, "[ERROR] game_init_players: hero_id=%d not found\n", hero_ids[i]);
            continue;
        }
        player_init(&g->players[i], i, h->name, h->max_hp, (HeroId)hero_ids[i]);
        g->players[i].is_ai = is_ai_flags[i] ? 1 : 0;
    }
}


void game_start_with_heroes(GameState* g, int player_hero_idx, int ai_hero_idx)
{
    if (!g) return;

    /* 初始化神经网络AI（只执行一次） */
    game_nn_ai_init();

    Hero* ph = hero_get(player_hero_idx);
    Hero* ah = hero_get(ai_hero_idx);
    if (!ph || !ah) return;

    g->player_hero_id = player_hero_idx;
    g->ai_hero_id = ai_hero_idx;

    /* 初始化玩家（调用通用函数，保持双人场兼容） */
    int hero_ids[2] = {player_hero_idx, ai_hero_idx};
    int is_ai_flags[2] = {0, 1};
    game_init_players(g, hero_ids, is_ai_flags, 2);

    /* 调试：确认角色初始化正确 */
    fprintf(stderr, "[DEBUG] game_start_with_heroes:\n");
    fprintf(stderr, "  player: idx=%d hero_id=%d name=%s hero_ptr=%p skill_count=%d\n",
            player_hero_idx, g->players[0].hero_id, g->players[0].name,
            (void*)g->players[0].hero, g->players[0].hero ? g->players[0].hero->skill_count : -1);
    fprintf(stderr, "  ai: idx=%d hero_id=%d name=%s hero_ptr=%p skill_count=%d\n",
            ai_hero_idx, g->players[1].hero_id, g->players[1].name,
            (void*)g->players[1].hero, g->players[1].hero ? g->players[1].hero->skill_count : -1);
    for(int s = 0; s < g->players[0].hero->skill_count; s++)
        fprintf(stderr, "    player skills[%d]: %s (type=%d)\n",
                s, g->players[0].hero->skills[s].name, g->players[0].hero->skills[s].type);

    deck_init_standard(&g->deck);
    g->discard.cards = NULL;
    g->discard.count = 0;
    g->discard.capacity = 0;
    g->discard.top = 0;

    g->resp_state = RESPONSE_NONE;
    g->duel_turn = -1;
    g->resp_source_player = -1;
    g->resp_target_player = -1;
    g->resp_trigger_card = NULL;
    g->resp_need_basic_after_wuxie = 0;
    g->resp_required_basic = 0;
    g->ai_play_finished = 0;
    g->central_show_card = NULL;

    /* 屏幕中心展示牌初始化 */
    g->show_card_center = NULL;
    g->show_card_who[0] = '\0';
    g->show_card_timer = 0;

    /* 长按技能描述初始化 */
    g->long_press_skill_idx = -1;

    g->group_active = 0;
    g->group_phase = 0;
    g->group_current = -1;
    g->group_source = -1;
    g->group_trick_type = 0;
    g->group_trigger_card = NULL;
    g->group_wuxie_mask = 0;
    g->group_wuxie_counter_from = -1;
    g->group_wugu_count = 0;
    g->current_damage_source = DMG_SRC_OTHER;
    g->current_damage_source_player = -1;

    /* 火攻状态初始化 */
    g->huogong_active = 0;
    g->huogong_source = -1;
    g->huogong_target = -1;
    g->huogong_show_card = NULL;
    g->huogong_need_suit = 0;

    /* 贯石斧状态初始化 */
    g->guanshi_active = 0;
    g->guanshi_source = -1;
    g->guanshi_target = -1;
    g->guanshi_damage = 0;
    g->guanshi_picking = 0;
    g->guanshi_picked_count = 0;
    g->guanshi_picked[0] = -1;
    g->guanshi_picked[1] = -1;

    /* 雨蝶飞舞选牌初始化 */
    g->feiwuu_selected_count = 0;
    for(int i = 0; i < 4; i++) g->feiwuu_selected[i] = -1;

    /* 寒冰剑状态初始化 */
    g->hanbing_active = 0;
    g->hanbing_source = -1;
    g->hanbing_target = -1;
    g->hanbing_damage = 0;
    g->hanbing_picking = 0;
    g->hanbing_picked_count = 0;
    g->hanbing_picked_type[0] = -1;
    g->hanbing_picked_type[1] = -1;
    g->hanbing_picked_index[0] = -1;
    g->hanbing_picked_index[1] = -1;

    /* 过河拆桥/顺手牵羊选择状态初始化 */
    g->pick_enemy_target = -1;
    g->pick_enemy_action = 0;
    g->pick_enemy_card_type = -1;
    g->pick_enemy_card_index = -1;

    /* 朱雀羽扇状态初始化 */
    g->zhuque_active = 0;
    g->zhuque_source = -1;
    g->zhuque_target = -1;
    g->zhuque_sha_card = NULL;
    g->zhuque_selected = 0;

    /* 丈八蛇矛状态初始化 */
    g->zhangba_active = 0;
    g->zhangba_selected_count = 0;
    g->zhangba_selected[0] = -1;
    g->zhangba_selected[1] = -1;
    g->zhangba_virtual_sha = NULL;

    /* 八卦阵状态初始化 */
    g->bagua_active = 0;
    g->bagua_source = -1;
    g->bagua_attacker = -1;
    g->bagua_trigger_card = NULL;
    g->bagua_selected = 0;
    g->bagua_judge_card = NULL;

    /* 选目标状态初始化 */
    g->pending_hand_index = -1;
    g->pending_card = NULL;
    g->discard_need_count = 0;
    g->response_pick_selected = 0;

    /* 通用主动弃牌初始化 */
    g->generic_discard_player = -1;
    g->generic_discard_need = 0;
    g->generic_discard_selected_count = 0;
    g->generic_discard_source = -1;
    g->generic_discard_done = 0;
    memset(g->generic_discard_selected, -1, sizeof(g->generic_discard_selected));

    /* 确认出牌初始化 */
    g->confirm_play_hand_index = -1;
    g->confirm_play_target_index = -1;

    /* 铁索连环选目标初始化 */
    g->tiesuo_hand_index = -1;
    g->tiesuo_targets[0] = -1;
    g->tiesuo_targets[1] = -1;
    g->tiesuo_target_count = 0;
    g->tiesuo_wuxie_index = 0;
    g->tiesuo_wuxie_mask = 0;

    /* 雨蝶飞舞拖拽初始化 */
    g->feiwuu_drag_count = 0;
    g->feiwuu_drag_index = -1;
    g->feiwuu_dragging = 0;
    g->feiwuu_drag_x = 0;
    g->feiwuu_drag_y = 0;
    for(int i = 0; i < 4; i++)
    {
        g->feiwuu_drag_cards[i] = NULL;
        g->feiwuu_placed_slots[i] = 0;
    }

    /* 吉尔伽美什技能初始化 */
    g->gilgamesh_skill_idx = -1;
    g->gilgamesh_target_idx = -1;

    /* 鼠标位置初始化 */
    g->mouse_x = 0;
    g->mouse_y = 0;

    g->judge_active = 0;
    g->judge_step = 0;
    g->judge_idx = 0;
    g->judge_delay = NULL;
    g->judge_card = NULL;
    g->judge_result[0] = '\0';
    g->judge_delay_action = 0;

    for(int i=0;i<16;i++) g->group_wugu_pile[i] = NULL;

    for (int i = 0; i < g->player_count; i++) {
        for (int j = 0; j < 4; j++) {
            Card* c = deck_draw(&g->deck);
            if (c) player_draw_card(&g->players[i], c);
        }
    }

    g->current_player = 0;
    g->phase = PHASE_PREPARE;
    g->game_over = 0;
    g->winner_id = -1;
    g->turn_count = 1;

    /* 第一轮开始：触发所有玩家的 on_round_start 回调（如凝盾） */
    for(int i = 0; i < g->player_count; i++) {
        Player* rp = &g->players[i];
        if(rp->alive && rp->hero && rp->hero->on_round_start)
            rp->hero->on_round_start(g, i);
    }

    game_log(g, "游戏开始！%s vs %s", g->players[0].name, g->players[1].name);
    game_log(g, "%s 的回合 - 准备阶段", g->players[g->current_player].name);


        Hero* ph2 = hero_get(g->players[0].hero_id);
    Hero* ah2 = hero_get(g->players[1].hero_id);
    printf("[DEBUG]玩家 hero_id=%d | ptr=%p | skill_count=%d\n",
           g->players[0].hero_id, ph2, ph2 ? ph2->skill_count : -1);
    printf("[DEBUG]AI     hero_id=%d | ptr=%p | skill_count=%d\n",
           g->players[1].hero_id, ah2, ah2 ? ah2->skill_count : -1);

}


void game_destroy(GameState* g)
{
    if (!g) return;
    for (int i = 0; i < g->player_count; i++) {
        player_destroy(&g->players[i]);
    }
    deck_destroy(&g->deck);
    deck_destroy(&g->discard);
    memset(g, 0, sizeof(GameState));
}


/* ================================================================
 * 发动主动技能（Q键触发）
 * ================================================================ */
void game_use_active_skill(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(!p->hero) return;

    /* can_use_skill 为 NULL 时默认可以发动 */
    int can_use = 1;
    if(p->hero->can_use_skill)
        can_use = p->hero->can_use_skill(g, player_idx, skill_idx);

    if(can_use)
    {
        if(p->hero->use_skill)
            p->hero->use_skill(g, player_idx, skill_idx);
    }
    else
    {
        game_log(g, "%s 无法发动技能（条件不满足）", p->name);
    }
}


void game_restart(GameState* g)
{
    game_destroy(g);
    game_init(g);
}


const char* game_get_player_name(GameState* g, int idx)
{
    if (!g || idx < 0 || idx >= g->player_count) return "";
    return g->players[idx].name;
}


/* ================================================================
 * 通用防具检查：判断玩家是否装备了指定类型的防具
 * 先检查是否是真正的防具牌，避免非装备牌的字段未定义导致误判
 * ================================================================ */
static int player_has_armor(Player* p, int armor_type)
{
    if(!p) return 0;
    Card* armor = player_get_armor(p);
    if(!armor) return 0;
    if(armor->type != CARD_EQUIP) return 0;
    if(armor->sub.equip.equip_type != EQUIP_ARMOR) return 0;
    return (armor->sub.equip.detail.armor.armor_type == armor_type);
}


int game_calc_distance(GameState* g, int from_idx, int to_idx)
{
    if (!g || from_idx == to_idx) return 0;
    if (from_idx < 0 || from_idx >= g->player_count) return 99;
    if (to_idx < 0 || to_idx >= g->player_count) return 99;

    /* 计算存活玩家数（距离只计算存活玩家之间的座位） */
    int alive_count = 0;
    int alive_indices[MAX_PLAYERS];
    for (int i = 0; i < g->player_count; i++) {
        if (g->players[i].alive) {
            alive_indices[alive_count++] = i;
        }
    }

    /* 找到 from 和 to 在存活列表中的位置 */
    int from_pos = -1, to_pos = -1;
    for (int i = 0; i < alive_count; i++) {
        if (alive_indices[i] == from_idx) from_pos = i;
        if (alive_indices[i] == to_idx) to_pos = i;
    }

    if (from_pos < 0 || to_pos < 0) return 99;

    /* 顺时针距离和逆时针距离，取较小值 */
    int clockwise = (to_pos - from_pos + alive_count) % alive_count;
    int counter_clockwise = (from_pos - to_pos + alive_count) % alive_count;
    int dist = clockwise < counter_clockwise ? clockwise : counter_clockwise;

    /* 2人场时距离固定为1（兼容现有逻辑） */
    if (alive_count == 2) dist = 1;

    /* 进攻马 -1，防御马 +1 */
    if (player_get_horse_atk(&g->players[from_idx])) dist--;
    if (player_get_horse_def(&g->players[to_idx])) dist++;
    if (dist < 1) dist = 1;
    return dist;
}


/* ================================================================
 * 通用玩家位置计算
 * 2人场：保持现有位置（玩家0右下角，玩家1正上方）
 * 多人场：环形布局，玩家0在底部，顺时针排列
 * ================================================================ */
void game_get_player_position(int player_idx, int player_count, int *x, int *y)
{
    if (!x || !y) return;
    if (player_idx < 0 || player_idx >= player_count) { *x = -1000; *y = -1000; return; }

    /* 2人场：保持现有位置，兼容现有渲染和输入 */
    if (player_count == 2) {
        if (player_idx == 0) {
            /* 玩家0：右下角 */
            *x = WINDOW_WIDTH - HERO_WIDTH - 40;
            *y = WINDOW_HEIGHT - HERO_HEIGHT - 40;
        } else {
            /* 玩家1：正上方中央 */
            *x = (WINDOW_WIDTH - HERO_WIDTH) / 2;
            *y = 60;
        }
        return;
    }

    /* 多人场：环形布局 */
    int cx = WINDOW_WIDTH / 2;
    int cy = WINDOW_HEIGHT / 2;
    int radius_x = WINDOW_WIDTH / 2 - HERO_WIDTH - 60;
    int radius_y = WINDOW_HEIGHT / 2 - HERO_HEIGHT - 80;
    if (radius_x < 100) radius_x = 100;
    if (radius_y < 100) radius_y = 100;

    /* 玩家0在底部（角度 = PI/2），顺时针排列 */
    double angle = M_PI / 2.0 + 2.0 * M_PI * player_idx / player_count;
    *x = cx + (int)(radius_x * cos(angle)) - HERO_WIDTH / 2;
    *y = cy + (int)(radius_y * sin(angle)) - HERO_HEIGHT / 2;

    /* 边界检查 */
    if (*x < 10) *x = 10;
    if (*x > WINDOW_WIDTH - HERO_WIDTH - 10) *x = WINDOW_WIDTH - HERO_WIDTH - 10;
    if (*y < 10) *y = 10;
    if (*y > WINDOW_HEIGHT - HERO_HEIGHT - 10) *y = WINDOW_HEIGHT - HERO_HEIGHT - 10;
}


void game_draw_cards(GameState* g, int player_idx, int n)
{
    if (!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    int drawn = 0;
    for (int i = 0; i < n; i++) {
        Card* c = deck_draw(&g->deck);
        if (!c) {
            /* 牌堆空了，尝试重洗弃牌堆 */
            if (g->discard.count > 0) {
                game_log(g, "牌堆已空，重洗弃牌堆（%d张）", g->discard.count);
                /* 把弃牌堆的牌移到牌堆 */
                for (int j = 0; j < g->discard.count; j++) {
                    if (g->deck.top > 0) {
                        g->deck.top--;
                        g->deck.cards[g->deck.top] = g->discard.cards[j];
                    }
                }
                g->deck.count = g->deck.top + g->discard.count;
                g->discard.count = 0;
                /* 洗牌 */
                deck_shuffle(&g->deck);
                /* 重洗后再摸一次 */
                c = deck_draw(&g->deck);
                if (!c) break;
            } else {
                break;  /* 牌堆和弃牌堆都空了 */
            }
        }
        player_draw_card(p, c);
        drawn++;
    }
    game_log(g, "%s 摸了 %d 张牌", p->name, drawn);
}



/* ================================================================
 * 神经网络AI决策
 * ================================================================ */

/* 初始化神经网络AI */
void game_nn_ai_init(void)
{
    if (g_nn_ai_inited) return;
    g_nn_ai_inited = 1;
    if (nn_init("model.onnx")) {
        g_nn_ai_enabled = 1;
        printf("[AI] 神经网络AI已启用\n");
    } else {
        g_nn_ai_enabled = 0;
        printf("[AI] 神经网络AI加载失败，使用规则AI\n");
    }
}

/* 神经网络AI：出牌阶段决策 */
static int game_nn_ai_play(GameState* g, int player_idx)
{
    if (!g_nn_ai_enabled) return 0;

    Player* p = &g->players[player_idx];
    if (!p->alive) return 0;

    /* 编码状态 */
    float global_data[NN_GLOBAL_DIM];
    float players_data[NN_MAX_PLAYERS * NN_PLAYER_DIM];
    float mask_data[NN_MAX_PLAYERS];
    encode_game_state(g, player_idx, global_data, players_data, mask_data);

    /* 推理 */
    NNResult result;
    if (!nn_infer(global_data, players_data, mask_data, &result)) return 0;

    /* 收集合法动作并选择logits最高的 */
    int best_action_type = -1;
    int best_action_param = -1;
    float best_logit = -1e9f;

    /* 动作类型1000：出牌（param=手牌索引） */
    for (int i = 0; i < p->hand_count; i++) {
        Card* c = p->hand[i];
        if (!c) continue;

        /* 检查是否可以打出 */
        int valid = 0;
        if (c->type == CARD_EQUIP) {
            EquipType et = c->sub.equip.equip_type;
            if ((et == EQUIP_WEAPON && !p->equip.weapon) ||
                (et == EQUIP_ARMOR && !p->equip.armor) ||
                (et == EQUIP_HORSE_ATK && !p->equip.horse_atk) ||
                (et == EQUIP_HORSE_DEF && !p->equip.horse_def)) {
                valid = 1;
            }
        } else if (c->type == CARD_BASIC) {
            if (c->sub.basic.basic_type == BASIC_SHA) {
                int max_sha = 1;
                if (p->hero && p->hero->sha_bonus) max_sha += p->hero->sha_bonus(p);
                int dist = game_calc_distance(g, player_idx, (player_idx == 0) ? 1 : 0);
                int range = player_attack_range(p);
                if (dist <= range && p->sha_used < max_sha) valid = 1;
            } else if (c->sub.basic.basic_type == BASIC_TAO) {
                if (p->hp < p->max_hp) valid = 1;
            } else if (c->sub.basic.basic_type == BASIC_JIU) {
                if (!p->jiu_used) valid = 1;
            }
        } else if (c->type == CARD_TRICK) {
            if (c->sub.trick.trick_type != TRICK_WUXIE) valid = 1;
        } else if (c->type == CARD_DELAYED) {
            valid = 1;
        }

        if (valid) {
            int action_idx = 1 * 1000 + i;  /* type=1: 出牌 */
            if (action_idx < 10000 && result.policy[action_idx] > best_logit) {
                best_logit = result.policy[action_idx];
                best_action_type = 1000;
                best_action_param = i;
            }
        }
    }

    /* 动作类型3000：发动技能（param=技能索引） */
    if (p->hero) {
        for (int i = 0; i < p->hero->skill_count; i++) {
            if (hero_skill_can_use(g, player_idx, i)) {
                int action_idx = 3 * 1000 + i;  /* type=3: 发动技能 */
                if (action_idx < 10000 && result.policy[action_idx] > best_logit) {
                    best_logit = result.policy[action_idx];
                    best_action_type = 3000;
                    best_action_param = i;
                }
            }
        }
    }

    /* 执行最优动作 */
    if (best_action_type == 1000) {
        /* 出牌 */
        Card* c = p->hand[best_action_param];
        int target = -1;
        int enemy_idx = (player_idx == 0) ? 1 : 0;

        if (c->type == CARD_EQUIP) {
            game_equip_card(g, player_idx, best_action_param);
            return 1;
        } else if (c->type == CARD_BASIC && c->sub.basic.basic_type == BASIC_SHA) {
            target = enemy_idx;
        } else if (c->type == CARD_TRICK) {
            switch (c->sub.trick.trick_type) {
                case TRICK_GUOHE: case TRICK_SHUNSHOU:
                case TRICK_JUEDOU: case TRICK_HUOGONG:
                    target = enemy_idx; break;
                default: target = -1;
            }
        } else if (c->type == CARD_DELAYED) {
            target = enemy_idx;
        }

        game_use_card(g, player_idx, best_action_param, target);
        return 1;
    } else if (best_action_type == 3000) {
        /* 发动技能 */
        if (p->hero->use_skill) {
            hero_skill_use(g, player_idx, best_action_param);
            p->hero->use_skill(g, player_idx, best_action_param);
            return 1;
        }
    }

    return 0;  /* 没有合适的动作 */
}

int game_ai_try_play_one(GameState* g, int player_idx)
{
    if (!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if (!p->is_ai || !p->alive) return 0;

    /* 优先使用神经网络AI */
    if (g_nn_ai_enabled) {
        int used = game_nn_ai_play(g, player_idx);
        if (used) return 1;
    }

    int enemy_idx = (player_idx == 0) ? 1 : 0;
    Player* enemy = &g->players[enemy_idx];
    if (!enemy->alive) return 0;

    /* AI技能使用：优先使用武将技能 */
    if(p->hero && p->hero->ai_use_skill)
    {
        int used = p->hero->ai_use_skill(g, player_idx);
        if(used) return 1;
    }

    for(int i = 0; i < p->hand_count; i++)
    {
        Card* c = p->hand[i];
        if(c->type == CARD_EQUIP)
        {
            EquipType et = c->sub.equip.equip_type;
            int has_slot = 0;
            if(et == EQUIP_WEAPON && !p->equip.weapon) has_slot =1;
            if(et == EQUIP_ARMOR && !p->equip.armor) has_slot =1;
            if(et == EQUIP_HORSE_ATK && !p->equip.horse_atk) has_slot =1;
            if(et == EQUIP_HORSE_DEF && !p->equip.horse_def) has_slot =1;
            if(has_slot)
            {
                game_equip_card(g, player_idx, i);
                return 1;
            }
        }
    }

    typedef struct{ int hand_idx; int score; int is_trick; }AiCandidate;
    AiCandidate cand[32];
    int cand_cnt = 0;

    /* AI状态评估 */
    int my_hp_ratio = p->hp * 100 / p->max_hp;  /* 自己血量百分比 */
    int enemy_hp_ratio = enemy->hp * 100 / enemy->max_hp;  /* 对手血量百分比 */
    int enemy_hand = enemy->hand_count;  /* 对手手牌数 */
    int dist = game_calc_distance(g, player_idx, enemy_idx);
    int range = player_attack_range(p);
    int can_sha = (dist <= range && p->sha_used < 1);

    for(int i=0; i < p->hand_count; i++)
    {
        Card* c = p->hand[i];
        int score = 0, is_trick = 0, valid = 1;

        if(c->type == CARD_BASIC)
        {
            switch(c->sub.basic.basic_type)
            {
            case BASIC_SHA:{
                int max_sha = 1;
                if(player_weapon_type(p) == WEAPON_ZHUGELIANNU)
                    max_sha = 999;
                else if(p->hero && p->hero->sha_bonus)
                    max_sha += p->hero->sha_bonus(p);
                if(dist <= range && p->sha_used < max_sha)
                {
                    /* 基础分5，对手血量越低分越高，自己有酒时加分 */
                    score = 5 + (100 - enemy_hp_ratio) / 20;
                    if(p->jiu_used) score += 3;  /* 酒后杀伤害高 */
                    if(enemy_hp_ratio <= 30) score += 5;  /* 对手濒死，优先击杀 */
                }
                else valid = 0;
                break;
            }
            case BASIC_TAO:
                /* 血量越低，吃桃优先级越高 */
                if(p->hp < p->max_hp)
                {
                    score = 6 + (100 - my_hp_ratio) / 10;
                    if(my_hp_ratio <= 30) score += 5;  /* 自己濒死，必须吃桃 */
                }
                else valid = 0;
                break;
            case BASIC_JIU:
                /* 有杀且能打到人时，酒的价值高 */
                if(!p->jiu_used && p->hp > 1 && can_sha)
                    score = 4 + (100 - enemy_hp_ratio) / 25;
                else valid = 0;
                break;
            case BASIC_SHAN: valid = 0; break;  /* 出牌阶段不出闪 */
            default: valid=0;
            }
        }
        else if(c->type == CARD_TRICK)
        {
            is_trick =1;
            switch(c->sub.trick.trick_type)
            {
            case TRICK_WUZHONG:
                score = 8;
                if(p->hand_count <= 2) score += 3;  /* 手牌少时优先摸牌 */
                break;
            case TRICK_GUOHE:
                if(enemy_hand > 0 || enemy->equip.weapon || enemy->equip.armor)
                {
                    score = 7;
                    if(enemy->equip.weapon && !can_sha) score += 3;  /* 先拆武器再杀 */
                    if(enemy_hand >= 3) score += 2;
                }
                else valid = 0;
                break;
            case TRICK_SHUNSHOU:{
                if(dist <= 1 && (enemy_hand > 0 || enemy->equip.weapon))
                {
                    score = 7;
                    if(enemy->equip.weapon && !can_sha) score += 3;
                }
                else valid = 0;
                break;
            }
            case TRICK_JUEDOU:
                score = 6;
                if(enemy_hand <= 1) score += 3;  /* 对手没杀时决斗强 */
                if(enemy_hp_ratio <= 30) score += 2;
                break;
            case TRICK_NANMAN:
            case TRICK_WANJIAN:
                score = 8;
                if(enemy_hand <= 1) score += 2;  /* 对手没响应牌时群体锦囊强 */
                break;
            case TRICK_HUOGONG:
                if(enemy_hand > 0)
                {
                    score = 6;
                    if(enemy->equip.armor && player_armor_type(enemy) == ARMOR_TENGJIA)
                        score += 4;  /* 藤甲怕火攻 */
                }
                else valid = 0;
                break;
            case TRICK_TAOYUAN:
                if(p->hp < p->max_hp && enemy_hp_ratio > 50)
                    score = 3;  /* 自己血量低且对手血量高时才用桃园 */
                else valid = 0;
                break;
            case TRICK_WUGU:
                score = 5;
                if(p->hand_count <= 2) score += 2;
                break;
            case TRICK_TIESUO:
                score = 2;
                if(!enemy->chained && !p->chained) score += 1;
                break;
            case TRICK_WUXIE: valid = 0; break;  /* 出牌阶段不出无懈可击 */
            default: valid=0;
            }
        }
        else if(c->type == CARD_DELAYED)
        {
            score = 4;
            is_trick = 1;
            if(enemy->judge.count >= 3) valid = 0;  /* 判定区满了 */
        }

        if(valid && score > 0)
        {
            cand[cand_cnt].hand_idx = i;
            cand[cand_cnt].score = score;
            cand[cand_cnt].is_trick = is_trick;
            cand_cnt++;
        }
    }

    if(cand_cnt == 0) return 0;

    int best_idx = 0;
    for(int k=1;k<cand_cnt;k++)
    {
        AiCandidate* curr = &cand[k];
        AiCandidate* best = &cand[best_idx];
        if(curr->score > best->score) best_idx = k;
        else if(curr->score == best->score && curr->is_trick > best->is_trick) best_idx = k;
    }

    int select_hand_idx = cand[best_idx].hand_idx;
    Card* sel_card = p->hand[select_hand_idx];

    int target = -1;
    if(sel_card->type == CARD_BASIC && sel_card->sub.basic.basic_type == BASIC_SHA)
        target = enemy_idx;
    else if(sel_card->type == CARD_TRICK)
    {
        switch(sel_card->sub.trick.trick_type)
        {
        case TRICK_GUOHE: case TRICK_SHUNSHOU:
        case TRICK_JUEDOU: case TRICK_HUOGONG:
            target = enemy_idx; break;
        default: target = -1;
        }
    }
    else if(sel_card->type == CARD_DELAYED)
        target = enemy_idx;

    game_use_card(g, player_idx, select_hand_idx, target);
    return 1;
}


void game_judge_advance(GameState* g)
{
    if(!g || !g->judge_active) return;
    Player* p = &g->players[g->current_player];

    if(g->judge_step == 0)
    {
        g->judge_delay = p->judge.cards[g->judge_idx];
        g->judge_card = deck_draw(&g->deck);
        g->central_show_card = g->judge_card;
        if(g->judge_card)
            game_log(g, "%s 判定【%s】，判定牌：%s",
                     p->name, card_get_full_name(g->judge_delay), card_get_full_name(g->judge_card));
        g->judge_step = 1;
    }
    else if(g->judge_step == 1)
    {
        DelayedType dt = g->judge_delay->sub.delayed.delayed_type;
        g->judge_delay_action = 0;

        if(dt == DELAYED_LEBU)
        {
            if(g->judge_card && g->judge_card->suit != SUIT_HEART)
            {
                p->skip_play = 1;
                snprintf(g->judge_result, sizeof(g->judge_result),
                         "判中！%s跳过出牌阶段", p->name);
            }
            else
            {
                snprintf(g->judge_result, sizeof(g->judge_result),
                         "未判中（红桃），失效");
            }
        }
        else if(dt == DELAYED_BINGLIANG)
        {
            if(g->judge_card && g->judge_card->suit != SUIT_CLUB)
            {
                p->skip_draw = 1;
                snprintf(g->judge_result, sizeof(g->judge_result),
                         "判中！%s跳过摸牌阶段", p->name);
            }
            else
            {
                snprintf(g->judge_result, sizeof(g->judge_result),
                         "未判中（梅花），失效");
            }
        }
        else if(dt == DELAYED_SHANDIAN)
        {
            if(g->judge_card && g->judge_card->suit == SUIT_SPADE &&
               g->judge_card->rank >= RANK_2 && g->judge_card->rank <= RANK_9)
            {
                snprintf(g->judge_result, sizeof(g->judge_result),
                         "判中！%s受到3点雷电伤害", p->name);
                game_deal_damage(g, g->current_player, 3, -1, DMG_THUNDER);
            }
            else
            {
                g->judge_delay_action = 1;
                snprintf(g->judge_result, sizeof(g->judge_result),
                         "未判中，闪电传下家");
            }
        }

        game_log(g, "【%s】%s", card_get_full_name(g->judge_delay), g->judge_result);
        g->judge_step = 2;
    }
    else if(g->judge_step == 2)
    {
        if(g->judge_delay_action == 0)
        {
            p->judge.cards[g->judge_idx] = NULL;
            discard_add(&g->discard, g->judge_delay);
        }
        else
        {
            p->judge.cards[g->judge_idx] = NULL;
            int next = (g->current_player + 1) % g->player_count;
            int tries = 0;
            while(!g->players[next].alive && tries < g->player_count)
            {
                next = (next + 1) % g->player_count;
                tries++;
            }
            if(g->players[next].alive && next != g->current_player &&
               g->players[next].judge.count < MAX_JUDGE_CARDS)
            {
                g->players[next].judge.cards[g->players[next].judge.count++] = g->judge_delay;
                game_log(g, "闪电移动到 %s 的判定区", g->players[next].name);
            }
            else
            {
                discard_add(&g->discard, g->judge_delay);
            }
        }

        int write = 0;
        for(int i = 0; i <= p->judge.count; i++)
        {
            if(p->judge.cards[i] != NULL)
                p->judge.cards[write++] = p->judge.cards[i];
        }
        p->judge.count = write;

        if(g->judge_card) discard_add(&g->discard, g->judge_card);

        g->judge_card = NULL;
        g->judge_delay = NULL;
        g->judge_result[0] = '\0';
        g->judge_delay_action = 0;

        if(p->judge.count > 0 && p->alive)
        {
            g->judge_idx = p->judge.count - 1;
            g->judge_step = 0;
        }
        else
        {
            g->judge_active = 0;
            g->phase = PHASE_DRAW;
            game_log(g, "%s 的回合 - 摸牌阶段", p->name);
            if(p->skip_draw) {
                game_log(g, "%s 跳过摸牌阶段", p->name);
                p->skip_draw = 0;
                game_next_phase(g);
            } else {
                /* 武将技能：摸牌数加成 */
                int draw_num = 2;
                if(p->hero && p->hero->draw_bonus)
                    draw_num += p->hero->draw_bonus(p);
                game_draw_cards(g, g->current_player, draw_num);
            }
        }
    }
}


void game_next_phase(GameState* g)
{
    if (!g || g->game_over) return;
    Player* p = &g->players[g->current_player];

    switch (g->phase) {
    case PHASE_PREPARE:
        game_log(g, "%s 的回合 - 准备阶段", p->name);
        /* 重置当前玩家的技能使用次数（每回合开始时） */
        hero_reset_skills(p);
        /* 武将技能：回合开始 */
        if(p->hero && p->hero->on_turn_start)
            p->hero->on_turn_start(g, g->current_player);
        /* 武将技能：任何角色回合开始时触发（如圣骑士神圣护盾） */
        for(int i = 0; i < g->player_count; i++) {
            Player* hp = &g->players[i];
            if(hp->alive && hp->hero && hp->hero->on_any_turn_start)
                hp->hero->on_any_turn_start(g, i, g->current_player);
            /* 化形②：重置目标角色使用记录（每回合重置） */
            if(hp->alive && hp->hero_id == HERO_YUDIE)
            {
                hp->yudie.huaxing_target_used_suits = 0;
                for(int s = 0; s < 4; s++)
                    hp->yudie.huaxing_target_used_names[s][0] = '\0';
                hp->yudie.huaxing_response_used[g->current_player] = 0;
            }
        }
        /* 如果触发了需要玩家选择的状态（如神圣护盾），停止推进阶段 */
        if(g->resp_state == RESPONSE_NEED_PALADIN_CHOICE ||
           g->resp_state == RESPONSE_NEED_GENERIC_DISCARD)
        {
            game_log(g, "【神圣护盾】等待玩家选择，暂停阶段推进");
            break;
        }
        /* 准备阶段完成，自动推进到判定阶段 */
        g->phase = PHASE_JUDGE;
        game_next_phase(g);
        break;

    case PHASE_JUDGE:
        game_log(g, "%s 的回合 - 判定阶段", p->name);
        if(p->judge.count > 0)
        {
            g->judge_active = 1;
            g->judge_step = 0;
            g->judge_idx = p->judge.count - 1;
        }
        else
        {
            /* 没有判定牌，自动推进到摸牌阶段 */
            g->phase = PHASE_DRAW;
            game_next_phase(g);
        }
        break;

    case PHASE_DRAW:
        game_log(g, "%s 的回合 - 摸牌阶段", p->name);
        if(p->skip_draw) {
            game_log(g, "%s 跳过摸牌阶段", p->name);
            p->skip_draw = 0;
        } else {
            /* 武将技能：摸牌数加成 */
            int draw_num = 2;
            if(p->hero && p->hero->draw_bonus)
                draw_num += p->hero->draw_bonus(p);
            game_draw_cards(g, g->current_player, draw_num);
        }
        /* 摸牌完成后自动推进到出牌阶段 */
        g->phase = PHASE_PLAY;
        game_next_phase(g);
        break;

    case PHASE_PLAY:
        game_log(g, "%s 的回合 - 出牌阶段", p->name);
        if (p->skip_play) {
            game_log(g, "%s 跳过出牌阶段", p->name);
            p->skip_play = 0;
            game_next_phase(g);
        } else if (p->is_ai) {
            g->ai_play_finished = 0;
            game_log(g, "%s 的回合 - 出牌阶段（AI行动中）", p->name);
        }
        break;

    case PHASE_DISCARD:
        game_log(g, "%s 的回合 - 弃牌阶段", p->name);
        /* 玩家还在通用弃牌状态时，检查是否已完成 */
        if(g->resp_state == RESPONSE_NEED_GENERIC_DISCARD)
        {
            /* 弃牌进行中，不推进阶段 */
            break;
        }
        /* 弃牌已完成（或取消），自动推进到结束阶段 */
        if(g->generic_discard_done)
        {
            g->generic_discard_done = 0;
            g->phase = PHASE_END;
            game_next_phase(g);
            break;
        }
        /* 第一次进入弃牌阶段，初始化 */
        {
            int limit = game_effective_hand_limit(g, g->current_player);
            int need = p->hand_count - limit;
            if(need > 0)
            {
                if(g->current_player == 0)
                {
                    /* 玩家：使用通用主动弃牌函数，source=201表示弃牌阶段 */
                    game_start_generic_discard(g, 0, need, 201);
                    game_log(g, "【弃牌阶段】请选择%d张手牌弃置（手牌上限%d）", need, limit);
                }
                else
                {
                    /* AI：自动弃牌到上限 */
                    game_discard_auto(g, g->current_player);
                    /* AI弃牌完成后自动推进到结束阶段 */
                    g->phase = PHASE_END;
                    game_next_phase(g);
                }
            }
            else
            {
                game_log(g, "%s 手牌未超过上限（上限%d），无需弃牌", p->name, limit);
                /* 无需弃牌，自动推进到结束阶段 */
                g->phase = PHASE_END;
                game_next_phase(g);
            }
        }
        break;

    case PHASE_END:
        game_log(g, "%s 的回合 - 结束阶段", p->name);
        /* 武将技能：回合结束 */
        if(p->hero && p->hero->on_turn_end)
            p->hero->on_turn_end(g, g->current_player);
        /* 化形②：检查所有雨蝶是否指定当前角色为目标 */
        for(int i = 0; i < g->player_count; i++)
        {
            if(g->players[i].alive && g->players[i].hero_id == HERO_YUDIE)
            {
                yudie_huaxing_phase2_on_turn_end(g, i, g->current_player);
            }
        }
        /* 切换到下一个玩家 */
        g->current_player = (g->current_player + 1) % g->player_count;
        int tries = 0;
        while (!g->players[g->current_player].alive && tries < g->player_count) {
            g->current_player = (g->current_player + 1) % g->player_count;
            tries++;
        }
        if (g->current_player == 0) {
            g->turn_count++;
            /* 每轮开始：触发所有玩家的 on_round_start 回调（如凝盾） */
            for(int i = 0; i < g->player_count; i++) {
                Player* rp = &g->players[i];
                if(rp->alive && rp->hero && rp->hero->on_round_start)
                    rp->hero->on_round_start(g, i);
            }
        }

        Player* np = &g->players[g->current_player];
        game_reset_turn_state(np);
        g->phase = PHASE_PREPARE;
        game_log(g, "--- 第 %d 轮 ---", g->turn_count);
        /* 自动推进到新玩家的准备阶段 */
        game_next_phase(g);
        break;

    case PHASE_GAME_OVER:
        break;
    }
}


static int game_discard_check_valid(GameState* g, Card* c)
{
    int valid = c->is_valid;
    g->central_show_card = c;
    if (!valid) {
        game_log(g, "【%s】被无效，效果不生效", card_get_full_name(c));
    }
    /* 虚拟牌：直接释放内存，不进入弃牌堆 */
    if(c->card_nature == CARD_NATURE_VIRTUAL) {
        game_log(g, "【虚拟牌】释放内存，不进入弃牌堆");
        free(c);
    } else {
        discard_add(&g->discard, c);
    }
    return valid;
}


/* ================================================================
 * 内部辅助：清除火攻状态（pick/cancel 共用）
 * ================================================================ */
static void huogong_clear(GameState* g)
{
    g->huogong_active = 0;
    g->huogong_source = -1;
    g->huogong_target = -1;
    g->huogong_show_card = NULL;
    g->huogong_need_suit = 0;
    g->huogong_picked_hand = -1;
    g->resp_state = RESPONSE_NONE;
}


/* ================================================================
 * 使用牌（包装函数）：先调用内部实现，成功后才触发 on_card_used 回调
 * 这样可以避免不能使用的牌（如闪）也触发技能（如破茧摸牌）
 * ================================================================ */
int game_use_card(GameState* g, int player_idx, int hand_index, int target_idx)
{
    if (!g || g->game_over) return 0;
    if (player_idx < 0 || player_idx >= g->player_count) return 0;

    Player* p = &g->players[player_idx];
    if (hand_index < 0 || hand_index >= p->hand_count) return 0;

    /* 保存牌的指针（internal 可能会移除手牌） */
    Card* card = p->hand[hand_index];

    /* 调用内部实现 */
    int result = game_use_card_internal(g, player_idx, hand_index, target_idx);

    /* 只有使用成功后才触发 on_card_used 回调 */
    /* 如果 internal 中已经提前触发了（如杀对AI时伤害在internal内结算），则不重复调用 */
    if(result == 1 && card && p->hero && p->hero->on_card_used && !g->on_card_used_done)
        p->hero->on_card_used(g, player_idx, card);
    g->on_card_used_done = 0;  /* 重置标记 */

    /* 化形②：记录目标角色使用过的牌（供雨蝶化形②使用） */
    if(result == 1 && card)
    {
        for(int i = 0; i < g->player_count; i++)
        {
            if(g->players[i].alive && g->players[i].hero_id == HERO_YUDIE &&
               g->players[i].yudie.chengdie && g->players[i].yudie.huaxing_target == player_idx)
            {
                Player* yd = &g->players[i];
                if(card->suit >= 0 && card->suit < 4)
                {
                    yd->yudie.huaxing_target_used_suits |= (1 << card->suit);
                    strncpy(yd->yudie.huaxing_target_used_names[card->suit],
                            card_get_name(card), 31);
                    yd->yudie.huaxing_target_used_names[card->suit][31] = '\0';
                }
            }
        }
    }

    return result;
}

int game_use_card_internal(GameState* g, int player_idx, int hand_index, int target_idx)
{
    if (!g || g->game_over) return 0;
    if (player_idx < 0 || player_idx >= g->player_count) return 0;
    if (g->phase != PHASE_PLAY) return 0;
    if (g->current_player != player_idx) return 0;

    Player* p = &g->players[player_idx];
    if (hand_index < 0 || hand_index >= p->hand_count) return 0;

    Card* card = p->hand[hand_index];
    if (!card) return 0;

    /* 无懈可击：不能在出牌阶段主动使用，只能响应锦囊时使用 */
    if(card->type == CARD_TRICK && card->sub.trick.trick_type == TRICK_WUXIE)
    {
        game_log(g, "【无懈可击】只能在响应锦囊时使用，不能主动打出");
        return 0;
    }

    /* 武将技能：成为目标时触发（仅对有目标的牌） */
    if(target_idx >= 0 && target_idx < g->player_count && target_idx != player_idx)
    {
        Player* target_p = &g->players[target_idx];
        if(target_p->alive && target_p->hero && target_p->hero->on_becoming_target)
            target_p->hero->on_becoming_target(g, target_idx, player_idx, card);
    }

    if (card->type == CARD_EQUIP) {
        return game_equip_card(g, player_idx, hand_index);
    }

    if (card->type == CARD_BASIC) {
        /* 龙胆：闪当杀打出（检测到后转换牌类型并重新进入switch） */
        if(p->hero_id == HERO_ZHAOYUN && p->longdan_active &&
           card->sub.basic.basic_type == BASIC_SHAN)
        {
            card->sub.basic.basic_type = BASIC_SHA;
            game_log(g, "【龙胆】%s将【闪】当【杀】打出", p->name);
        }

        switch (card->sub.basic.basic_type) {
        case BASIC_SHA: {
            if (target_idx < 0 || target_idx >= g->player_count) return 0;
            if (target_idx == player_idx) return 0;
            if (!g->players[target_idx].alive) return 0;

            int dist = game_calc_distance(g, player_idx, target_idx);
            int range = player_attack_range(p);
            if (dist > range) {
                game_log(g, "距离不够，无法出杀！");
                return 0;
            }

            int max_sha = 1;
            if (player_weapon_type(p) == WEAPON_ZHUGELIANNU) {
                max_sha = 999;
            } else if(p->hero && p->hero->sha_bonus) {
                max_sha += p->hero->sha_bonus(p);
            }
            if (p->sha_used >= max_sha) {
                game_log(g, "本回合已不能再出杀！");
                return 0;
            }

            /* 仁王盾：黑色杀（黑桃/梅花）指定装备仁王盾的目标时，该杀无效 */
            Player* sha_target = &g->players[target_idx];
            if(player_has_armor(sha_target, ARMOR_RENWANG) &&
               (card->suit == SUIT_SPADE || card->suit == SUIT_CLUB))
            {
                card->is_valid = 0;
                game_log(g, "【仁王盾】%s的黑色杀被%s的仁王盾无效",
                         p->name, sha_target->name);
            }

            Card* sha_card = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, sha_card)) return 1;
            p->sha_used++;

            /* 提前触发 on_card_used 回调：
             * 对AI目标时伤害在本函数内直接结算，若等game_use_card返回后再触发，
             * 过载等"使用杀后、伤害结算前"的状态就来不及设置 */
            if(p->hero && p->hero->on_card_used) {
                g->on_card_used_done = 1;
                p->hero->on_card_used(g, player_idx, sha_card);
            }

            /* 龙胆：闪当杀打出后，触发虎威（摸2张牌），退出龙胆模式 */
            if(p->hero_id == HERO_ZHAOYUN && p->longdan_active)
            {
                p->longdan_active = 0;
                zhaoyun_huwei(g, player_idx);
            }

            game_log(g, "%s 对 %s 使用了【%s】",
                     p->name, g->players[target_idx].name, card_get_full_name(card));

            /* 朱雀羽扇：打出杀时，如果攻击者装备朱雀羽扇，进入朱雀羽扇发动阶段 */
            game_start_zhuque(g, player_idx, target_idx, sha_card);
            if(g->zhuque_active) return 1;  /* 朱雀羽扇发动中，等待玩家选择 */

            Player* target = &g->players[target_idx];
            /* 镜流·无罅飞光花色4：下一张杀不可响应 */
            int unblockable = (p->hero_id == HERO_JINGLIU && p->jingliu.next_sha_unblockable);
            if(unblockable) game_log(g, "【无罅飞光】此杀不可被响应");
            if(target->is_ai)
            {
                int shan_index = -1;
                if(!unblockable) {
                    for (int i = 0; i < target->hand_count; i++) {
                        if (target->hand[i]->type == CARD_BASIC &&
                            target->hand[i]->sub.basic.basic_type == BASIC_SHAN) {
                            shan_index = i; break;
                        }
                    }
                }
                if (shan_index >= 0) {
                    Card* shan_card = player_remove_hand(target, shan_index);
                    discard_add(&g->discard, shan_card);
                    g->central_show_card = shan_card;
                    game_log(g, "%s 打出了【%s】", target->name, card_get_full_name(shan_card));
                    /* 贯石斧：杀被闪后，如果攻击者装备贯石斧，进入贯石斧发动阶段 */
                    int dmg = game_calc_sha_damage(g, player_idx, target_idx);
                    game_start_guanshi(g, player_idx, target_idx, dmg);
                } else {
                    int dmg = game_calc_sha_damage(g, player_idx, target_idx);
                    /* 寒冰剑：杀造成伤害前，如果攻击者装备寒冰剑，进入寒冰剑发动阶段 */
                    game_start_hanbing(g, player_idx, target_idx, dmg);
                    if(!g->hanbing_active) {
                        /* 没有发动寒冰剑（未装备或牌不足），正常造成伤害 */
                        g->current_damage_source = DMG_SRC_SHA;
                        game_deal_damage(g, target_idx, dmg, player_idx, DMG_NORMAL);
                    }
                }
            }
            else
            {
                if(unblockable) {
                    /* 不可响应：直接造成伤害 */
                    int dmg = game_calc_sha_damage(g, player_idx, target_idx);
                    g->current_damage_source = DMG_SRC_SHA;
                    game_deal_damage(g, target_idx, dmg, player_idx, DMG_NORMAL);
                } else {
                    /* 八卦阵：目标需要出闪时，如果装备八卦阵，先进入八卦阵发动阶段 */
                    game_start_bagua(g, target_idx, player_idx, sha_card);
                    if(g->bagua_active) return 1;

                    g->resp_state = RESPONSE_NEED_BASIC;
                    g->resp_trigger_card = sha_card;
                    g->resp_source_player = player_idx;
                    g->resp_target_player = target_idx;
                    g->resp_required_basic = BASIC_SHAN;
                    g->resp_need_basic_after_wuxie = 0;
                    g->duel_turn = -1;
                    game_log(g, "请点击闪牌选中，点击确认打出，点击取消放弃");
                }
            }
            return 1;
        }

        case BASIC_TAO: {
            if (p->hp >= p->max_hp) {
                game_log(g, "体力已满，不能用桃！");
                return 0;
            }
            Card* tao_card = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, tao_card)) return 1;
            player_recover(p, 1);
            game_log(g, "%s 使用了【%s】，回复1点体力", p->name, card_get_full_name(tao_card));
            return 1;
        }

        case BASIC_JIU: {
            if (p->jiu_used) {
                game_log(g, "本回合已用过酒！");
                return 0;
            }
            if (p->hp < 1) {
                Card* jiu_card = player_remove_hand(p, hand_index);
                if (!game_discard_check_valid(g, jiu_card)) return 1;
                player_recover(p, 1);
                game_log(g, "%s 使用了【酒】，回复1点体力", p->name);
                return 1;
            }
            Card* jiu_card = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, jiu_card)) return 1;
            p->jiu_used = 1;
            game_log(g, "%s 使用了【酒】，本回合下一张杀伤害+1", p->name);
            return 1;
        }

        case BASIC_SHAN:
            game_log(g, "闪不能主动使用！");
            return 0;
        }
    }

    if (card->type == CARD_TRICK) {
        switch (card->sub.trick.trick_type) {
        case TRICK_WUZHONG: {
            Card* c = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, c)) return 1;
            game_log(g, "%s 使用了【无中生有】", p->name);
            /* 进入无懈可击询问 */
            g->single_trick_pending = 1;
            g->single_trick_type = TRICK_WUZHONG;
            g->single_trick_source = player_idx;
            g->single_trick_target = -1;
            g->single_trick_card = c;
            g->resp_state = RESPONSE_NEED_WUXIE;
            g->resp_trigger_card = c;
            g->resp_source_player = player_idx;
            g->resp_target_player = player_idx;  /* 无中生有目标是自己 */
            g->group_active = 0;
            if(p->is_ai)
            {
                /* AI使用：询问玩家是否打无懈 */
                game_log(g, "请点击【无懈可击】选中抵消无中生有，点击确认打出，点击取消放弃");
            }
            return 1;
        }

        case TRICK_GUOHE: {
            if (target_idx < 0 || target_idx >= g->player_count) return 0;
            if (target_idx == player_idx) return 0;
            Player* target = &g->players[target_idx];
            if (!target->alive) return 0;
            /* 检查对方是否有牌可拆（手牌+装备+延时锦囊） */
            int total_cards = target->hand_count;
            if(target->equip.weapon) total_cards++;
            if(target->equip.armor) total_cards++;
            if(target->equip.horse_atk) total_cards++;
            if(target->equip.horse_def) total_cards++;
            total_cards += target->judge.count;
            if (total_cards <= 0) {
                game_log(g, "对方没有牌可拆！");
                return 0;
            }
            Card* c = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, c)) return 1;
            game_log(g, "%s 对 %s 使用了【过河拆桥】", p->name, target->name);
            /* 进入无懈可击询问 */
            g->single_trick_pending = 1;
            g->single_trick_type = TRICK_GUOHE;
            g->single_trick_source = player_idx;
            g->single_trick_target = target_idx;
            g->single_trick_card = c;
            g->resp_state = RESPONSE_NEED_WUXIE;
            g->resp_trigger_card = c;
            g->resp_source_player = player_idx;
            g->resp_target_player = target_idx;
            g->group_active = 0;
            if(p->is_ai)
                game_log(g, "请点击【无懈可击】选中抵消过河拆桥，点击确认打出，点击取消放弃");
            return 1;
        }

        case TRICK_SHUNSHOU: {
            if (target_idx < 0 || target_idx >= g->player_count) return 0;
            if (target_idx == player_idx) return 0;
            int dist = game_calc_distance(g, player_idx, target_idx);
            if (dist > 1) {
                game_log(g, "距离超过1，不能顺手牵羊！");
                return 0;
            }
            Player* target = &g->players[target_idx];
            /* 检查对方是否有牌可牵（手牌+装备+延时锦囊） */
            int total_cards = target->hand_count;
            if(target->equip.weapon) total_cards++;
            if(target->equip.armor) total_cards++;
            if(target->equip.horse_atk) total_cards++;
            if(target->equip.horse_def) total_cards++;
            total_cards += target->judge.count;
            if (total_cards <= 0) {
                game_log(g, "对方没有手牌可牵！");
                return 0;
            }
            Card* c = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, c)) return 1;
            game_log(g, "%s 对 %s 使用了【顺手牵羊】", p->name, target->name);
            /* 进入无懈可击询问 */
            g->single_trick_pending = 1;
            g->single_trick_type = TRICK_SHUNSHOU;
            g->single_trick_source = player_idx;
            g->single_trick_target = target_idx;
            g->single_trick_card = c;
            g->resp_state = RESPONSE_NEED_WUXIE;
            g->resp_trigger_card = c;
            g->resp_source_player = player_idx;
            g->resp_target_player = target_idx;
            g->group_active = 0;
            if(p->is_ai)
                game_log(g, "请点击【无懈可击】选中抵消顺手牵羊，点击确认打出，点击取消放弃");
            return 1;
        }

        case TRICK_TAOYUAN: {
            Card* c = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, c)) return 1;
            game_log(g, "%s 使用【桃园结义】", p->name);
            g->group_active=1; g->group_phase=0; g->group_current=-1;
            g->group_source=player_idx; g->group_trick_type=TRICK_TAOYUAN;
            g->group_trigger_card=c; g->group_wuxie_mask=0;
            return 1;
        }

        case TRICK_TIESUO: {
            /* 铁索连环：选择1-2名目标，改变横置状态 */
            if(p->is_ai)
            {
                /* AI使用：自动选择目标并结算 */
                Card* c = player_remove_hand(p, hand_index);
                if (!game_discard_check_valid(g, c)) return 1;
                game_log(g, "%s 使用【铁索连环】", p->name);

                /* AI自动选择目标：选择玩家（和自己，如果需要） */
                int ai_enemy_idx = (player_idx == 0) ? 1 : 0;
                g->tiesuo_targets[0] = ai_enemy_idx;
                g->tiesuo_target_count = 1;
                /* 50%概率也选择自己（连环自己和玩家） */
                if(rand() % 2 == 0)
                {
                    g->tiesuo_targets[1] = player_idx;
                    g->tiesuo_target_count = 2;
                }

                /* 直接进入无懈可击询问流程 */
                g->tiesuo_wuxie_index = 0;
                g->tiesuo_wuxie_mask = 0;
                g->resp_state = RESPONSE_NEED_TIESUO_WUXIE;
                game_log(g, "【铁索连环】%s选择了%d名目标", p->name, g->tiesuo_target_count);
                /* 推进无懈可击询问 */
                game_tiesuo_wuxie_advance(g);
            }
            else
            {
                /* 玩家使用：进入选目标状态 */
                game_start_tiesuo_target(g, hand_index);
            }
            return 1;
        }

        case TRICK_JUEDOU: {
            if(target_idx < 0 || target_idx >= g->player_count || target_idx == player_idx) return 0;
            Player* defender = &g->players[target_idx];
            if(!defender->alive) return 0;

            Card* c = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, c)) return 1;
            game_log(g, "%s 对 %s 使用【决斗】", p->name, defender->name);

             if(defender->is_ai)
            {
                g->resp_state = RESPONSE_NEED_BASIC;
                g->resp_trigger_card = c;
                g->resp_source_player = player_idx;
                g->resp_target_player = target_idx;
                g->duel_turn = target_idx;
                g->resp_required_basic = BASIC_SHA;  /* 决斗需要出杀 */
                g->resp_need_basic_after_wuxie = 0;
            }
            else
            {
                g->resp_state = RESPONSE_NEED_WUXIE;
                g->resp_trigger_card = c;
                g->resp_source_player = player_idx;
                g->resp_target_player = target_idx;
                g->resp_need_basic_after_wuxie = 1;
                g->resp_required_basic = BASIC_SHA;
                g->duel_turn = -1;
                game_log(g, "请点击【无懈可击】选中，点击确认打出，点击取消放弃");
            }
            return 1;
        }

        case TRICK_WANJIAN: {
            Card* c = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, c)) return 1;
            game_log(g, "%s 使用【万箭齐发】！", p->name);
            g->group_active=1; g->group_phase=0; g->group_current=-1;
            g->group_source=player_idx; g->group_trick_type=TRICK_WANJIAN;
            g->group_trigger_card=c; g->group_wuxie_mask=0;
            return 1;
        }

        case TRICK_NANMAN: {
            Card* c = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, c)) return 1;
            game_log(g, "%s 使用【南蛮入侵】！", p->name);
            g->group_active=1; g->group_phase=0; g->group_current=-1;
            g->group_source=player_idx; g->group_trick_type=TRICK_NANMAN;
            g->group_trigger_card=c; g->group_wuxie_mask=0;
            return 1;
        }

        case TRICK_WUGU: {
            int alive_cnt = 0;
            for(int i=0;i<g->player_count;i++)
                if(g->players[i].alive) alive_cnt++;
            g->group_wugu_count = 0;
            for(int i=0;i<alive_cnt;i++)
            {
                if(g->deck.top >= g->deck.count) break;
                g->group_wugu_pile[g->group_wugu_count++] = deck_draw(&g->deck);
            }
            Card* c = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, c)) return 1;
            game_log(g, "%s 使用【五谷丰登】，亮出%d张牌", p->name, g->group_wugu_count);
            g->group_active=1; g->group_phase=0; g->group_current=-1;
            g->group_source=player_idx; g->group_trick_type=TRICK_WUGU;
            g->group_trigger_card=c; g->group_wuxie_mask=0;
            return 1;
        }

        /* ================================================================
         * 火攻：两阶段中断状态机
         *   发动时统一进入 SHOW 状态等待目标选牌展示
         *   AI目标由 game_update 自动随机展示，进入PICK
         *   AI使用者由 game_update 直接放弃（方案A），清除状态
         * ================================================================ */
        case TRICK_HUOGONG: {
            if(target_idx < 0 || target_idx >= g->player_count || target_idx == player_idx) return 0;
            Player* target = &g->players[target_idx];
            if(!target->alive || target->hand_count <= 0){
                game_log(g,"火攻目标无手牌，火攻失效");
                return 0;
            }
            Card* c = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, c)) return 1;
            game_log(g, "火攻：%s 对 %s 使用火攻", p->name, target->name);
            /* 进入无懈可击询问 */
            g->single_trick_pending = 1;
            g->single_trick_type = TRICK_HUOGONG;
            g->single_trick_source = player_idx;
            g->single_trick_target = target_idx;
            g->single_trick_card = c;
            g->resp_state = RESPONSE_NEED_WUXIE;
            g->resp_trigger_card = c;
            g->resp_source_player = player_idx;
            g->resp_target_player = target_idx;
            g->group_active = 0;
            if(p->is_ai)
                game_log(g, "请点击【无懈可击】选中抵消火攻，点击确认打出，点击取消放弃");
            return 1;
        }

        case TRICK_WUXIE: {
            game_log(g,"【无懈可击】：需要在锦囊结算回调中做抵消逻辑");
            Card* c = player_remove_hand(p, hand_index);
            if (!game_discard_check_valid(g, c)) return 1;
            return 1;
        }

        default:
            game_log(g, "【%s】暂未实现", card_get_full_name(card));
            return 0;
        }
    }

    if (card->type == CARD_DELAYED) {
        if(target_idx <0 || target_idx >= g->player_count)
        {
            game_log(g,"延时锦囊需要指定目标");
            return 0;
        }
        Player* tar = &g->players[target_idx];
        if(!tar->alive) return 0;

        /* 先检查：判定区是否已有相同类型的延时锦囊（不移除手牌） */
        DelayedType dt = card->sub.delayed.delayed_type;
        int has_same = 0;
        for(int i = 0; i < tar->judge.count; i++)
        {
            if(tar->judge.cards[i] && tar->judge.cards[i]->sub.delayed.delayed_type == dt)
            {
                has_same = 1;
                break;
            }
        }
        if(has_same)
        {
            game_log(g, "%s的判定区已有相同类型的延时锦囊，无法使用", tar->name);
            return 0;  /* 不移除手牌，不消耗牌 */
        }

        /* 检查判定区是否已满 */
        if(tar->judge.count >= MAX_JUDGE_CARDS)
        {
            game_log(g,"目标判定区已满，无法放置延时锦囊");
            return 0;  /* 不移除手牌，不消耗牌 */
        }

        /* 检查通过，才移除手牌 */
        Card* c = player_remove_hand(p, hand_index);
        if (!c->is_valid) {
            game_log(g, "【%s】被无效，效果不生效", card_get_full_name(c));
            discard_add(&g->discard, c);
            return 1;
        }
        g->central_show_card = c;

        tar->judge.cards[tar->judge.count++] = c;
        game_log(g,"将延时锦囊放置到%s的判定区", tar->name);
        return 1;
    }

    return 0;
}


int game_equip_card(GameState* g, int player_idx, int hand_index)
{
    if (!g || g->game_over) return 0;
    if (player_idx < 0 || player_idx >= g->player_count) return 0;
    if (g->phase != PHASE_PLAY) return 0;
    if (g->current_player != player_idx) return 0;

    Player* p = &g->players[player_idx];
    if (hand_index < 0 || hand_index >= p->hand_count) return 0;

    Card* card = p->hand[hand_index];
    if (!card || card->type != CARD_EQUIP) return 0;

    Card* c = player_remove_hand(p, hand_index);
    if (!c->is_valid) {
        game_log(g, "【%s】被无效，效果不生效", card_get_full_name(c));
        discard_add(&g->discard, c);
        return 1;
    }
    g->central_show_card = c;
    Card* old = player_equip(p, c);
    if (old) {
        discard_add(&g->discard, old);
    }
    game_log(g, "%s 装备了【%s】", p->name, card_get_full_name(c));
    return 1;
}


/* ================================================================
 * 实际手牌上限（包含武将技能修正）
 * 通用函数：所有需要计算手牌上限的地方都调用这个
 * 如雨蝶破茧：同花色手牌不计入手牌上限
 * ================================================================ */
int game_effective_hand_limit(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    int limit = player_hand_limit(p);

    /* 武将技能：手牌上限修正（如雨蝶破茧同花色牌不计入） */
    if(p->hero && p->hero->hand_limit_mod)
        limit += p->hero->hand_limit_mod(g, player_idx);

    return limit;
}

void game_discard_to_limit(GameState* g, int player_idx)
{
    if (!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    int limit = game_effective_hand_limit(g, player_idx);

    while (p->hand_count > limit) {
        Card* c = player_remove_hand(p, 0);
        if (c) discard_add(&g->discard, c);
    }
    game_log(g, "%s 弃牌，剩余 %d 张手牌", p->name, p->hand_count);
}


/* ===== 弃牌阶段：玩家手动弃一张牌 ===== */
void game_discard_one(GameState* g, int hand_index)
{
    if(!g || g->resp_state != RESPONSE_NEED_DISCARD) return;
    Player* p = &g->players[g->current_player];
    if(hand_index < 0 || hand_index >= p->hand_count) return;

    Card* c = player_remove_hand(p, hand_index);
    if(c) {
        discard_add(&g->discard, c);
        game_log(g, "你弃置了【%s】", card_get_full_name(c));
    }

    /* 每次弃牌后重新计算需要弃多少牌（因为同花色牌弃掉后手牌上限会变化） */
    int limit = game_effective_hand_limit(g, g->current_player);
    int need = p->hand_count - limit;
    if(need <= 0)
    {
        /* 弃够了，清除状态，进入结束阶段 */
        g->discard_need_count = 0;
        g->resp_state = RESPONSE_NONE;
        game_log(g, "弃牌完毕，剩余 %d 张手牌（上限%d）", p->hand_count, limit);
        game_next_phase(g);  /* 自动推进到结束阶段 */
    }
    else
    {
        g->discard_need_count = need;
        game_log(g, "还需弃 %d 张牌（上限%d）", need, limit);
    }
}


/* ================================================================
 * 通用主动弃牌（选牌→确认→弃牌）
 * 适用于所有需要主动选择弃牌的场景（圣骑士、贯石斧、寒冰剑等）
 * ================================================================ */

/* 开始通用弃牌选择 */
void game_start_generic_discard(GameState* g, int player_idx, int need_count, int source)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    if(need_count <= 0) return;

    g->resp_state = RESPONSE_NEED_GENERIC_DISCARD;
    g->generic_discard_player = player_idx;
    g->generic_discard_need = need_count;
    g->generic_discard_selected_count = 0;
    g->generic_discard_source = source;
    g->generic_discard_done = 0;
    memset(g->generic_discard_selected, -1, sizeof(g->generic_discard_selected));

    Player* p = &g->players[player_idx];
    game_log(g, "【通用弃牌】请选择%d张手牌弃置（点击选中，点击确认弃牌）", need_count);
}

/* 选中/取消选中一张牌 */
void game_generic_discard_pick(GameState* g, int hand_index)
{
    if(!g || g->resp_state != RESPONSE_NEED_GENERIC_DISCARD) return;
    Player* p = &g->players[g->generic_discard_player];
    if(hand_index < 0 || hand_index >= p->hand_count) return;

    /* 检查是否已选中 */
    for(int i = 0; i < g->generic_discard_selected_count; i++)
    {
        if(g->generic_discard_selected[i] == hand_index)
        {
            /* 取消选中：从数组中移除 */
            for(int j = i; j < g->generic_discard_selected_count - 1; j++)
            {
                g->generic_discard_selected[j] = g->generic_discard_selected[j + 1];
            }
            g->generic_discard_selected_count--;
            g->generic_discard_selected[g->generic_discard_selected_count] = -1;
            game_log(g, "【通用弃牌】取消选中【%s】", card_get_full_name(p->hand[hand_index]));
            return;
        }
    }

    /* 未选中：检查是否还能选 */
    if(g->generic_discard_selected_count >= g->generic_discard_need)
    {
        game_log(g, "【通用弃牌】已选满%d张，不能再选", g->generic_discard_need);
        return;
    }

    /* 选中 */
    g->generic_discard_selected[g->generic_discard_selected_count++] = hand_index;
    game_log(g, "【通用弃牌】选中【%s】（%d/%d）",
             card_get_full_name(p->hand[hand_index]),
             g->generic_discard_selected_count, g->generic_discard_need);
}

/* 确认弃牌 */
void game_generic_discard_confirm(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_GENERIC_DISCARD) return;
    if(g->generic_discard_selected_count < g->generic_discard_need)
    {
        game_log(g, "【通用弃牌】还需选择%d张牌", g->generic_discard_need - g->generic_discard_selected_count);
        return;
    }

    Player* p = &g->players[g->generic_discard_player];

    /* 按下标从大到小弃牌，避免下标偏移 */
    int indices[10];
    for(int i = 0; i < g->generic_discard_selected_count; i++)
        indices[i] = g->generic_discard_selected[i];

    /* 冒泡排序，从大到小 */
    for(int i = 0; i < g->generic_discard_selected_count - 1; i++)
    {
        for(int j = 0; j < g->generic_discard_selected_count - 1 - i; j++)
        {
            if(indices[j] < indices[j + 1])
            {
                int tmp = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = tmp;
            }
        }
    }

    /* 弃牌 */
    for(int i = 0; i < g->generic_discard_selected_count; i++)
    {
        Card* c = player_remove_hand(p, indices[i]);
        if(c)
        {
            discard_add(&g->discard, c);
            game_log(g, "【通用弃牌】弃置【%s】", card_get_full_name(c));
        }
    }

    /* 根据 source 执行后续逻辑 */
    int source = g->generic_discard_source;

    /* 完成 */
    g->generic_discard_done = 1;
    g->resp_state = RESPONSE_NONE;
    game_log(g, "【通用弃牌】完成，共弃置%d张牌", g->generic_discard_selected_count);

    /* 神圣护盾选项2/3：弃牌后执行盾数变化 */
    if(source == 102 || source == 103)
    {
        /* 重新获取圣骑士索引（从 paladin_choice_paladin_idx） */
        int paladin_idx = g->paladin_choice_paladin_idx;
        if(paladin_idx >= 0 && paladin_idx < g->player_count)
        {
            Player* paladin = &g->players[paladin_idx];
            if(paladin->hero_id == HERO_PALADIN)
            {
                int old_shield = paladin->shield;
                if(source == 102)  /* 选项2：+1盾 */
                {
                    paladin->shield += 1;
                    if(paladin->shield > MAX_SHIELD) paladin->shield = MAX_SHIELD;
                    game_log(g, "【神圣护盾】%s获得1盾（%d→%d）",
                             paladin->name, old_shield, paladin->shield);
                }
                else if(source == 103)  /* 选项3：-1盾 */
                {
                    paladin->shield -= 1;
                    if(paladin->shield < 0) paladin->shield = 0;
                    game_log(g, "【神圣护盾】%s失去1盾（%d→%d）",
                             paladin->name, old_shield, paladin->shield);
                }

                /* 盾数变化摸牌 */
                int delta = paladin->shield - old_shield;
                if(delta != 0)
                {
                    /* 调用 paladin_shield_changed（需要 extern 声明或直接实现） */
                    extern void paladin_shield_changed(GameState* g, int paladin_idx, int delta);
                    paladin_shield_changed(g, paladin_idx, delta);
                }

                /* 重置神圣护盾状态 */
                g->paladin_choice_paladin_idx = -1;
                g->paladin_choice_turn_idx = -1;

                /* 破灭护盾响应模式：恢复原响应状态 */
                extern void paladin_pomie_resume_response(GameState* g);
                paladin_pomie_resume_response(g);

                game_check_victory(g);

                /* 神圣护盾选择完成后，继续推进阶段（如果还在准备阶段且不是破灭护盾响应模式） */
                if(g->phase == PHASE_PREPARE && !g->game_over && g->pomie_mode == 0)
                {
                    int turn_idx = g->current_player;
                    g->phase = PHASE_JUDGE;
                    game_log(g, "%s 的回合 - 判定阶段", g->players[turn_idx].name);
                    game_next_phase(g);
                }
            }
        }
    }
}

/* 取消弃牌 */
void game_generic_discard_cancel(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_GENERIC_DISCARD) return;

    g->resp_state = RESPONSE_NONE;
    g->generic_discard_done = 0;
    g->generic_discard_selected_count = 0;
    memset(g->generic_discard_selected, -1, sizeof(g->generic_discard_selected));
    game_log(g, "【通用弃牌】取消弃牌");
}


/* ===== 弃牌阶段：AI自动弃牌到上限 ===== */
void game_discard_auto(GameState* g, int player_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];

    /* 每次弃牌后重新计算上限（因为同花色牌弃掉后上限会变化） */
    while(1)
    {
        int limit = game_effective_hand_limit(g, player_idx);
        if(p->hand_count <= limit) break;

        Card* c = player_remove_hand(p, 0);
        if(c) {
            discard_add(&g->discard, c);
            game_log(g, "%s 弃置了【%s】", p->name, card_get_full_name(c));
        }
    }
    game_log(g, "%s 弃牌完毕，剩余 %d 张手牌", p->name, p->hand_count);
}


/* ===== 杀伤害计算：基础1点 + 酒+1 + 古锭刀（目标无手牌）+1 ===== */
int game_calc_sha_damage(GameState* g, int attacker_idx, int target_idx)
{
    if(!g || attacker_idx < 0 || attacker_idx >= g->player_count ||
       target_idx < 0 || target_idx >= g->player_count) return 1;

    Player* attacker = &g->players[attacker_idx];
    Player* target = &g->players[target_idx];

    int dmg = 1;

    /* 酒：本回合下一张杀伤害+1 */
    if(attacker->jiu_used) {
        dmg++;
        attacker->jiu_used = 0;
    }

    /* 古锭刀：目标没有手牌时，杀伤害+1 */
    if(player_weapon_type(attacker) == WEAPON_GUDING &&
       target->hand_count == 0) {
        dmg++;
        game_log(g, "【古锭刀】%s没有手牌，伤害+1", target->name);
    }

    /* 镜流·无罅飞光花色2：杀伤害+1 */
    if(attacker->hero_id == HERO_JINGLIU && attacker->jingliu.sha_damage_plus > 0)
    {
        dmg += attacker->jingliu.sha_damage_plus;
        game_log(g, "【无罅飞光】%s杀伤害+%d", attacker->name, attacker->jingliu.sha_damage_plus);
    }

    return dmg;
}


/* ===== 无视防具判定（青缸剑效果） =====
 * 返回1：攻击者装备青缸剑，目标防具失效
 * 返回0：正常结算防具效果
 * 以后添加新防具（仁王盾/八卦阵/藤甲等）时，在结算前调用此函数判断
 */
int game_ignore_armor(GameState* g, int attacker_idx, int target_idx)
{
    if(!g || attacker_idx < 0 || attacker_idx >= g->player_count ||
       target_idx < 0 || target_idx >= g->player_count) return 0;

    Player* attacker = &g->players[attacker_idx];

    /* 青缸剑：攻击者装备青缸剑时，无视目标防具 */
    if(player_weapon_type(attacker) == WEAPON_QINGGANG)
    {
        return 1;
    }

    return 0;
}


/* 铁索连环传导标志：防止递归传导 */
static int g_chaining = 0;

void game_deal_damage(GameState* g, int target_idx, int amount, int source_idx, DamageType dmg_type)
{
    if(target_idx <0 || target_idx >= g->player_count) return;
    Player* victim = &g->players[target_idx];
    if(!victim->alive || amount <=0) return;
    /* 传导中的伤害不再触发新的传导（避免无限递归） */
    int was_chaining = g_chaining;

    /* 记录当前伤害来源角色（反伤将用） */
    g->current_damage_source_player = source_idx;

    /* 流萤·迸发：下次造成的伤害视为火属性/雷属性 */
    if(source_idx >= 0 && source_idx < g->player_count)
    {
        Player* src_p = &g->players[source_idx];
        if(src_p->alive && src_p->hero_id == HERO_LIUYING &&
           src_p->liuying.bengfa_element != 0)
        {
            if(src_p->liuying.bengfa_element == 1)
            {
                dmg_type = DMG_FIRE;
                game_log(g, "【迸发】%s的伤害视为火属性", src_p->name);
            }
            else if(src_p->liuying.bengfa_element == 2)
            {
                dmg_type = DMG_THUNDER;
                game_log(g, "【迸发】%s的伤害视为雷属性", src_p->name);
            }
            src_p->liuying.bengfa_element = 0;  /* 重置，只生效一次 */
        }
    }

    if(dmg_type == DMG_VIRTUAL)
    {
        victim->hidden_hp -= amount;
        if(victim->hidden_hp < 0) victim->hidden_hp = 0;
        game_log(g, "【视为伤害】%s 里体力 -%d（剩余 %d）", victim->name, amount, victim->hidden_hp);
        return;
    }

    /* 武将技能：花色免疫（玉盏效果，仅linyuxia） */
    if(victim->hero_id == HERO_LINYUXIA && victim->immune_suit != -1 &&
       g->central_show_card && g->central_show_card->suit == victim->immune_suit)
    {
        const char* suit_names[] = {"黑桃", "红桃", "梅花", "方块"};
        game_log(g, "【玉盏】%s免疫%s花色伤害，未受到伤害",
                 victim->name, suit_names[victim->immune_suit]);
        return;
    }

    /* 武将技能：造成伤害加成（来源有效时） */
    if(source_idx >= 0 && source_idx < g->player_count)
    {
        Player* source = &g->players[source_idx];
        if(source->hero && source->hero->damage_bonus)
        {
            int bonus = source->hero->damage_bonus(source);
            if(bonus > 0) {
                amount += bonus;
                game_log(g, "【%s】%s造成伤害+%d（共%d点）",
                         source->hero->name, source->name, bonus, amount);
            }
        }
    }

    /* 青缸剑：攻击者装备青缸剑时，无视目标防具（所有防具效果均不生效） */
    int ignore_armor = game_ignore_armor(g, source_idx, target_idx);

    /* ===== 藤甲防具效果（提前到武将技能减伤之前，避免免伤前触发琉璃摸牌） ===== */
    int has_tengjia = (!ignore_armor && player_has_armor(victim, ARMOR_TENGJIA));

    if(has_tengjia)
    {
        /* 藤甲效果1：受到火焰伤害时，此次伤害+1 */
        if(dmg_type == DMG_FIRE)
        {
            amount++;
            game_log(g, "【藤甲】%s受到火焰伤害，伤害+1（共%d点）", victim->name, amount);
        }
        /* 藤甲效果2：普通杀/南蛮入侵/万箭齐发造成的普通伤害免伤 */
        else if(dmg_type == DMG_NORMAL &&
                (g->current_damage_source == DMG_SRC_SHA ||
                 g->current_damage_source == DMG_SRC_NANMAN ||
                 g->current_damage_source == DMG_SRC_WANJIAN))
        {
            game_log(g, "【藤甲】%s免疫普通伤害，未受到伤害", victim->name);
            return;  /* 免伤：不扣血，不增加累计受伤次数，不触发武将减伤技能 */
        }
    }

    /* 白银狮子防具：伤害大于1则修正为1（青缸剑无视防具时不生效） */
    if(!ignore_armor && player_has_armor(victim, ARMOR_BAIYIN))
    {
        if(amount > 1)
        {
            game_log(g, "【白银狮子】%s受到%d点伤害，修正为1点", victim->name, amount);
            amount = 1;
        }
    }

    /* 确保hero指针正确（某些状态下可能为NULL） */
    if(!victim->hero && victim->hero_id >= 0)
    {
        victim->hero = hero_get(victim->hero_id);
    }

    /* 武将技能：受到伤害减免（琉璃等） */
    int damage_reduced = 0;
    if(victim->hero && victim->hero->damage_reduce)
    {
        damage_reduced = victim->hero->damage_reduce(g, target_idx, amount);
        if(damage_reduced > 0) {
            amount -= damage_reduced;
            if(amount < 0) amount = 0;
        }
    }
    /* 后备：基于hero_id的减伤（hero指针异常时使用） */
    else if(victim->hero_id == HERO_LINYUXIA && victim->shield > 0 && amount > 0)
    {
        damage_reduced = 1;
        amount -= 1;
        if(amount < 0) amount = 0;
        game_log(g, "【琉璃·后备】%s有盾，伤害减1", victim->name);
    }
    /* 伤害减免后触发（琉璃摸牌等） */
    if(damage_reduced > 0 && victim->hero && victim->hero->on_damage_reduced)
    {
        victim->hero->on_damage_reduced(g, target_idx, damage_reduced);
    }
    /* 减免后伤害为0，不继续结算 */
    if(amount <= 0) {
        game_log(g, "%s 的伤害被完全减免，未受到伤害", victim->name);
        return;
    }

    /* ===== 圣骑士：有盾时优先扣盾，不扣体力不濒死 ===== */
    if(victim->hero_id == HERO_PALADIN && victim->shield > 0)
    {
        paladin_take_damage(g, target_idx, amount);
        game_check_victory(g);
        return;
    }

    const char* type_str = "普通";
    if(dmg_type == DMG_FIRE) type_str = "火焰";
    if(dmg_type == DMG_THUNDER) type_str = "雷电";

    game_log(g,"【%s伤害】%s 受到 %d 点%s伤害", type_str, victim->name, amount, type_str);
    victim->hp -= amount;
    victim->damage_taken_count++;  /* 累计受伤次数+1（真正扣血才增加） */
    game_log(g,"%s 当前血量：%d（累计受伤%d次）", victim->name, victim->hp, victim->damage_taken_count);

    /* 流萤·过载：杀造成伤害时，回复2点体力 */
    if(g->current_damage_source == DMG_SRC_SHA && source_idx >= 0)
    {
        liuying_guozai_on_damage(g, source_idx);
    }

    game_dying_resolve(g, target_idx, source_idx);

    if(g->game_over) return;

    if(dmg_type == DMG_FIRE || dmg_type == DMG_THUNDER)
    {
        victim->chained = 0;

        int chain_list[32];
        int chain_cnt = 0;
        for(int step = 0; step < g->player_count; step++)
        {
            int idx = (g->current_player + step) % g->player_count;
            if(idx == target_idx) continue;
            Player* p = &g->players[idx];
            if(p->alive && p->chained == 1)
            {
                chain_list[chain_cnt++] = idx;
            }
        }

        const char* chain_type_str = (dmg_type == DMG_FIRE) ? "火焰" : "雷电";
        g_chaining = 1;  /* 标记传导开始 */
        for(int k = 0; k < chain_cnt; k++)
        {
            int chain_idx = chain_list[k];
            Player* cp = &g->players[chain_idx];
            if(!cp->alive) continue;

            cp->chained = 0;

            game_log(g,"铁索连环传导：%s受到%d点%s传导伤害", cp->name, amount, chain_type_str);
            /* 递归调用game_deal_damage，触发藤甲+1、武将减伤、防具等完整结算 */
            game_deal_damage(g, chain_idx, source_idx, amount, dmg_type);

            if(g->game_over) break;
        }
        g_chaining = was_chaining;  /* 恢复传导标志 */
    }
}


void game_dying_resolve(GameState* g, int victim_idx, int damage_source)
{
    Player* vic = &g->players[victim_idx];
    if(vic->hp > 0) return;

    /* 武将技能：进入濒死时触发（如雨蝶成蝶使命失败） */
    if(vic->hero && vic->hero->on_dying)
        vic->hero->on_dying(g, victim_idx);

    game_log(g,"%s 进入濒死状态！请求求桃", vic->name);

    while(vic->hp <= 0 && vic->alive)
    {
        int found_tao = -1;
        for(int i=0;i<vic->hand_count;i++)
        {
            Card* c = vic->hand[i];
            if(c->type == CARD_BASIC && c->sub.basic.basic_type == BASIC_TAO)
            {
                found_tao = i;
                break;
            }
        }
        int found_jiu = -1;
        for(int i=0;i<vic->hand_count;i++)
        {
            Card* c = vic->hand[i];
            if(c->type == CARD_BASIC && c->sub.basic.basic_type == BASIC_JIU)
            {
                found_jiu = i;
                break;
            }
        }

        if(found_tao != -1)
        {
            Card* c = player_remove_hand(vic, found_tao);
            discard_add(&g->discard, c);
            player_recover(vic,1);
            game_log(g,"%s 使用桃脱离濒死", vic->name);
        }
        else if(found_jiu != -1)
        {
            Card* c = player_remove_hand(vic, found_jiu);
            discard_add(&g->discard, c);
            player_recover(vic,1);
            game_log(g,"%s 使用酒脱离濒死", vic->name);
        }
        else
        {
            /* 镜流·魔阴：求桃结束后，确认死亡前进行判定 */
            if(vic->hero_id == HERO_JINGLIU && !vic->jingliu.dying_judged)
            {
                vic->jingliu.dying_judged = 1;
                jingliu_moyin_judge(g, victim_idx);
                /* 如果判定后回满了体力，不死亡，继续游戏 */
                if(vic->hp > 0)
                {
                    game_log(g, "【魔阴】%s 判定后回满体力，脱离濒死", vic->name);
                    break;
                }
            }
            vic->alive = 0;
            game_log(g,"%s 死亡！", vic->name);
            break;
        }
    }
    game_check_victory(g);
}


/* ================================================================
 * 伤害来源获取函数（反伤将用）
 * ================================================================ */

/* 获取当前伤害来源类型（DMG_SRC_*） */
int game_get_damage_source_type(GameState* g)
{
    if(!g) return DMG_SRC_OTHER;
    return g->current_damage_source;
}

/* 获取当前伤害来源角色下标（-1=无来源） */
int game_get_damage_source_player(GameState* g)
{
    if(!g) return -1;
    return g->current_damage_source_player;
}


/* ================================================================
 * 流失体力：直接扣体力，不触发任何伤害相关技能
 * 与 game_deal_damage 的区别：
 *   - 不触发伤害减免（琉璃、藤甲等）
 *   - 不触发防具效果
 *   - 不增加 damage_taken_count
 *   - 没有伤害来源
 *   - 到0时进入濒死求桃
 * ================================================================ */
void game_lose_hp(GameState* g, int player_idx, int amount)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(!p->alive || amount <= 0) return;

    /* ===== 圣骑士：有盾时完全免疫流失体力，不扣体力也不扣盾 ===== */
    if(p->hero_id == HERO_PALADIN && p->shield > 0)
    {
        game_log(g, "【流失体力】%s 有盾，免疫 %d 点体力流失", p->name, amount);
        return;
    }

    p->hp -= amount;
    if(p->hp < 0) p->hp = 0;

    game_log(g, "【流失体力】%s 流失 %d 点体力（剩余 %d）", p->name, amount, p->hp);

    /* 不增加 damage_taken_count（不是受到伤害） */
    /* 不触发伤害减免、防具、卖血技能 */

    /* 到0时进入濒死求桃 */
    if(p->hp <= 0)
    {
        game_log(g, "%s 体力流失殆尽，进入濒死状态", p->name);
        game_dying_resolve(g, player_idx, -1);  /* 来源-1表示无来源（流失体力） */
    }
}


void game_check_victory(GameState* g)
{
    if (!g) return;
    int alive_count = 0;
    int last_alive = -1;
    for (int i = 0; i < g->player_count; i++) {
        if (g->players[i].alive) {
            alive_count++;
            last_alive = i;
        }
    }
    if (alive_count == 1) {
        g->game_over = 1;
        g->winner_id = last_alive;
        g->phase = PHASE_GAME_OVER;
        game_log(g, "===== %s 获胜！=====", g->players[last_alive].name);
        game_log(g, "按 R 键重新开始");
    }
}


int game_play_card(GameState* game, int hand_index) {
    if (hand_index < 0 || hand_index >= game->players[game->current_player].hand_count)
        return -1;
    return 0;
}

int game_can_play_card(const GameState* game) {
    if (game->phase != PHASE_PLAY) return 0;
    if (game->players[game->current_player].hp <= 0) return 0;
    return 1;
}


/* ================================================================
 * game_update：每帧调用
 * 火攻AI自动结算在这里：
 *   SHOW状态且目标是AI → 自动随机展示一张牌，进入PICK
 *   PICK状态且使用者是AI → 直接放弃（方案A），清除状态
 * ================================================================ */
/* ================================================================
 * 设置AI操作延迟（0.75秒=45帧@60fps）
 * ================================================================ */
static void game_set_ai_delay(GameState* g)
{
    if(!g) return;
    g->ai_delay = 45;
    g->ai_delay_active = 1;
}

void game_update(GameState* g)
{
    if(!g || g->game_over) return;

    /* AI操作延迟：延迟期间不执行任何AI操作 */
    if(g->ai_delay_active)
    {
        g->ai_delay--;
        if(g->ai_delay <= 0)
        {
            g->ai_delay_active = 0;
            g->ai_delay = 0;
        }
        return;
    }

    /* AI回合：自动推进判定（不需要玩家按空格） */
    if(g->judge_active && g->players[g->current_player].is_ai)
    {
        game_set_ai_delay(g);  /* 每次判定推进间隔0.75秒 */
        game_judge_advance(g);
        return;
    }

    /* 屏幕中心展示牌：计时器递减，到期清除 */
    if(g->show_card_timer > 0)
    {
        g->show_card_timer--;
        if(g->show_card_timer <= 0)
        {
            g->show_card_center = NULL;
            g->show_card_who[0] = '\0';
        }
    }

    /* 屏幕中心文字提示：计时器递减，到期清除 */
    if(g->center_message_timer > 0)
    {
        g->center_message_timer--;
        if(g->center_message_timer <= 0)
            g->center_message[0] = '\0';
    }

    /* 屏幕中心批量展示牌：计时器递减，到期清除 */
    if(g->show_cards_timer > 0)
    {
        g->show_cards_timer--;
        if(g->show_cards_timer <= 0)
        {
            for(int i = 0; i < 10; i++)
                g->show_cards_center[i] = NULL;
            g->show_cards_count = 0;
            g->show_cards_who[0] = '\0';
        }
    }

    /* 镜流·狂乱：任何时候手牌数大于4，立刻弃置至4张并获得等量薨标记 */
    for(int i = 0; i < g->player_count; i++)
    {
        Player* jp = &g->players[i];
        if(jp->alive && jp->hero_id == HERO_JINGLIU &&
           jp->jingliu.transformation == JINGLIU_FORM_NORMAL &&
           jp->hand_count > 4 &&
           g->resp_state == RESPONSE_NONE)
        {
            jingliu_kuangluan_check(g, i);
        }
    }

    /* 通用主动弃牌完成后的处理 */
    if(g->generic_discard_done)
    {
        g->generic_discard_done = 0;
        int source = g->generic_discard_source;
        int player = g->generic_discard_player;

        /* 圣骑士神圣护盾 source=102/103 已在 game_generic_discard_confirm 里处理，这里跳过 */
        /* 贯石斧：source=200表示贯石斧弃2牌强制命中 */
        if(source == 200)
        {
            if(g->guanshi_active)
            {
                game_guanshi_confirm(g);
            }
        }
        /* 弃牌阶段：source=201表示弃牌阶段弃牌，完成后无需额外处理 */
        else if(source == 201)
        {
            game_log(g, "【弃牌阶段】弃牌完成");
        }
        /* 镜流狂乱：source=300表示狂乱弃牌 */
        else if(source == 300)
        {
            jingliu_kuangluan_discard_done(g, player);
        }
    }

    /* 火攻：AI自动结算 */
    if(g->huogong_active)
    {
        Player* hg_target = &g->players[g->huogong_target];
        Player* hg_source = &g->players[g->huogong_source];

        /* 阶段1：目标选牌展示。如果目标是AI，自动随机选一张展示 */
        if(g->resp_state == RESPONSE_NEED_HUOGONG_SHOW && hg_target->is_ai)
        {
            game_set_ai_delay(g);
            if(hg_target->hand_count > 0)
            {
                int show_idx = rand() % hg_target->hand_count;
                Card* show_card = hg_target->hand[show_idx];
                g->huogong_show_card = show_card;
                g->huogong_need_suit = show_card->suit;
                g->central_show_card = show_card;
                game_log(g, "火攻：%s 展示手牌【%s】",
                         hg_target->name, card_get_full_name(show_card));
            }
            /* 进入阶段2：等待使用者选牌 */
            g->huogong_picked_hand = -1;
            g->resp_state = RESPONSE_NEED_HUOGONG_PICK;
            return;
        }

        /* 阶段2：使用者选牌。如果使用者是AI，直接放弃（方案A） */
        if(g->resp_state == RESPONSE_NEED_HUOGONG_PICK && hg_source->is_ai)
        {
            game_set_ai_delay(g);
            game_log(g, "%s放弃火攻，不造成伤害", hg_source->name);
            huogong_clear(g);
            return;
        }
    }

    /* 单体锦囊无懈可击：如果目标是AI，AI自动决定是否打无懈 */
    if(g->resp_state == RESPONSE_NEED_WUXIE && !g->group_active &&
       g->single_trick_pending && g->players[g->resp_target_player].is_ai)
    {
        game_set_ai_delay(g);
        Player* ai = &g->players[g->resp_target_player];
        int wuxie_idx = -1;
        for(int h = 0; h < ai->hand_count; h++)
        {
            if(ai->hand[h]->type == CARD_TRICK &&
               ai->hand[h]->sub.trick.trick_type == TRICK_WUXIE)
            {
                wuxie_idx = h; break;
            }
        }

        if(wuxie_idx != -1 && rand() % 10 < 6)
        {
            /* AI打无懈可击 */
            Card* w = player_remove_hand(ai, wuxie_idx);
            discard_add(&g->discard, w);
            g->central_show_card = w;
            game_log(g, "%s 打出【无懈可击】！", ai->name);
            /* 询问玩家是否反无懈 */
            g->group_wuxie_counter_from = g->resp_target_player;
            g->resp_state = RESPONSE_NEED_WUXIE;
            g->resp_target_player = 0;  /* 现在需要玩家响应 */
            g->group_active = 1;  /* 标记为群体无懈模式，复用反无懈逻辑 */
            game_log(g, "请点击【无懈可击】选中抵消%s的无懈可击，点击确认打出，点击取消放弃", ai->name);
        }
        else
        {
            /* AI不打无懈，直接执行锦囊效果 */
            game_log(g, "%s 不使用无懈可击", ai->name);
            /* 模拟玩家选择不打无懈，续接执行锦囊效果 */
            int trick_type = g->single_trick_type;
            int source = g->single_trick_source;
            int target = g->single_trick_target;
            Card* card = g->single_trick_card;
            g->single_trick_pending = 0;
            g->single_trick_card = NULL;
            g->resp_state = RESPONSE_NONE;

            if(trick_type == TRICK_WUZHONG)
            {
                game_draw_cards(g, source, 2);
                discard_add(&g->discard, card);
                game_log(g, "%s 使用无中生有，摸2张牌", g->players[source].name);
            }
            else if(trick_type == TRICK_GUOHE)
            {
                discard_add(&g->discard, card);
                game_start_pick_enemy_card(g, source, target, 0);
            }
            else if(trick_type == TRICK_SHUNSHOU)
            {
                discard_add(&g->discard, card);
                game_start_pick_enemy_card(g, source, target, 1);
            }
            else if(trick_type == TRICK_HUOGONG)
            {
                g->huogong_active = 1;
                g->huogong_source = source;
                g->huogong_target = target;
                g->huogong_show_card = NULL;
                g->huogong_need_suit = 0;
                g->resp_state = RESPONSE_NEED_HUOGONG_SHOW;
                g->single_trick_card = card;
            }
        }
        return;
    }

    /* 群体锦囊优先 */
    if(g->group_active && g->resp_state == RESPONSE_NONE)
    {
        game_set_ai_delay(g);
        game_group_advance(g);
        return;
    }

    /* 贯石斧：AI自动发动（弃前2张手牌强制命中） */
    if(g->resp_state == RESPONSE_NEED_GUANSHI && g->guanshi_active &&
       g->players[g->guanshi_source].is_ai)
    {
        game_set_ai_delay(g);
        Player* ai = &g->players[g->guanshi_source];
        if(ai->hand_count >= 2)
        {
            /* AI自动发动：弃前2张手牌 */
            g->guanshi_picked[0] = 0;
            g->guanshi_picked[1] = 1;
            g->guanshi_picked_count = 2;
            game_guanshi_confirm(g);
        }
        else
        {
            /* 手牌不足，取消发动 */
            game_guanshi_cancel(g);
        }
        return;
    }

    /* 寒冰剑：AI自动决定是否发动（对方牌多时发动弃牌免伤，否则正常造成伤害） */
    if(g->resp_state == RESPONSE_NEED_HANBING && g->hanbing_active &&
       g->players[g->hanbing_source].is_ai)
    {
        game_set_ai_delay(g);
        Player* ai = &g->players[g->hanbing_source];
        Player* target = &g->players[g->hanbing_target];

        /* 计算对方牌总数 */
        int target_card_count = target->hand_count;
        if(target->equip.weapon) target_card_count++;
        if(target->equip.armor) target_card_count++;
        if(target->equip.horse_atk) target_card_count++;
        if(target->equip.horse_def) target_card_count++;

        /* AI策略：对方牌>=3张时有70%概率发动寒冰剑（弃2牌免伤），否则正常造成伤害 */
        if(target_card_count >= 3 && rand() % 10 < 7)
        {
            /* AI自动发动：选对方前2张牌（优先装备，再手牌） */
            int picked = 0;
            /* 先选装备 */
            int equip_types[4] = {1, 2, 3, 4};
            for(int i = 0; i < 4 && picked < 2; i++)
            {
                int has_equip = 0;
                if(equip_types[i] == 1 && target->equip.weapon) has_equip = 1;
                if(equip_types[i] == 2 && target->equip.armor) has_equip = 1;
                if(equip_types[i] == 3 && target->equip.horse_atk) has_equip = 1;
                if(equip_types[i] == 4 && target->equip.horse_def) has_equip = 1;
                if(has_equip)
                {
                    g->hanbing_picked_type[picked] = equip_types[i];
                    g->hanbing_picked_index[picked] = 0;
                    picked++;
                }
            }
            /* 再选手牌 */
            for(int i = 0; i < target->hand_count && picked < 2; i++)
            {
                g->hanbing_picked_type[picked] = 0;
                g->hanbing_picked_index[picked] = i;
                picked++;
            }
            g->hanbing_picked_count = picked;
            if(picked >= 2)
                game_hanbing_confirm(g);
            else
                game_hanbing_cancel(g);
        }
        else
        {
            /* AI不发动寒冰剑，正常造成伤害 */
            game_hanbing_cancel(g);
        }
        return;
    }

    /* 过河拆桥/顺手牵羊：AI自动选择对方的一张牌 */
    if(g->resp_state == RESPONSE_NEED_PICK_ENEMY_CARD &&
       g->players[g->current_player].is_ai)
    {
        Player* target = &g->players[g->pick_enemy_target];

        /* AI策略：优先选手牌，其次装备，最后延时锦囊 */
        if(target->hand_count > 0)
        {
            game_pick_enemy_card(g, 0, 0);
            game_confirm_pick_enemy_card(g);
        }
        else if(target->equip.weapon)
        {
            game_pick_enemy_card(g, 1, 0);
            game_confirm_pick_enemy_card(g);
        }
        else if(target->equip.armor)
        {
            game_pick_enemy_card(g, 2, 0);
            game_confirm_pick_enemy_card(g);
        }
        else if(target->equip.horse_atk)
        {
            game_pick_enemy_card(g, 3, 0);
            game_confirm_pick_enemy_card(g);
        }
        else if(target->equip.horse_def)
        {
            game_pick_enemy_card(g, 4, 0);
            game_confirm_pick_enemy_card(g);
        }
        else if(target->judge.count > 0)
        {
            game_pick_enemy_card(g, 5, 0);
            game_confirm_pick_enemy_card(g);
        }
        else
        {
            game_cancel_pick_enemy_card(g);
        }
        return;
    }

    /* 朱雀羽扇：AI自动发动（70%概率将杀变成火杀） */
    if(g->resp_state == RESPONSE_NEED_ZHUQUE && g->zhuque_active &&
       g->players[g->zhuque_source].is_ai)
    {
        if(rand() % 10 < 7)
        {
            /* AI发动朱雀羽扇，杀变成火杀 */
            game_zhuque_confirm(g);
        }
        else
        {
            /* AI不发动，普通杀继续结算 */
            game_zhuque_cancel(g);
        }
        return;
    }

    /* 八卦阵：AI自动发动（70%概率进行判定） */
    if(g->resp_state == RESPONSE_NEED_BAGUA && g->bagua_active &&
       g->players[g->bagua_source].is_ai)
    {
        if(rand() % 10 < 7)
        {
            /* AI发动八卦阵，进行判定 */
            game_bagua_confirm(g);
        }
        else
        {
            /* AI不发动，继续结算杀 */
            game_bagua_cancel(g);
        }
        return;
    }

    /* 决斗状态机 */
    if(g->resp_state == RESPONSE_NEED_BASIC && g->duel_turn != -1)
    {
        int cur_idx = g->duel_turn;

        if(cur_idx < 0 || cur_idx >= g->player_count)
        {
            game_clear_duel(g);
            return;
        }

        Player* cur = &g->players[cur_idx];
        if(!cur->alive)
        {
            game_clear_duel(g);
            return;
        }

        if(cur->is_ai)
        {
            int sha_idx = -1;
            for(int i=0;i<cur->hand_count;i++)
                if(cur->hand[i]->type == CARD_BASIC &&
                   cur->hand[i]->sub.basic.basic_type == BASIC_SHA)
                { sha_idx = i; break; }

            if(sha_idx != -1)
            {
                Card* sha = player_remove_hand(cur, sha_idx);
                discard_add(&g->discard, sha);
                g->central_show_card = sha;
                game_log(g, "%s 打出一张【杀】响应决斗", cur->name);
                g->duel_turn = (cur_idx == g->resp_target_player) ?
                                g->resp_source_player : g->resp_target_player;
            }
            else
            {
                game_log(g, "%s 无法打出杀，受到决斗1点伤害", cur->name);
                g->current_damage_source = DMG_SRC_JUEDOU;
                game_deal_damage(g, cur_idx, 1, g->resp_source_player, DMG_NORMAL);
                game_clear_duel(g);
                game_check_victory(g);
            }
        }
        return;
    }

    /* AI出牌：每帧出一张 */
    if(g->phase == PHASE_PLAY &&
       g->players[g->current_player].is_ai &&
       g->resp_state == RESPONSE_NONE)
    {
        int played = game_ai_try_play_one(g, g->current_player);
        if(!played && !g->ai_play_finished)
        {
            g->ai_play_finished = 1;
            game_log(g, "%s 出牌完毕", g->players[g->current_player].name);
        }
        /* 每次出牌后设置延迟，下次出牌前等待0.75秒 */
        if(played)
        {
            game_set_ai_delay(g);
        }
    }

    /* AI出牌完成后，推进到弃牌阶段 */
    if(g->phase == PHASE_PLAY &&
       g->players[g->current_player].is_ai &&
       g->ai_play_finished &&
       g->resp_state == RESPONSE_NONE &&
       !g->ai_delay_active)
    {
        game_log(g, "%s 结束出牌阶段，进入弃牌阶段", g->players[g->current_player].name);
        g->phase = PHASE_DISCARD;
        game_next_phase(g);
    }
}


void game_group_advance(GameState* g)
{
    if(!g || !g->group_active) return;

    while(1)
    {
        g->group_current++;
        if(g->group_current >= g->player_count)
        {
            if(g->group_phase == 0)
            {
                g->group_phase = 1;
                g->group_current = -1;
                continue;
            }
            else
            {
                for(int s=0;s<g->group_wugu_count;s++)
                    discard_add(&g->discard, g->group_wugu_pile[s]);
                g->group_active = 0;
                g->group_trigger_card = NULL;
                g->group_wuxie_mask = 0;
                g->group_wuxie_counter_from = -1;
                g->group_wugu_count = 0;
                g->resp_state = RESPONSE_NONE;
                return;
            }
        }

        /* 无懈可击阶段（phase=0）：使用者不需要对自己的锦囊打无懈，跳过使用者 */
        /* 选牌阶段（phase=1）：五谷丰登使用者也需要选牌，不跳过；其他锦囊使用者跳过 */
        if(g->group_phase == 0 && g->group_current == g->group_source) continue;
        if(g->group_phase == 1 && g->group_trick_type != TRICK_WUGU && g->group_current == g->group_source) continue;
        if(!g->players[g->group_current].alive) continue;

        if(g->group_phase == 1 &&
           (g->group_wuxie_mask & (1 << g->group_current)))
            continue;

        break;
    }

    int cur = g->group_current;
    Player* p = &g->players[cur];

    if(g->group_phase == 0)
    {
        if(p->is_ai)
            {
                int wuxie_idx = -1;
                for(int h=0;h<p->hand_count;h++)
                    if(p->hand[h]->type==CARD_TRICK &&
                       p->hand[h]->sub.trick.trick_type==TRICK_WUXIE)
                    { wuxie_idx=h; break; }
                if(wuxie_idx!=-1 && rand()%10 < 7)
                {
                    Card* w=player_remove_hand(p,wuxie_idx);
                    discard_add(&g->discard,w);
                    g->central_show_card=w;
                    g->group_wuxie_mask |= (1<<cur);
                    g->group_wuxie_counter_from = cur;  /* 标记AI打了无懈，等待玩家反无懈 */
                    game_log(g,"%s 打出无懈可击！",p->name);
                    /* 询问玩家是否反无懈 */
                    g->resp_state = RESPONSE_NEED_WUXIE;
                    g->resp_target_player = 0;  /* 需要响应的是玩家自己，不是AI */
                    g->resp_source_player = g->group_source;
                    g->resp_trigger_card = g->group_trigger_card;
                    game_log(g, "请点击【无懈可击】选中抵消%s的无懈可击，点击确认打出，点击取消放弃", p->name);
                    return;
                }
                else
                {
                    game_log(g,"%s 不使用无懈可击",p->name);
                    g->group_wuxie_counter_from = -1;
                }
                game_group_advance(g);
                return;
            }
        else
        {
            g->resp_state = RESPONSE_NEED_WUXIE;
            g->resp_target_player = cur;
            g->resp_source_player = g->group_source;
            g->resp_trigger_card = g->group_trigger_card;
            game_log(g, "%s 请点击【无懈可击】选中，点击确认打出，点击取消放弃", p->name);
            return;
        }
    }

    switch(g->group_trick_type)
    {
    case TRICK_NANMAN: {
        /* 被无懈可击抵消：直接推进，不询问出杀 */
        if(g->group_wuxie_mask & (1 << cur)) {
            game_log(g, "%s 被无懈可击抵消，不受南蛮入侵影响", p->name);
            game_group_advance(g);
            return;
        }
        if(p->is_ai)
        {
            int has = 0;
            for(int h=0;h<p->hand_count;h++)
            {
                if(p->hand[h]->type==CARD_BASIC &&
                   p->hand[h]->sub.basic.basic_type==BASIC_SHA)
                {
                    has=1;
                    Card* sha=player_remove_hand(p,h);
                    discard_add(&g->discard,sha);
                    g->central_show_card=sha;
                    game_log(g,"%s 打出【杀】响应南蛮入侵",p->name);
                    break;
                }
            }
            if(!has)
            {
                game_log(g,"%s 无法打出杀，受到1点伤害",p->name);
                g->current_damage_source = DMG_SRC_NANMAN;
                game_deal_damage(g,cur,1,g->group_source,DMG_NORMAL);
            }
            game_group_advance(g);
        }
        else
        {
            g->resp_state=RESPONSE_NEED_BASIC;
            g->resp_target_player=cur;
            g->resp_source_player=g->group_source;
            g->resp_trigger_card=g->group_trigger_card;
            g->resp_required_basic=BASIC_SHA;
            g->duel_turn=-1;
            game_log(g,"%s 请点击【杀】选中响应南蛮入侵，点击确认打出，点击取消放弃",p->name);
        }
        return;
    }
    case TRICK_WANJIAN: {
        /* 被无懈可击抵消：直接推进，不询问出闪 */
        if(g->group_wuxie_mask & (1 << cur)) {
            game_log(g, "%s 被无懈可击抵消，不受万箭齐发影响", p->name);
            game_group_advance(g);
            return;
        }
        if(p->is_ai)
        {
            int has=0;
            for(int h=0;h<p->hand_count;h++)
            {
                if(p->hand[h]->type==CARD_BASIC &&
                   p->hand[h]->sub.basic.basic_type==BASIC_SHAN)
                {
                    has=1;
                    Card* shan=player_remove_hand(p,h);
                    discard_add(&g->discard,shan);
                    g->central_show_card=shan;
                    game_log(g,"%s 打出【闪】响应万箭齐发",p->name);
                    break;
                }
            }
            if(!has)
            {
                game_log(g,"%s 无法打出闪，受到1点伤害",p->name);
                g->current_damage_source = DMG_SRC_WANJIAN;
                game_deal_damage(g,cur,1,g->group_source,DMG_NORMAL);
            }
            game_group_advance(g);
            return;
        }
        else
        {
            g->resp_state=RESPONSE_NEED_BASIC;
            g->resp_target_player=cur;
            g->resp_source_player=g->group_source;
            g->resp_trigger_card=g->group_trigger_card;
            g->resp_required_basic=BASIC_SHAN;
            g->duel_turn=-1;
            game_log(g, "%s 请点击闪牌选中，点击确认打出，点击取消放弃", p->name);
        }
        return;
    }
    case TRICK_TAOYUAN: {
        /* 被无懈可击抵消：不回血 */
        if(g->group_wuxie_mask & (1 << cur)) {
            game_log(g, "%s 被无懈可击抵消，不回复体力", p->name);
            game_group_advance(g);
            return;
        }
        if(p->hp < p->max_hp)
        {
            player_recover(p,1);
            game_log(g,"%s 回复1点体力",p->name);
        }
        game_group_advance(g);
        return;
    }
    case TRICK_WUGU:
        /* 被无懈可击抵消：不选牌 */
        if(g->group_wuxie_mask & (1 << cur)) {
            game_log(g, "%s 被无懈可击抵消，不能选五谷丰登的牌", p->name);
            game_group_advance(g);
            return;
        }
        if(p->is_ai)
        {
            if(g->group_wugu_count > 0)
            {
                Card* get = g->group_wugu_pile[0];
                for(int s=0;s<g->group_wugu_count-1;s++)
                    g->group_wugu_pile[s] = g->group_wugu_pile[s+1];
                g->group_wugu_count--;
                player_draw_card(p, get);
                game_log(g,"%s 获取五谷丰登亮出的牌",p->name);
            }
            game_group_advance(g);
        }
        else
        {
            g->resp_state = RESPONSE_NEED_WUGU_PICK;
            g->resp_target_player = cur;
            game_log(g,"%s 请点击选择一张五谷丰登的牌（1.5秒）",p->name);
            /* 启动1.5秒倒计时，超时自动选第一张牌 */
            g->countdown.active = 1;
            g->countdown.remaining = 1.5f;
            g->countdown.duration = 1.5f;
            g->countdown.callback_type = 1;  /* 1=五谷丰登选牌超时 */
        }
        return;
    default:
        game_group_advance(g);
        return;
    }
}


void game_clear_duel(GameState* g)
{
    g->resp_state = RESPONSE_NONE;
    g->duel_turn = -1;
    g->resp_source_player = -1;
    g->resp_target_player = -1;
    g->resp_trigger_card = NULL;
    g->resp_need_basic_after_wuxie = 0;
    g->resp_required_basic = 0;
    g->group_active = 0;
    g->group_phase = 0;
    g->group_current = -1;
    g->group_source = -1;
    g->group_trick_type = 0;
    g->group_trigger_card = NULL;
    g->group_wuxie_mask = 0;
}


/* ================================================================
 * 火攻阶段1结算：目标(玩家)左键点击一张手牌展示
 * 调用时机：input.c 里玩家左键点击手牌时（仅 SHOW 状态）
 * 效果：记录展示的牌和花色，central_show_card 指向该牌
 *       然后进入 PICK 状态等待使用者选牌
 *       （AI使用者由 game_update 自动结算，AI目标由 game_update 自动展示）
 * ================================================================ */
void input_handle_huogong_show(GameState* g, int hand_index)
{
    if(!g || !g->huogong_active) return;
    if(g->resp_state != RESPONSE_NEED_HUOGONG_SHOW) return;

    Player* target = &g->players[g->huogong_target];

    if(hand_index < 0 || hand_index >= target->hand_count) {
        game_log(g, "火攻：无效的手牌选择");
        return;
    }

    Card* show_card = target->hand[hand_index];
    g->huogong_show_card = show_card;
    g->huogong_need_suit = show_card->suit;
    g->central_show_card = show_card;

    game_log(g, "%s 展示手牌【%s】", target->name, card_get_full_name(show_card));

    /* 进入阶段2：等待使用者选牌（AI使用者由game_update自动结算） */
    g->resp_state = RESPONSE_NEED_HUOGONG_PICK;
}


/* ================================================================
 * 火攻阶段2：使用者(玩家)左键点击一张手牌选中（不立即结算）
 * 花色匹配：选中该手牌，等待点击确定
 * 花色不匹配：提示花色不匹配
 * ================================================================ */
void input_handle_huogong_pick(GameState* g, int hand_index)
{
    if(!g || !g->huogong_active) return;
    if(g->resp_state != RESPONSE_NEED_HUOGONG_PICK) return;

    Player* source = &g->players[g->huogong_source];

    if(hand_index < 0 || hand_index >= source->hand_count) {
        game_log(g, "火攻：无效的手牌选择");
        return;
    }

    Card* chosen = source->hand[hand_index];
    if(chosen->suit == g->huogong_need_suit) {
        /* 花色匹配：选中该手牌 */
        g->huogong_picked_hand = hand_index;
        game_log(g, "选中【%s】，点击确定弃置并造成伤害，点击取消放弃",
                 card_get_full_name(chosen));
    } else {
        /* 花色不匹配：取消选中 */
        g->huogong_picked_hand = -1;
        game_log(g, "【%s】花色不匹配，无法弃置", card_get_full_name(chosen));
    }
}

/* ================================================================
 * 火攻阶段2：点击确定按钮，确认弃置选中的手牌并造成伤害
 * ================================================================ */
void input_handle_huogong_confirm(GameState* g)
{
    if(!g || !g->huogong_active) return;
    if(g->resp_state != RESPONSE_NEED_HUOGONG_PICK) return;
    if(g->huogong_picked_hand < 0) return;

    Player* source = &g->players[g->huogong_source];
    Player* target = &g->players[g->huogong_target];

    int hand_index = g->huogong_picked_hand;
    if(hand_index < 0 || hand_index >= source->hand_count) {
        game_log(g, "火攻：无效的手牌选择");
        huogong_clear(g);
        return;
    }

    Card* burn = player_remove_hand(source, hand_index);
    discard_add(&g->discard, burn);
    game_log(g, "%s弃置同花色手牌【%s】，对%s造成1点火焰伤害",
             source->name, card_get_full_name(burn), target->name);
    g->current_damage_source = DMG_SRC_HUOGONG;
    game_deal_damage(g, g->huogong_target, 1, g->huogong_source, DMG_FIRE);

    huogong_clear(g);
}


/* ================================================================
 * 火攻阶段2结算：使用者(玩家)右键放弃
 * ================================================================ */
void input_handle_huogong_cancel(GameState* g)
{
    if(!g || !g->huogong_active) return;
    if(g->resp_state != RESPONSE_NEED_HUOGONG_PICK) return;

    game_log(g, "%s放弃火攻，不造成伤害", g->players[g->huogong_source].name);

    huogong_clear(g);
}


/* ================================================================
 * 贯石斧：杀被闪后，弃两张手牌强制命中
 * 流程：
 *   1. 杀被闪 → game_start_guanshi（贯石斧发亮，等待点击）
 *   2. 左键点击贯石斧 → game_guanshi_click_weapon（进入选牌阶段）
 *   3. 左键点击手牌 → game_guanshi_pick_card（选牌，选满2张自动确认）
 *   4. 右键点击贯石斧 → game_guanshi_cancel（取消发动）
 * ================================================================ */

/* 清除贯石斧状态 */
void game_guanshi_clear(GameState* g)
{
    if(!g) return;
    g->guanshi_active = 0;
    g->guanshi_source = -1;
    g->guanshi_target = -1;
    g->guanshi_damage = 0;
    g->guanshi_picking = 0;
    g->guanshi_picked_count = 0;
    g->guanshi_picked[0] = -1;
    g->guanshi_picked[1] = -1;
    g->resp_state = RESPONSE_NONE;
}

/* 开始贯石斧发动阶段（杀被闪后调用） */
void game_start_guanshi(GameState* g, int source_idx, int target_idx, int damage)
{
    if(!g) return;
    if(source_idx < 0 || source_idx >= g->player_count) return;
    if(target_idx < 0 || target_idx >= g->player_count) return;

    Player* source = &g->players[source_idx];

    /* 必须装备贯石斧 */
    if(player_weapon_type(source) != WEAPON_GUANSHI)
        return;

    /* 必须有至少2张手牌才能发动 */
    if(source->hand_count < 2)
    {
        game_log(g, "%s手牌不足2张，无法发动贯石斧", source->name);
        return;
    }

    g->guanshi_active = 1;
    g->guanshi_source = source_idx;
    g->guanshi_target = target_idx;
    g->guanshi_damage = damage;
    g->guanshi_picking = 0;
    g->guanshi_picked_count = 0;
    g->guanshi_picked[0] = -1;
    g->guanshi_picked[1] = -1;
    g->resp_state = RESPONSE_NEED_GUANSHI;

    game_log(g, "【贯石斧】%s打出的杀被闪避，是否发动贯石斧？（左键点击贯石斧弃2牌强制命中，右键取消）",
             source->name);
}

/* 左键点击贯石斧：进入通用弃牌选择（弃2张牌强制命中） */
void game_guanshi_click_weapon(GameState* g)
{
    if(!g || !g->guanshi_active) return;
    if(g->resp_state != RESPONSE_NEED_GUANSHI) return;

    /* 调用通用弃牌函数，弃2张牌，source=200表示贯石斧 */
    game_start_generic_discard(g, g->guanshi_source, 2, 200);
    game_log(g, "【贯石斧】请选择2张手牌弃置（选满后点击确认弃牌强制命中）");
}

/* 贯石斧造成伤害（通用弃牌完成后调用） */
void game_guanshi_confirm(GameState* g)
{
    if(!g || !g->guanshi_active) return;

    Player* source = &g->players[g->guanshi_source];
    Player* target = &g->players[g->guanshi_target];

    game_log(g, "【贯石斧】%s弃置2张手牌，强制命中！", source->name);

    /* 造成原本应造成的伤害 */
    g->current_damage_source = DMG_SRC_SHA;
    game_deal_damage(g, g->guanshi_target, g->guanshi_damage, g->guanshi_source, DMG_NORMAL);

    game_guanshi_clear(g);
}

/* 右键点击贯石斧：取消发动 */
void game_guanshi_cancel(GameState* g)
{
    if(!g || !g->guanshi_active) return;

    /* 如果正在通用弃牌阶段，先取消弃牌 */
    if(g->resp_state == RESPONSE_NEED_GENERIC_DISCARD)
    {
        game_generic_discard_cancel(g);
    }

    game_log(g, "【贯石斧】%s取消发动贯石斧", g->players[g->guanshi_source].name);
    game_guanshi_clear(g);
}


/* ================================================================
 * 雨蝶飞舞：选择要置入装备区的手牌（0-4张）
 * 流程：
 *   1. 点击飞舞技能 → game_start_feiwuu_pick（进入选牌状态）
 *   2. 左键点击手牌 → game_feiwuu_pick_card（选中/取消选中）
 *   3. 点击确定按钮 → game_feiwuu_confirm（确认选择，执行结算）
 *   4. 右键 → game_feiwuu_cancel（取消选择）
 * ================================================================ */

/* 进入飞舞选牌状态 */
void game_start_feiwuu_pick(GameState* g)
{
    if(!g) return;
    g->resp_state = RESPONSE_NEED_FEIWUU_PICK;
    g->feiwuu_selected_count = 0;
    for(int i = 0; i < 4; i++) g->feiwuu_selected[i] = -1;
    game_log(g, "【飞舞】请选择要置入装备区的手牌（0-4张），点击确定确认");
}

/* 选中/取消选中一张手牌 */
void game_feiwuu_pick_card(GameState* g, int hand_idx)
{
    if(!g || g->resp_state != RESPONSE_NEED_FEIWUU_PICK) return;
    if(hand_idx < 0) return;

    /* 检查是否已经选中 */
    for(int i = 0; i < g->feiwuu_selected_count; i++)
    {
        if(g->feiwuu_selected[i] == hand_idx)
        {
            /* 取消选中：前移覆盖 */
            for(int j = i; j < g->feiwuu_selected_count - 1; j++)
                g->feiwuu_selected[j] = g->feiwuu_selected[j + 1];
            g->feiwuu_selected_count--;
            g->feiwuu_selected[g->feiwuu_selected_count] = -1;
            game_log(g, "【飞舞】取消选择第%d张手牌", hand_idx + 1);
            return;
        }
    }

    /* 未选中：检查是否已满4张 */
    if(g->feiwuu_selected_count >= 4)
    {
        game_log(g, "【飞舞】最多选择4张手牌");
        return;
    }

    /* 选中 */
    g->feiwuu_selected[g->feiwuu_selected_count++] = hand_idx;
    game_log(g, "【飞舞】选择第%d张手牌（已选%d张）", hand_idx + 1, g->feiwuu_selected_count);
}

/* 确认选择，进入拖拽放置状态 */
void game_feiwuu_confirm(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_FEIWUU_PICK) return;

    Player* p = &g->players[g->current_player];

    /* 确认时才真正使用技能（增加使用次数，设置active=1） */
    if(!hero_skill_use(g, g->current_player, 1))
    {
        game_feiwuu_cancel(g);
        return;
    }

    int selected_count = g->feiwuu_selected_count;

    game_log(g, "【飞舞】确认选择%d张手牌，请拖拽到装备区", selected_count);

    /* 将选中的手牌按从左到右（下标从小到大）排序 */
    int sorted[4];
    for(int i = 0; i < selected_count; i++) sorted[i] = g->feiwuu_selected[i];
    for(int i = 0; i < selected_count - 1; i++)
        for(int j = i + 1; j < selected_count; j++)
            if(sorted[i] > sorted[j])
            {
                int tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }

    /* 从右往左移除手牌（避免下标变化） */
    Card* to_equip[4];
    int equip_count = 0;
    for(int i = selected_count - 1; i >= 0; i--)
    {
        Card* c = player_remove_hand(p, sorted[i]);
        if(c) to_equip[equip_count++] = c;
    }
    /* to_equip 现在是倒序的，需要反转 */
    for(int i = 0; i < equip_count / 2; i++)
    {
        Card* tmp = to_equip[i];
        to_equip[i] = to_equip[equip_count - 1 - i];
        to_equip[equip_count - 1 - i] = tmp;
    }

    /* 如果选中0张牌，直接进入亮牌结算，不需要拖拽 */
    if(equip_count == 0)
    {
        game_log(g, "【飞舞】未选择手牌，直接亮牌");
        /* 重置选牌状态 */
        g->feiwuu_selected_count = 0;
        for(int i = 0; i < 4; i++) g->feiwuu_selected[i] = -1;
        game_feiwuu_finish_resolve(g);
        return;
    }

    /* 进入拖拽放置状态 */
    g->resp_state = RESPONSE_NEED_FEIWUU_DRAG;
    g->feiwuu_drag_count = equip_count;
    for(int i = 0; i < equip_count; i++)
        g->feiwuu_drag_cards[i] = to_equip[i];
    g->feiwuu_drag_index = -1;
    g->feiwuu_dragging = 0;
    g->feiwuu_drag_x = 0;
    g->feiwuu_drag_y = 0;
    for(int i = 0; i < 4; i++)
        g->feiwuu_placed_slots[i] = 0;

    /* 重置选牌状态 */
    g->feiwuu_selected_count = 0;
    for(int i = 0; i < 4; i++) g->feiwuu_selected[i] = -1;

    game_log(g, "【飞舞】请将%d张牌拖拽到对应的装备格子中（虚线框）", equip_count);
}

/* 进入拖拽放置状态（空函数，实际在confirm里进入） */
void game_feiwuu_start_drag(GameState* g)
{
    /* 空函数，实际在 game_feiwuu_confirm 里进入拖拽状态 */
}

/* 开始拖拽一张牌 */
void game_feiwuu_begin_drag(GameState* g, int card_idx, int mx, int my)
{
    if(!g || g->resp_state != RESPONSE_NEED_FEIWUU_DRAG) return;
    if(card_idx < 0 || card_idx >= g->feiwuu_drag_count) return;
    if(!g->feiwuu_drag_cards[card_idx]) return;

    g->feiwuu_drag_index = card_idx;
    g->feiwuu_dragging = 1;
    g->feiwuu_drag_x = mx;
    g->feiwuu_drag_y = my;
    game_log(g, "【飞舞】开始拖拽【%s】", card_get_full_name(g->feiwuu_drag_cards[card_idx]));
}

/* 更新拖拽位置 */
void game_feiwuu_update_drag(GameState* g, int mx, int my)
{
    if(!g || !g->feiwuu_dragging) return;
    g->feiwuu_drag_x = mx;
    g->feiwuu_drag_y = my;
}

/* 结束拖拽，检测放置位置 */
void game_feiwuu_end_drag(GameState* g, int mx, int my)
{
    if(!g || !g->feiwuu_dragging) return;

    int slot_idx = game_feiwuu_hit_equip_slot(g, mx, my);
    if(slot_idx >= 0)
    {
        /* 放置到装备槽 */
        game_feiwuu_place_card(g, slot_idx);
    }
    else
    {
        /* 没有放置到装备槽，取消拖拽 */
        game_log(g, "【飞舞】未放置到装备区，牌回到待放置区");
        g->feiwuu_dragging = 0;
        g->feiwuu_drag_index = -1;
    }
}

/* 将牌放置到指定装备槽 */
void game_feiwuu_place_card(GameState* g, int slot_idx)
{
    if(!g || g->resp_state != RESPONSE_NEED_FEIWUU_DRAG) return;
    if(g->feiwuu_drag_index < 0 || g->feiwuu_drag_index >= g->feiwuu_drag_count) return;
    if(slot_idx < 0 || slot_idx >= 4) return;
    if(g->feiwuu_placed_slots[slot_idx])
    {
        game_log(g, "【飞舞】该装备槽已放置牌");
        g->feiwuu_dragging = 0;
        g->feiwuu_drag_index = -1;
        return;
    }

    Player* p = &g->players[0];
    Card* c = g->feiwuu_drag_cards[g->feiwuu_drag_index];
    if(!c) return;

    /* 如果该槽已有牌，将原牌弃置 */
    if(slot_idx == 0 && p->equip.weapon)
    {
        discard_add(&g->discard, p->equip.weapon);
        game_log(g, "【飞舞】原武器【%s】被弃置", card_get_full_name(p->equip.weapon));
    }
    else if(slot_idx == 1 && p->equip.armor)
    {
        discard_add(&g->discard, p->equip.armor);
        game_log(g, "【飞舞】原防具【%s】被弃置", card_get_full_name(p->equip.armor));
    }
    else if(slot_idx == 2 && p->equip.horse_atk)
    {
        discard_add(&g->discard, p->equip.horse_atk);
        game_log(g, "【飞舞】原进攻马【%s】被弃置", card_get_full_name(p->equip.horse_atk));
    }
    else if(slot_idx == 3 && p->equip.horse_def)
    {
        discard_add(&g->discard, p->equip.horse_def);
        game_log(g, "【飞舞】原防御马【%s】被弃置", card_get_full_name(p->equip.horse_def));
    }

    /* 放置新牌 */
    if(slot_idx == 0) p->equip.weapon = c;
    else if(slot_idx == 1) p->equip.armor = c;
    else if(slot_idx == 2) p->equip.horse_atk = c;
    else if(slot_idx == 3) p->equip.horse_def = c;

    /* 标记为飞舞放置，不生效 */
    p->equip.feiwuu_placed[slot_idx] = 1;

    g->feiwuu_placed_slots[slot_idx] = 1;
    g->feiwuu_drag_cards[g->feiwuu_drag_index] = NULL;
    g->feiwuu_dragging = 0;
    g->feiwuu_drag_index = -1;

    const char* slot_names[] = {"武器", "防具", "进攻马", "防御马"};
    game_log(g, "【飞舞】将【%s】放置到%s槽", card_get_full_name(c), slot_names[slot_idx]);

    /* 检查是否所有牌都放置完了 */
    int all_placed = 1;
    for(int i = 0; i < g->feiwuu_drag_count; i++)
    {
        if(g->feiwuu_drag_cards[i] != NULL)
        {
            all_placed = 0;
            break;
        }
    }

    if(all_placed)
    {
        game_log(g, "【飞舞】所有牌已放置完毕，继续结算");
        game_feiwuu_finish_resolve(g);
    }
}

/* 飞舞后续结算：亮出牌堆顶(6-X)张牌 */
void game_feiwuu_finish_resolve(GameState* g)
{
    if(!g) return;
    Player* p = &g->players[g->current_player];

    g->resp_state = RESPONSE_NONE;
    g->feiwuu_drag_count = 0;
    g->feiwuu_drag_index = -1;
    g->feiwuu_dragging = 0;
    for(int i = 0; i < 4; i++)
    {
        g->feiwuu_drag_cards[i] = NULL;
        g->feiwuu_placed_slots[i] = 0;
    }

    /* 继续飞舞结算：亮出牌堆顶(6-X)张牌 */
    p->yudie.feiwuu_count++;
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

    /* 屏幕中心展示亮出的牌（时间 = Ln(X)+0.5秒，X为牌数） */
    float feiwuu_dur = logf((float)reveal_idx) + 0.5f;
    if(feiwuu_dur < 1.0f) feiwuu_dur = 1.0f;
    int feiwuu_dur_frames = (int)(feiwuu_dur * 60);
    game_show_cards(g, revealed, reveal_idx, p->name, feiwuu_dur_frames);

    /* 获得其中与装备区花色相同的牌 */
    int got = 0;
    /* 先打印装备区花色，方便调试 */
    char equip_suits[64] = "";
    if(p->equip.weapon) snprintf(equip_suits+strlen(equip_suits), 16, "武%c ", card_get_suit_char(p->equip.weapon));
    if(p->equip.armor) snprintf(equip_suits+strlen(equip_suits), 16, "防%c ", card_get_suit_char(p->equip.armor));
    if(p->equip.horse_atk) snprintf(equip_suits+strlen(equip_suits), 16, "攻%c ", card_get_suit_char(p->equip.horse_atk));
    if(p->equip.horse_def) snprintf(equip_suits+strlen(equip_suits), 16, "防%c ", card_get_suit_char(p->equip.horse_def));
    game_log(g, "【飞舞】装备区花色：%s", equip_suits);

    for(int i = 0; i < reveal_idx; i++)
    {
        int match = yudie_card_matches_equip_suit(p, revealed[i]);
        game_log(g, "【飞舞】亮出第%d张：【%s】花色%c，匹配=%d",
                 i+1, card_get_full_name(revealed[i]), card_get_suit_char(revealed[i]), match);
        if(match)
        {
            player_draw_card(p, revealed[i]);
            got++;
            p->yudie.feiwuu_cards++;
        }
        else
        {
            /* 其余牌以任意顺序置于牌堆顶 */
            if(g->deck.top > 0)
            {
                g->deck.top--;
                g->deck.cards[g->deck.top] = revealed[i];
            }
            else
            {
                /* top 已经是0，放到最前面并后移 */
                for(int j = g->deck.count; j > 0; j--)
                    g->deck.cards[j] = g->deck.cards[j - 1];
                g->deck.cards[0] = revealed[i];
                g->deck.count++;
            }
        }
    }

    game_log(g, "【飞舞】亮出%d张牌，获得%d张同花色牌（累计%d张）",
             reveal_count, got, p->yudie.feiwuu_cards);

    /* 检查成蝶使命技成功条件 */
    yudie_check_chengdie(g, g->current_player);

    /* 技能结算完成 */
    hero_skill_finish(g, g->current_player, 1);
}

/* 取消拖拽放置：未放置的牌放回手牌，已放置的牌保留，继续后续结算 */
void game_feiwuu_drag_cancel(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_FEIWUU_DRAG) return;

    /* 将未放置的牌放回手牌 */
    Player* p = &g->players[g->current_player];
    for(int i = 0; i < g->feiwuu_drag_count; i++)
    {
        if(g->feiwuu_drag_cards[i])
        {
            player_draw_card(p, g->feiwuu_drag_cards[i]);
            game_log(g, "【飞舞】未放置的【%s】放回手牌", card_get_full_name(g->feiwuu_drag_cards[i]));
            g->feiwuu_drag_cards[i] = NULL;
        }
    }

    game_log(g, "【飞舞】取消拖拽放置，已放置的牌保留，继续结算");

    /* 继续后续结算（亮牌、摸牌） */
    game_feiwuu_finish_resolve(g);
}

/* 检测点击的装备槽（0=武器,1=防具,2=进攻马,3=防御马），返回-1表示没点中 */
int game_feiwuu_hit_equip_slot(GameState* g, int mx, int my)
{
    if(!g) return -1;

    /* 装备区位置：和 render.c 里一致 */
    Player* me = &g->players[0];
    int hand_start_x = 320;  /* 和 render.c 里一致 */
    int feixiao_y = WINDOW_HEIGHT - HERO_HEIGHT - 40;
    int hand_y = feixiao_y + HERO_HEIGHT - CARD_HEIGHT + 10;
    int equip_y = hand_y - CARD_HEIGHT - 25;

    for(int i = 0; i < 4; i++)
    {
        int slot_x = hand_start_x + i * (CARD_WIDTH + 10);
        if(mx >= slot_x && mx <= slot_x + CARD_WIDTH &&
           my >= equip_y && my <= equip_y + CARD_HEIGHT)
        {
            return i;
        }
    }
    return -1;
}

/* 取消飞舞选牌 */
void game_feiwuu_cancel(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_FEIWUU_PICK) return;
    game_log(g, "【飞舞】取消选择");
    g->resp_state = RESPONSE_NONE;
    g->feiwuu_selected_count = 0;
    for(int i = 0; i < 4; i++) g->feiwuu_selected[i] = -1;
}


/* ================================================================
 * 寒冰剑：杀造成伤害时，弃对方2牌（手牌+装备区合计），然后免伤
 * 流程：
 *   1. 杀造成伤害前 → game_start_hanbing（寒冰剑发亮，等待点击）
 *   2. 左键点击寒冰剑 → game_hanbing_click_weapon（进入选牌阶段）
 *   3. 左键点击对方手牌/装备 → game_hanbing_pick_card（选牌，选满2张自动确认）
 *   4. 右键点击寒冰剑 → game_hanbing_cancel（取消发动，正常造成伤害）
 * ================================================================ */

/* 清除寒冰剑状态 */
void game_hanbing_clear(GameState* g)
{
    if(!g) return;
    g->hanbing_active = 0;
    g->hanbing_source = -1;
    g->hanbing_target = -1;
    g->hanbing_damage = 0;
    g->hanbing_picking = 0;
    g->hanbing_picked_count = 0;
    g->hanbing_picked_type[0] = -1;
    g->hanbing_picked_type[1] = -1;
    g->hanbing_picked_index[0] = -1;
    g->hanbing_picked_index[1] = -1;
    g->resp_state = RESPONSE_NONE;
}

/* 开始寒冰剑发动阶段（杀造成伤害前调用） */
void game_start_hanbing(GameState* g, int source_idx, int target_idx, int damage)
{
    if(!g) return;
    if(source_idx < 0 || source_idx >= g->player_count) return;
    if(target_idx < 0 || target_idx >= g->player_count) return;

    Player* source = &g->players[source_idx];
    Player* target = &g->players[target_idx];

    /* 必须装备寒冰剑 */
    if(player_weapon_type(source) != WEAPON_HANBING)
        return;

    /* 对方必须有至少2张牌（手牌+装备区合计） */
    int target_card_count = target->hand_count;
    if(target->equip.weapon) target_card_count++;
    if(target->equip.armor) target_card_count++;
    if(target->equip.horse_atk) target_card_count++;
    if(target->equip.horse_def) target_card_count++;

    if(target_card_count < 2)
    {
        game_log(g, "%s的牌不足2张，无法发动寒冰剑", target->name);
        return;
    }

    g->hanbing_active = 1;
    g->hanbing_source = source_idx;
    g->hanbing_target = target_idx;
    g->hanbing_damage = damage;
    g->hanbing_picking = 0;
    g->hanbing_picked_count = 0;
    g->hanbing_picked_type[0] = -1;
    g->hanbing_picked_type[1] = -1;
    g->hanbing_picked_index[0] = -1;
    g->hanbing_picked_index[1] = -1;
    g->resp_state = RESPONSE_NEED_HANBING;

    game_log(g, "【寒冰剑】%s打出的杀造成伤害，是否发动寒冰剑？（左键点击寒冰剑弃对方2牌免伤，右键取消正常造成伤害）",
             source->name);
}

/* 左键点击寒冰剑：进入选牌阶段 */
void game_hanbing_click_weapon(GameState* g)
{
    if(!g || !g->hanbing_active) return;
    if(g->resp_state != RESPONSE_NEED_HANBING) return;

    if(g->hanbing_picking) return;  /* 已经在选牌阶段 */

    g->hanbing_picking = 1;
    game_log(g, "【寒冰剑】请依次点击对方的2张牌（手牌或装备区）弃置");
}

/* 选一张牌（手牌或装备） */
void game_hanbing_pick_card(GameState* g, int card_type, int card_index)
{
    if(!g || !g->hanbing_active) return;
    if(g->resp_state != RESPONSE_NEED_HANBING) return;
    if(!g->hanbing_picking) return;

    Player* target = &g->players[g->hanbing_target];

    /* 检查是否已经选过这张牌 */
    for(int i = 0; i < g->hanbing_picked_count; i++)
    {
        if(g->hanbing_picked_type[i] == card_type && g->hanbing_picked_index[i] == card_index)
        {
            game_log(g, "【寒冰剑】这张牌已经选过了");
            return;
        }
    }

    /* 验证牌是否存在 */
    int valid = 0;
    const char* card_name = "";
    if(card_type == 0)  /* 手牌 */
    {
        if(card_index >= 0 && card_index < target->hand_count)
        {
            valid = 1;
            card_name = card_get_full_name(target->hand[card_index]);
        }
    }
    else if(card_type == 1)  /* 武器 */
    {
        if(target->equip.weapon) { valid = 1; card_name = card_get_full_name(target->equip.weapon); }
    }
    else if(card_type == 2)  /* 防具 */
    {
        if(target->equip.armor) { valid = 1; card_name = card_get_full_name(target->equip.armor); }
    }
    else if(card_type == 3)  /* 进攻马 */
    {
        if(target->equip.horse_atk) { valid = 1; card_name = card_get_full_name(target->equip.horse_atk); }
    }
    else if(card_type == 4)  /* 防御马 */
    {
        if(target->equip.horse_def) { valid = 1; card_name = card_get_full_name(target->equip.horse_def); }
    }

    if(!valid)
    {
        game_log(g, "【寒冰剑】无效的牌选择");
        return;
    }

    /* 选中这张牌 */
    g->hanbing_picked_type[g->hanbing_picked_count] = card_type;
    g->hanbing_picked_index[g->hanbing_picked_count] = card_index;
    g->hanbing_picked_count++;
    game_log(g, "【寒冰剑】已选择第%d张牌【%s】", g->hanbing_picked_count, card_name);

    /* 选满2张后自动确认 */
    if(g->hanbing_picked_count >= 2)
    {
        game_hanbing_confirm(g);
    }
}

/* 确认发动：弃对方2牌，免伤（不扣血，不增加受伤次数） */
void game_hanbing_confirm(GameState* g)
{
    if(!g || !g->hanbing_active) return;
    if(g->hanbing_picked_count < 2) return;

    Player* target = &g->players[g->hanbing_target];

    game_log(g, "【寒冰剑】%s弃置对方2张牌，本次杀不造成伤害", g->players[g->hanbing_source].name);

    /* 弃置选中的牌（注意：先处理装备，再处理手牌，避免下标偏移） */
    for(int i = 0; i < 2; i++)
    {
        int ctype = g->hanbing_picked_type[i];
        int cidx = g->hanbing_picked_index[i];
        Card* removed = NULL;

        if(ctype == 0)  /* 手牌 */
        {
            /* 重新计算手牌下标（因为之前可能弃了装备，不影响手牌下标） */
            if(cidx >= 0 && cidx < target->hand_count)
                removed = player_remove_hand(target, cidx);
        }
        else if(ctype == 1)  /* 武器 */
        {
            removed = target->equip.weapon;
            target->equip.weapon = NULL;
        }
        else if(ctype == 2)  /* 防具 */
        {
            removed = target->equip.armor;
            target->equip.armor = NULL;
        }
        else if(ctype == 3)  /* 进攻马 */
        {
            removed = target->equip.horse_atk;
            target->equip.horse_atk = NULL;
        }
        else if(ctype == 4)  /* 防御马 */
        {
            removed = target->equip.horse_def;
            target->equip.horse_def = NULL;
        }

        if(removed)
        {
            discard_add(&g->discard, removed);
            game_log(g, "  弃置【%s】", card_get_full_name(removed));
        }
    }

    /* 免伤：不调用 game_deal_damage，不扣血，不增加受伤次数 */
    game_log(g, "【寒冰剑】%s未受到本次杀的伤害", target->name);

    game_hanbing_clear(g);
}

/* 右键点击寒冰剑：取消发动，正常造成伤害 */
void game_hanbing_cancel(GameState* g)
{
    if(!g || !g->hanbing_active) return;
    if(g->resp_state != RESPONSE_NEED_HANBING) return;

    int source = g->hanbing_source;
    int target = g->hanbing_target;
    int damage = g->hanbing_damage;

    game_log(g, "【寒冰剑】%s取消发动寒冰剑，正常造成伤害", g->players[source].name);

    game_hanbing_clear(g);

    /* 正常造成伤害 */
    g->current_damage_source = DMG_SRC_SHA;
    game_deal_damage(g, target, damage, source, DMG_NORMAL);
}


/* ================================================================
 * 过河拆桥/顺手牵羊：选择对方的一张牌
 * 牌类型：0=手牌,1=武器,2=防具,3=进攻马,4=防御马,5=延时锦囊
 * action：0=过河拆桥（弃置），1=顺手牵羊（获得）
 * ================================================================ */

/* 清除选择状态 */
void game_clear_pick_enemy_card(GameState* g)
{
    if(!g) return;
    g->pick_enemy_target = -1;
    g->pick_enemy_action = 0;
    g->pick_enemy_card_type = -1;
    g->pick_enemy_card_index = -1;
    g->resp_state = RESPONSE_NONE;
}

/* 开始选择对方的一张牌 */
void game_start_pick_enemy_card(GameState* g, int source_idx, int target_idx, int action)
{
    if(!g) return;
    if(source_idx < 0 || source_idx >= g->player_count) return;
    if(target_idx < 0 || target_idx >= g->player_count) return;

    Player* target = &g->players[target_idx];

    /* 检查对方是否有牌可拆/牵 */
    int total_cards = target->hand_count;
    if(target->equip.weapon) total_cards++;
    if(target->equip.armor) total_cards++;
    if(target->equip.horse_atk) total_cards++;
    if(target->equip.horse_def) total_cards++;
    total_cards += target->judge.count;

    if(total_cards <= 0)
    {
        game_log(g, "%s 没有牌可拆/牵", target->name);
        return;
    }

    g->pick_enemy_target = target_idx;
    g->pick_enemy_action = action;
    g->pick_enemy_card_type = -1;
    g->pick_enemy_card_index = -1;
    g->resp_state = RESPONSE_NEED_PICK_ENEMY_CARD;

    const char* action_name = (action == 0) ? "过河拆桥" : "顺手牵羊";
    game_log(g, "【%s】请点击%s的一张牌（手牌暗置，装备/延时锦囊明置）",
             action_name, target->name);
}

/* 选择一张牌 */
void game_pick_enemy_card(GameState* g, int card_type, int card_index)
{
    if(!g || g->resp_state != RESPONSE_NEED_PICK_ENEMY_CARD) return;

    Player* target = &g->players[g->pick_enemy_target];

    /* 验证牌是否存在 */
    int valid = 0;
    const char* card_name = "";
    if(card_type == 0)  /* 手牌 */
    {
        if(card_index >= 0 && card_index < target->hand_count)
        {
            valid = 1;
            card_name = "一张手牌";
        }
    }
    else if(card_type == 1)  /* 武器 */
    {
        if(target->equip.weapon) { valid = 1; card_name = card_get_full_name(target->equip.weapon); }
    }
    else if(card_type == 2)  /* 防具 */
    {
        if(target->equip.armor) { valid = 1; card_name = card_get_full_name(target->equip.armor); }
    }
    else if(card_type == 3)  /* 进攻马 */
    {
        if(target->equip.horse_atk) { valid = 1; card_name = card_get_full_name(target->equip.horse_atk); }
    }
    else if(card_type == 4)  /* 防御马 */
    {
        if(target->equip.horse_def) { valid = 1; card_name = card_get_full_name(target->equip.horse_def); }
    }
    else if(card_type == 5)  /* 延时锦囊 */
    {
        if(card_index >= 0 && card_index < target->judge.count)
        {
            valid = 1;
            card_name = card_get_full_name(target->judge.cards[card_index]);
        }
    }

    if(!valid)
    {
        game_log(g, "无效的牌选择");
        return;
    }

    /* 选中这张牌 */
    g->pick_enemy_card_type = card_type;
    g->pick_enemy_card_index = card_index;
    game_log(g, "已选择【%s】，再次点击确认，或点击其他牌切换", card_name);
}

/* 确认选择，执行拆/牵 */
void game_confirm_pick_enemy_card(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_PICK_ENEMY_CARD) return;
    if(g->pick_enemy_card_type < 0) return;

    Player* source = &g->players[g->current_player];
    Player* target = &g->players[g->pick_enemy_target];
    int ctype = g->pick_enemy_card_type;
    int cidx = g->pick_enemy_card_index;
    Card* removed = NULL;

    /* 移除选中的牌 */
    if(ctype == 0)  /* 手牌 */
    {
        if(cidx >= 0 && cidx < target->hand_count)
            removed = player_remove_hand(target, cidx);
    }
    else if(ctype == 1)  /* 武器 */
    {
        removed = target->equip.weapon;
        target->equip.weapon = NULL;
    }
    else if(ctype == 2)  /* 防具 */
    {
        removed = target->equip.armor;
        target->equip.armor = NULL;
    }
    else if(ctype == 3)  /* 进攻马 */
    {
        removed = target->equip.horse_atk;
        target->equip.horse_atk = NULL;
    }
    else if(ctype == 4)  /* 防御马 */
    {
        removed = target->equip.horse_def;
        target->equip.horse_def = NULL;
    }
    else if(ctype == 5)  /* 延时锦囊 */
    {
        if(cidx >= 0 && cidx < target->judge.count)
        {
            removed = target->judge.cards[cidx];
            /* 压缩判定区 */
            for(int i = cidx; i < target->judge.count - 1; i++)
                target->judge.cards[i] = target->judge.cards[i + 1];
            target->judge.cards[target->judge.count - 1] = NULL;
            target->judge.count--;
        }
    }

    if(!removed)
    {
        game_log(g, "选择的牌不存在");
        game_clear_pick_enemy_card(g);
        return;
    }

    const char* action_name = (g->pick_enemy_action == 0) ? "过河拆桥" : "顺手牵羊";
    if(g->pick_enemy_action == 0)
    {
        /* 过河拆桥：弃置 */
        discard_add(&g->discard, removed);
        game_log(g, "%s 对 %s 使用【%s】，弃置【%s】",
                 source->name, target->name, action_name, card_get_full_name(removed));
    }
    else
    {
        /* 顺手牵羊：获得 */
        player_draw_card(source, removed);
        game_log(g, "%s 对 %s 使用【%s】，获得【%s】",
                 source->name, target->name, action_name, card_get_full_name(removed));
    }

    /* 选牌完成后的回调处理 */
    int cb_type = g->pick_enemy_callback_type;
    g->pick_enemy_callback_type = 0;  /* 先清除，避免递归 */

    if(cb_type == 1)  /* 无罅飞光弃牌后结算杀 */
    {
        game_log(g, "【无罅飞光】弃牌完成，继续结算杀");
        /* 继续结算当前目标的杀 */
        int source = g->pick_enemy_sha_source;
        int target = g->pick_enemy_sha_targets[g->pick_enemy_sha_current];
        Card* sha_card = g->pick_enemy_sha_card;
        Player* tp = &g->players[target];
        if(tp->is_ai)
        {
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
                game_log(g, "  %s 打出【闪】", tp->name);
            }
            else
            {
                int dmg = game_calc_sha_damage(g, source, target);
                g->current_damage_source = DMG_SRC_SHA;
                game_deal_damage(g, target, dmg, source, DMG_NORMAL);
            }
            /* 处理下一个目标 */
            g->pick_enemy_sha_current++;
            if(g->pick_enemy_sha_current < g->pick_enemy_sha_target_count)
            {
                int next_tgt = g->pick_enemy_sha_targets[g->pick_enemy_sha_current];
                if(g->players[source].jingliu.allow_zone_card && g->players[next_tgt].alive)
                {
                    game_log(g, "【无罅飞光】继续弃置下一个目标的牌");
                    g->pick_enemy_callback_type = 1;
                    game_start_pick_enemy_card(g, source, next_tgt, 0);
                    game_clear_pick_enemy_card(g);
                    return;
                }
                else
                {
                    /* 不需要弃牌，直接结算杀 */
                    /* 递归调用回调处理 */
                    g->pick_enemy_callback_type = 1;
                    game_confirm_pick_enemy_card(g);
                    return;
                }
            }
            else
            {
                /* 所有目标结算完毕 */
                if(sha_card->card_nature == CARD_NATURE_VIRTUAL) {
                    free(sha_card);
                    game_log(g, "【虚拟牌】释放内存");
                } else {
                    discard_add(&g->discard, sha_card);
                }
                game_log(g, "多目标杀结算完毕");
            }
        }
        else
        {
            /* 玩家目标：进入响应状态 */
            g->resp_state = RESPONSE_NEED_BASIC;
            g->resp_trigger_card = sha_card;
            g->resp_source_player = source;
            g->resp_target_player = target;
            g->resp_required_basic = BASIC_SHAN;
            g->duel_turn = -1;
            game_log(g, "  请点击闪牌选中，点击确认打出，点击取消放弃");
        }
    }
    else if(cb_type == 2)  /* 古镜照神选项A继续下一个角色 */
    {
        game_log(g, "【古镜照神】获得一张牌，继续下一个角色");
        if(g->gujing_pick_remaining_count > 0)
        {
            int next_target = g->gujing_pick_remaining[--g->gujing_pick_remaining_count];
            if(g->players[next_target].alive)
            {
                /* 检查目标是否有牌 */
                Player* nt = &g->players[next_target];
                int has_card = nt->hand_count > 0 || nt->equip.weapon || nt->equip.armor
                              || nt->equip.horse_atk || nt->equip.horse_def;
                if(has_card)
                {
                    g->pick_enemy_callback_type = 2;
                    game_start_pick_enemy_card(g, g->gujing_pick_source, next_target, 1);
                    game_clear_pick_enemy_card(g);
                    return;
                }
            }
            /* 目标没有牌或已死亡，跳过 */
            g->pick_enemy_callback_type = 2;
            game_confirm_pick_enemy_card(g);
            return;
        }
        else
        {
            game_log(g, "【古镜照神】所有角色处理完毕");
        }
    }

    game_clear_pick_enemy_card(g);
}

/* 取消选择 */
void game_cancel_pick_enemy_card(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_PICK_ENEMY_CARD) return;
    game_log(g, "取消选择");
    game_clear_pick_enemy_card(g);
}


/* ================================================================
 * 朱雀羽扇：打出杀时，可选择将杀变成火杀
 * 流程：
 *   1. 打出杀时 → game_start_zhuque（朱雀羽扇发亮，等待点击）
 *   2. 左键点击朱雀羽扇 → game_zhuque_click_weapon（选中，按钮变确认）
 *   3. 点击"确认" → game_zhuque_confirm（杀变成火杀，继续结算）
 *   4. 点击"取消" → game_zhuque_cancel（普通杀继续结算）
 * ================================================================ */

/* 清除朱雀羽扇状态 */
void game_zhuque_clear(GameState* g)
{
    if(!g) return;
    g->zhuque_active = 0;
    g->zhuque_source = -1;
    g->zhuque_target = -1;
    g->zhuque_sha_card = NULL;
    g->zhuque_selected = 0;
    g->resp_state = RESPONSE_NONE;
}

/* 开始朱雀羽扇发动阶段（打出杀时调用） */
void game_start_zhuque(GameState* g, int source_idx, int target_idx, Card* sha_card)
{
    if(!g || !sha_card) return;
    if(source_idx < 0 || source_idx >= g->player_count) return;
    if(target_idx < 0 || target_idx >= g->player_count) return;

    Player* source = &g->players[source_idx];

    /* 必须装备朱雀羽扇 */
    if(player_weapon_type(source) != WEAPON_ZHUQUE)
        return;

    g->zhuque_active = 1;
    g->zhuque_source = source_idx;
    g->zhuque_target = target_idx;
    g->zhuque_sha_card = sha_card;
    g->zhuque_selected = 0;
    g->resp_state = RESPONSE_NEED_ZHUQUE;

    game_log(g, "【朱雀羽扇】%s打出【杀】，是否发动朱雀羽扇将杀变成火杀？（点击朱雀羽扇选中，点击确认发动，点击取消不发动）",
             source->name);
}

/* 左键点击朱雀羽扇：选中 */
void game_zhuque_click_weapon(GameState* g)
{
    if(!g || !g->zhuque_active) return;
    if(g->resp_state != RESPONSE_NEED_ZHUQUE) return;

    if(g->zhuque_selected) return;  /* 已经选中 */

    g->zhuque_selected = 1;
    game_log(g, "【朱雀羽扇】已选中，点击确认将杀变成火杀，或点击其他区域取消选中");
}

/* 确认：杀变成火杀，继续结算 */
void game_zhuque_confirm(GameState* g)
{
    if(!g || !g->zhuque_active) return;
    if(g->resp_state != RESPONSE_NEED_ZHUQUE) return;

    int source = g->zhuque_source;
    int target = g->zhuque_target;
    Card* sha_card = g->zhuque_sha_card;

    game_log(g, "【朱雀羽扇】%s发动朱雀羽扇，【杀】变成火杀", g->players[source].name);

    game_zhuque_clear(g);

    /* 继续结算杀：目标需要出闪 */
    Player* tp = &g->players[target];
    if(tp->is_ai)
    {
        /* 目标是AI：自动出闪 */
        int shan_index = -1;
        for (int i = 0; i < tp->hand_count; i++) {
            if (tp->hand[i]->type == CARD_BASIC &&
                tp->hand[i]->sub.basic.basic_type == BASIC_SHAN) {
                shan_index = i; break;
            }
        }
        if (shan_index >= 0) {
            Card* shan_card = player_remove_hand(tp, shan_index);
            discard_add(&g->discard, shan_card);
            g->central_show_card = shan_card;
            game_log(g, "%s 打出了【闪】", tp->name);
        } else {
            int dmg = game_calc_sha_damage(g, source, target);
            g->current_damage_source = DMG_SRC_SHA;
            /* 火杀：火焰伤害 */
            game_deal_damage(g, target, dmg, source, DMG_FIRE);
        }
    }
    else
    {
        /* 目标是玩家：中断，询问是否出闪 */
        g->resp_state = RESPONSE_NEED_BASIC;
        g->resp_trigger_card = sha_card;
        g->resp_source_player = source;
        g->resp_target_player = target;
        g->resp_required_basic = BASIC_SHAN;
        g->resp_need_basic_after_wuxie = 0;
        g->duel_turn = -1;
        game_log(g, "请点击闪牌选中，点击确认打出，点击取消放弃");
    }
}

/* 取消：普通杀继续结算 */
void game_zhuque_cancel(GameState* g)
{
    if(!g || !g->zhuque_active) return;
    if(g->resp_state != RESPONSE_NEED_ZHUQUE) return;

    int source = g->zhuque_source;
    int target = g->zhuque_target;
    Card* sha_card = g->zhuque_sha_card;

    game_log(g, "【朱雀羽扇】%s不发动朱雀羽扇，普通杀继续结算", g->players[source].name);

    game_zhuque_clear(g);

    /* 继续结算普通杀：目标需要出闪 */
    Player* tp = &g->players[target];
    if(tp->is_ai)
    {
        /* 目标是AI：自动出闪 */
        int shan_index = -1;
        for (int i = 0; i < tp->hand_count; i++) {
            if (tp->hand[i]->type == CARD_BASIC &&
                tp->hand[i]->sub.basic.basic_type == BASIC_SHAN) {
                shan_index = i; break;
            }
        }
        if (shan_index >= 0) {
            Card* shan_card = player_remove_hand(tp, shan_index);
            discard_add(&g->discard, shan_card);
            g->central_show_card = shan_card;
            game_log(g, "%s 打出了【闪】", tp->name);
        } else {
            int dmg = game_calc_sha_damage(g, source, target);
            g->current_damage_source = DMG_SRC_SHA;
            game_deal_damage(g, target, dmg, source, DMG_NORMAL);
        }
    }
    else
    {
        /* 八卦阵：目标需要出闪时，如果装备八卦阵，先进入八卦阵发动阶段 */
        game_start_bagua(g, target, source, sha_card);
        if(g->bagua_active) return;  /* 八卦阵发动中，等待玩家选择 */

        /* 目标是玩家：中断，询问是否出闪 */
        g->resp_state = RESPONSE_NEED_BASIC;
        g->resp_trigger_card = sha_card;
        g->resp_source_player = source;
        g->resp_target_player = target;
        g->resp_required_basic = BASIC_SHAN;
        g->resp_need_basic_after_wuxie = 0;
        g->duel_turn = -1;
        game_log(g, "请点击闪牌选中，点击确认打出，点击取消放弃");
    }
}


/* ================================================================
 * 丈八蛇矛：出牌阶段选两张手牌当杀打出
 * 流程：
 *   1. 出牌阶段装备丈八蛇矛时，丈八常亮
 *   2. 点击丈八 → 进入选牌模式，丈八变暗
 *   3. 点击手牌 → 选中/取消选中（最多2张，必须不同的牌）
 *   4. 选中0张或1张时，屏幕中间是"取消"
 *   5. 选中2张时，屏幕中间是"确定"
 *   6. 点击"取消" → 退出选牌模式，丈八变亮，选中的牌变暗
 *   7. 点击"确定" → 把两张牌当杀打出
 *      - 花色：两张牌同花色则该花色，否则无色
 *      - 颜色：两张牌同颜色则该颜色，否则无色
 *      - 点数：无
 * ================================================================ */

/* 清除丈八蛇矛状态 */
void game_zhangba_clear(GameState* g)
{
    if(!g) return;
    g->zhangba_active = 0;
    g->zhangba_selected_count = 0;
    g->zhangba_selected[0] = -1;
    g->zhangba_selected[1] = -1;
    if(g->zhangba_virtual_sha)
    {
        free(g->zhangba_virtual_sha);
        g->zhangba_virtual_sha = NULL;
    }
    g->resp_state = RESPONSE_NONE;
}

/* 点击丈八蛇矛，进入选牌模式 */
void game_start_zhangba(GameState* g)
{
    if(!g) return;
    if(g->phase != PHASE_PLAY || g->current_player != 0) return;
    if(g->resp_state != RESPONSE_NONE) return;

    Player* me = &g->players[0];
    if(player_weapon_type(me) != WEAPON_ZHANGBA)
        return;

    if(me->hand_count < 2)
    {
        game_log(g, "手牌不足2张，无法发动丈八蛇矛");
        return;
    }

    g->zhangba_active = 1;
    g->zhangba_selected_count = 0;
    g->zhangba_selected[0] = -1;
    g->zhangba_selected[1] = -1;
    g->resp_state = RESPONSE_NEED_ZHANGBA;

    game_log(g, "【丈八蛇矛】请选择两张手牌当杀打出（点击手牌选中/取消，点击确定打出，点击取消退出）");
}

/* 选择/取消选择一张手牌 */
void game_zhangba_pick_card(GameState* g, int hand_index)
{
    if(!g || !g->zhangba_active) return;
    if(g->resp_state != RESPONSE_NEED_ZHANGBA) return;

    Player* me = &g->players[0];
    if(hand_index < 0 || hand_index >= me->hand_count) return;

    /* 检查是否已经选中这张牌 */
    for(int i = 0; i < g->zhangba_selected_count; i++)
    {
        if(g->zhangba_selected[i] == hand_index)
        {
            /* 取消选中 */
            for(int j = i; j < g->zhangba_selected_count - 1; j++)
                g->zhangba_selected[j] = g->zhangba_selected[j + 1];
            g->zhangba_selected_count--;
            g->zhangba_selected[g->zhangba_selected_count] = -1;
            game_log(g, "【丈八蛇矛】取消选择第%d张手牌", hand_index + 1);
            return;
        }
    }

    /* 选中新的牌 */
    if(g->zhangba_selected_count >= 2)
    {
        game_log(g, "【丈八蛇矛】已选择2张牌，不能再选");
        return;
    }

    g->zhangba_selected[g->zhangba_selected_count++] = hand_index;
    game_log(g, "【丈八蛇矛】选择第%d张手牌", hand_index + 1);

    /* 选满两张后自动进入选目标状态 */
    if(g->zhangba_selected_count == 2)
    {
        game_log(g, "【丈八蛇矛】已选满2张牌，请选择目标");
        game_zhangba_confirm(g);
    }
}

/* 确认：把两张牌当杀打出 */
void game_zhangba_confirm(GameState* g)
{
    if(!g || !g->zhangba_active) return;
    if(g->resp_state != RESPONSE_NEED_ZHANGBA) return;
    if(g->zhangba_selected_count != 2) return;

    Player* me = &g->players[0];

    /* 获取两张选中的牌 */
    int idx1 = g->zhangba_selected[0];
    int idx2 = g->zhangba_selected[1];
    Card* card1 = me->hand[idx1];
    Card* card2 = me->hand[idx2];

    /* 构造虚拟杀牌 */
    Card* virtual_sha = (Card*)malloc(sizeof(Card));
    memset(virtual_sha, 0, sizeof(Card));
    virtual_sha->id = -1;
    virtual_sha->type = CARD_BASIC;
    virtual_sha->sub.basic.basic_type = BASIC_SHA;
    virtual_sha->sub.basic.sha_element = SHA_NORMAL;
    virtual_sha->is_valid = 1;
    virtual_sha->card_nature = CARD_NATURE_CONVERTED;  /* 丈八蛇矛的杀是转化牌 */
    virtual_sha->rank = 0;  /* 无点数 */

    /* 花色：两张牌同花色则该花色，否则无色 */
    if(card1->suit == card2->suit)
        virtual_sha->suit = card1->suit;
    else
        virtual_sha->suit = SUIT_NONE;

    /* 颜色：两张牌同颜色则该颜色，否则无色 */
    if(card1->color == card2->color)
        virtual_sha->color = card1->color;
    else
        virtual_sha->color = COLOR_NONE;

    /* 弃置两张选中的手牌（从大到小移除，避免下标变化） */
    if(idx1 > idx2)
    {
        player_remove_hand(me, idx1);
        player_remove_hand(me, idx2);
    }
    else
    {
        player_remove_hand(me, idx2);
        player_remove_hand(me, idx1);
    }
    discard_add(&g->discard, card1);
    discard_add(&g->discard, card2);

    g->zhangba_virtual_sha = virtual_sha;

    const char* suit_str = (virtual_sha->suit == SUIT_NONE) ? "无色" : "有花色";
    const char* color_str = (virtual_sha->color == COLOR_NONE) ? "无色" : "有颜色";
    game_log(g, "【丈八蛇矛】弃置两张手牌，当杀打出（花色：%s，颜色：%s）", suit_str, color_str);

    /* 清除丈八蛇矛选牌状态，但保留虚拟杀牌 */
    g->zhangba_active = 0;
    g->zhangba_selected_count = 0;
    g->zhangba_selected[0] = -1;
    g->zhangba_selected[1] = -1;

    /* 进入选目标状态 */
    g->resp_state = RESPONSE_NEED_TARGET;
    g->pending_card = virtual_sha;
    g->pending_hand_index = -1;  /* 标记为虚拟杀牌 */
}

/* 取消：退出选牌模式 */
void game_zhangba_cancel(GameState* g)
{
    if(!g || !g->zhangba_active) return;
    if(g->resp_state != RESPONSE_NEED_ZHANGBA) return;

    game_log(g, "【丈八蛇矛】取消发动");
    game_zhangba_clear(g);
}


/* ================================================================
 * 使用虚拟杀牌（丈八蛇矛构造的杀，不需要从手牌移除）
 * 逻辑与 game_use_card 里 BASIC_SHA 相同，只是不移除手牌
 * ================================================================ */
void game_use_virtual_sha(GameState* g, int source_idx, int target_idx, Card* sha_card)
{
    if(!g || !sha_card) return;
    if(source_idx < 0 || source_idx >= g->player_count) return;
    if(target_idx < 0 || target_idx >= g->player_count) return;
    if(target_idx == source_idx) return;
    if(!g->players[target_idx].alive) return;

    Player* p = &g->players[source_idx];
    Player* target = &g->players[target_idx];

    /* 攻击距离检查 */
    int dist = game_calc_distance(g, source_idx, target_idx);
    int range = player_attack_range(p);
    if(dist > range)
    {
        game_log(g, "距离不够，无法出杀！");
        return;
    }

    /* 出杀次数检查 */
    int max_sha = 1;
    if(player_weapon_type(p) == WEAPON_ZHUGELIANNU)
        max_sha = 999;
    else if(p->hero && p->hero->sha_bonus)
        max_sha += p->hero->sha_bonus(p);
    if(p->sha_used >= max_sha)
    {
        game_log(g, "本回合已不能再出杀！");
        return;
    }

    /* 仁王盾：黑色杀（黑桃/梅花）指定装备仁王盾的目标时，该杀无效 */
    if(player_armor_type(target) == ARMOR_RENWANG &&
       (sha_card->suit == SUIT_SPADE || sha_card->suit == SUIT_CLUB))
    {
        sha_card->is_valid = 0;
        game_log(g, "【仁王盾】%s的黑色杀被%s的仁王盾无效（丈八蛇矛）",
                 p->name, target->name);
        return;
    }

    p->sha_used++;
    g->central_show_card = sha_card;

    game_log(g, "%s 对 %s 使用了【杀】（丈八蛇矛）",
             p->name, target->name);

    /* 朱雀羽扇：打出杀时，如果攻击者装备朱雀羽扇，进入朱雀羽扇发动阶段 */
    game_start_zhuque(g, source_idx, target_idx, sha_card);
    if(g->zhuque_active) return;  /* 朱雀羽扇发动中，等待玩家选择 */

    /* 目标需要出闪 */
    if(target->is_ai)
    {
        /* 目标是AI：自动出闪 */
        int shan_index = -1;
        for (int i = 0; i < target->hand_count; i++) {
            if (target->hand[i]->type == CARD_BASIC &&
                target->hand[i]->sub.basic.basic_type == BASIC_SHAN) {
                shan_index = i; break;
            }
        }
        if (shan_index >= 0) {
            Card* shan_card = player_remove_hand(target, shan_index);
            discard_add(&g->discard, shan_card);
            g->central_show_card = shan_card;
            game_log(g, "%s 打出了【闪】", target->name);
            /* 贯石斧：杀被闪后，如果攻击者装备贯石斧，进入贯石斧发动阶段 */
            int dmg = game_calc_sha_damage(g, source_idx, target_idx);
            game_start_guanshi(g, source_idx, target_idx, dmg);
        } else {
            int dmg = game_calc_sha_damage(g, source_idx, target_idx);
            /* 寒冰剑：杀造成伤害前，如果攻击者装备寒冰剑，进入寒冰剑发动阶段 */
            game_start_hanbing(g, source_idx, target_idx, dmg);
            if(!g->hanbing_active) {
                /* 没有发动寒冰剑（未装备或牌不足），正常造成伤害 */
                g->current_damage_source = DMG_SRC_SHA;
                game_deal_damage(g, target_idx, dmg, source_idx, DMG_NORMAL);
            }
        }
    }
    else
    {
        /* 目标是玩家：中断，询问是否出闪 */
        g->resp_state = RESPONSE_NEED_BASIC;
        g->resp_trigger_card = sha_card;
        g->resp_source_player = source_idx;
        g->resp_target_player = target_idx;
        g->resp_required_basic = BASIC_SHAN;
        g->resp_need_basic_after_wuxie = 0;
        g->duel_turn = -1;
        game_log(g, "请点击闪牌选中，点击确认打出，点击取消放弃");
    }
}


/* ================================================================
 * 通用判定函数（单独写，用于八卦阵等需要判定的场景）
 * game_perform_judge：从牌堆顶翻一张牌，设置为 central_show_card，返回判定牌
 * game_finish_judge：判定完成后，将判定牌置入弃牌堆
 * ================================================================ */
Card* game_perform_judge(GameState* g, const char* judge_name)
{
    if(!g) return NULL;

    Card* judge_card = deck_draw(&g->deck);
    if(!judge_card)
    {
        game_log(g, "【%s】牌堆已空，无法判定", judge_name);
        return NULL;
    }

    g->central_show_card = judge_card;

    const char* color_str = "无色";
    if(judge_card->color == COLOR_RED) color_str = "红色";
    else if(judge_card->color == COLOR_BLACK) color_str = "黑色";

    game_log(g, "【%s】判定牌：【%s】（花色：%d，颜色：%s，点数：%d）",
             judge_name, card_get_full_name(judge_card),
             judge_card->suit, color_str, judge_card->rank);

    return judge_card;
}

void game_finish_judge(GameState* g, Card* judge_card)
{
    if(!g || !judge_card) return;
    discard_add(&g->discard, judge_card);
    if(g->central_show_card == judge_card)
        g->central_show_card = NULL;
}


/* ================================================================
 * 八卦阵：需要出闪时，可选择判定，红色视为打出虚拟闪
 * 流程：
 *   1. 需要出闪时 → game_start_bagua（八卦阵亮起，等待点击）
 *   2. 左键点击八卦阵 → game_bagua_click_armor（选中，按钮变确认）
 *   3. 点击"确认" → game_bagua_confirm（进行判定）
 *      - 判定牌红色 → 视为打出虚拟闪
 *      - 判定牌黑色 → 没有打出闪，受到杀的伤害
 *   4. 点击"取消" → game_bagua_cancel（跳过八卦阵，继续结算杀）
 * ================================================================ */

/* 清除八卦阵状态 */
void game_bagua_clear(GameState* g)
{
    if(!g) return;
    g->bagua_active = 0;
    g->bagua_source = -1;
    g->bagua_attacker = -1;
    g->bagua_trigger_card = NULL;
    g->bagua_selected = 0;
    g->bagua_judge_card = NULL;
    g->resp_state = RESPONSE_NONE;
}

/* 开始八卦阵发动阶段（需要出闪时调用） */
void game_start_bagua(GameState* g, int source_idx, int attacker_idx, Card* trigger_card)
{
    if(!g || !trigger_card) return;
    if(source_idx < 0 || source_idx >= g->player_count) return;
    if(attacker_idx < 0 || attacker_idx >= g->player_count) return;

    Player* source = &g->players[source_idx];

    /* 必须装备八卦阵 */
    if(!player_has_armor(source, ARMOR_BAGUA))
        return;

    g->bagua_active = 1;
    g->bagua_source = source_idx;
    g->bagua_attacker = attacker_idx;
    g->bagua_trigger_card = trigger_card;
    g->bagua_selected = 0;
    g->bagua_judge_card = NULL;
    g->resp_state = RESPONSE_NEED_BAGUA;

    game_log(g, "【八卦阵】%s需要出闪，是否发动八卦阵判定？（点击八卦阵选中，点击确认发动，点击取消不发动）",
             source->name);
}

/* 左键点击八卦阵：选中 */
void game_bagua_click_armor(GameState* g)
{
    if(!g || !g->bagua_active) return;
    if(g->resp_state != RESPONSE_NEED_BAGUA) return;

    if(g->bagua_selected) return;  /* 已经选中 */

    g->bagua_selected = 1;
    game_log(g, "【八卦阵】已选中，点击确认进行判定，或点击其他区域取消选中");
}

/* 确认：进行判定 */
void game_bagua_confirm(GameState* g)
{
    if(!g || !g->bagua_active) return;
    if(g->resp_state != RESPONSE_NEED_BAGUA) return;

    int source = g->bagua_source;
    int attacker = g->bagua_attacker;
    Card* trigger_card = g->bagua_trigger_card;

    game_log(g, "【八卦阵】%s发动八卦阵，进行判定", g->players[source].name);

    /* 进行判定 */
    Card* judge_card = game_perform_judge(g, "八卦阵");
    g->bagua_judge_card = judge_card;

    if(!judge_card)
    {
        /* 牌堆空了，无法判定，视为没有打出闪 */
        game_log(g, "【八卦阵】牌堆已空，判定失败，没有打出闪");
        game_finish_judge(g, judge_card);
        game_bagua_clear(g);
        /* 受到杀的伤害 */
        int dmg = game_calc_sha_damage(g, attacker, source);
        g->current_damage_source = DMG_SRC_SHA;
        game_deal_damage(g, source, dmg, attacker, DMG_NORMAL);
        return;
    }

    /* 判定结果：红色视为打出虚拟闪，黑色没有打出闪 */
    if(judge_card->color == COLOR_RED)
    {
        game_log(g, "【八卦阵】判定牌为红色，视为打出一张【闪】（虚拟牌）");

        /* 构造虚拟闪 */
        Card* virtual_shan = (Card*)malloc(sizeof(Card));
        memset(virtual_shan, 0, sizeof(Card));
        virtual_shan->id = -1;
        virtual_shan->type = CARD_BASIC;
        virtual_shan->sub.basic.basic_type = BASIC_SHAN;
        virtual_shan->is_valid = 1;
        virtual_shan->card_nature = CARD_NATURE_VIRTUAL;  /* 虚拟牌 */
        virtual_shan->suit = SUIT_NONE;    /* 无色 */
        virtual_shan->color = COLOR_NONE;   /* 无色 */
        virtual_shan->rank = 0;             /* 无点数 */

        g->central_show_card = virtual_shan;

        /* 判定牌置入弃牌堆 */
        game_finish_judge(g, judge_card);

        game_bagua_clear(g);

        /* 闪响应成功，不造成伤害 */
        game_log(g, "%s 打出了【闪】（八卦阵虚拟闪）", g->players[source].name);

        /* 释放虚拟闪（虚拟牌不进入弃牌堆） */
        free(virtual_shan);
    }
    else
    {
        game_log(g, "【八卦阵】判定牌为黑色，八卦阵未生效，可以继续用手牌闪响应");

        /* 判定牌置入弃牌堆 */
        game_finish_judge(g, judge_card);

        game_bagua_clear(g);

        /* 回到普通响应流程：让玩家用手牌里的闪响应 */
        g->resp_state = RESPONSE_NEED_BASIC;
        g->resp_trigger_card = trigger_card;
        g->resp_source_player = attacker;
        g->resp_target_player = source;
        g->resp_required_basic = BASIC_SHAN;
        g->resp_need_basic_after_wuxie = 0;
        g->duel_turn = -1;
        game_log(g, "请点击闪牌选中，点击确认打出，点击取消放弃");
    }
}

/* 取消：跳过八卦阵，继续结算杀 */
void game_bagua_cancel(GameState* g)
{
    if(!g || !g->bagua_active) return;
    if(g->resp_state != RESPONSE_NEED_BAGUA) return;

    int source = g->bagua_source;
    int attacker = g->bagua_attacker;
    Card* trigger_card = g->bagua_trigger_card;

    game_log(g, "【八卦阵】%s不发动八卦阵，继续结算杀", g->players[source].name);

    game_bagua_clear(g);

    /* 继续结算杀：目标需要出闪（回到普通响应流程） */
    g->resp_state = RESPONSE_NEED_BASIC;
    g->resp_trigger_card = trigger_card;
    g->resp_source_player = attacker;
    g->resp_target_player = source;
    g->resp_required_basic = BASIC_SHAN;
    g->resp_need_basic_after_wuxie = 0;
    g->duel_turn = -1;
    game_log(g, "请点击闪牌选中，点击确认打出，点击取消放弃");
}


/* ================================================================
 * 选目标：判断一张牌是否需要选择目标
 * 需要选目标：杀、过河拆桥、顺手牵羊、决斗、火攻、延时锦囊
 * ================================================================ */
int card_needs_target(Card* card)
{
    if(!card) return 0;
    /* 杀 */
    if(card->type == CARD_BASIC && card->sub.basic.basic_type == BASIC_SHA)
        return 1;
    /* 单体锦囊：过河拆桥、顺手牵羊、决斗、火攻 */
    if(card->type == CARD_TRICK)
    {
        TrickType tt = card->sub.trick.trick_type;
        if(tt == TRICK_GUOHE || tt == TRICK_SHUNSHOU ||
           tt == TRICK_JUEDOU || tt == TRICK_HUOGONG ||
           tt == TRICK_TIESUO)  /* 铁索连环也需要选目标（1-2人） */
            return 1;
    }
    /* 延时锦囊：都需要选目标（闪电只能指定自己，在选目标时限制） */
    if(card->type == CARD_DELAYED)
        return 1;
    return 0;
}


/* ================================================================
 * 选目标：开始选择目标，记录待使用的牌，进入 RESPONSE_NEED_TARGET
 * 调用时机：input.c 里玩家点击需要选目标的手牌时
 * ================================================================ */
void game_start_target_select(GameState* g, int hand_index)
{
    if(!g) return;
    if(g->resp_state != RESPONSE_NONE) return;
    if(g->phase != PHASE_PLAY) return;
    if(g->current_player != 0) return;

    Player* me = &g->players[0];
    if(hand_index < 0 || hand_index >= me->hand_count) return;

    Card* card = me->hand[hand_index];
    if(!card) return;
    if(!card_needs_target(card)) return;

    g->pending_hand_index = hand_index;
    g->pending_card = card;

    /* 铁索连环：进入专门的选目标状态（可选1-2人） */
    if(card->type == CARD_TRICK && card->sub.trick.trick_type == TRICK_TIESUO)
    {
        game_start_tiesuo_target(g, hand_index);
        return;
    }

    /* 镜流·无罅飞光花色1：杀可以多指定X个目标 */
    if(me->hero_id == HERO_JINGLIU && me->jingliu.sha_extra_target > 0 &&
       card->type == CARD_BASIC && card->sub.basic.basic_type == BASIC_SHA)
    {
        int max_targets = 1 + me->jingliu.sha_extra_target;
        game_log(g, "【无罅飞光】杀可指定%d个目标", max_targets);
        game_start_multi_target(g, 0, hand_index, 1, max_targets);
        return;
    }

    g->resp_state = RESPONSE_NEED_TARGET;
    game_log(g, "请选择【%s】的目标", card_get_full_name(card));
}


/* ================================================================
 * 选目标：判断某个角色是否可以作为这张牌的目标
 * 杀：存活、不是自己、距离<=攻击范围、出杀次数未用完
 * 过河拆桥：目标有牌（手牌/装备/判定区）
 * 顺手牵羊：距离<=1、目标有牌
 * 决斗：无额外限制
 * 火攻：目标有手牌
 * 延时锦囊：判定区未满
 * ================================================================ */
int game_can_target(GameState* g, Card* card, int from_idx, int target_idx)
{
    if(!g || !card) return 0;
    if(target_idx < 0 || target_idx >= g->player_count) return 0;
    if(!g->players[target_idx].alive) return 0;

    /* 闪电：只能指定自己 */
    if(card->type == CARD_DELAYED && card->sub.delayed.delayed_type == DELAYED_SHANDIAN)
    {
        if(target_idx != from_idx) return 0;  /* 只能指定自己 */
        if(g->players[target_idx].judge.count < MAX_JUDGE_CARDS) return 1;
        return 0;
    }

    /* 其他牌：不能指定自己 */
    if(target_idx == from_idx) return 0;

    Player* target = &g->players[target_idx];

    /* 龙胆：闪当杀打出时，按照杀来判断目标合法性 */
    int is_longdan_sha = 0;
    if(g->players[from_idx].hero_id == HERO_ZHAOYUN &&
       g->players[from_idx].longdan_active &&
       card->type == CARD_BASIC && card->sub.basic.basic_type == BASIC_SHAN)
    {
        is_longdan_sha = 1;
    }

    /* 杀：距离、攻击范围、出杀次数 */
    if((card->type == CARD_BASIC && card->sub.basic.basic_type == BASIC_SHA) || is_longdan_sha)
    {
        Player* from = &g->players[from_idx];
        int dist = game_calc_distance(g, from_idx, target_idx);
        int range = player_attack_range(from);
        if(dist > range) return 0;

        int max_sha = 1;
        Card* weapon = player_get_weapon(from);
        if(weapon &&
           weapon->type == CARD_EQUIP &&
           weapon->sub.equip.equip_type == EQUIP_WEAPON &&
           weapon->sub.equip.detail.weapon.weapon_type == WEAPON_ZHUGELIANNU)
            max_sha = 999;
        else if(from->hero && from->hero->sha_bonus)
            max_sha += from->hero->sha_bonus(from);
        /* 结束阶段打出的杀无次数限制（化形规则） */
        if(g->phase == PHASE_END)
            max_sha = 999;
        if(from->sha_used >= max_sha) return 0;

        return 1;
    }

    /* 过河拆桥：目标有牌（手牌/装备/判定区） */
    if(card->type == CARD_TRICK && card->sub.trick.trick_type == TRICK_GUOHE)
    {
        if(target->hand_count > 0) return 1;
        if(target->equip.weapon || target->equip.armor ||
           target->equip.horse_atk || target->equip.horse_def) return 1;
        if(target->judge.count > 0) return 1;
        return 0;
    }

    /* 顺手牵羊：距离<=1、目标有牌 */
    if(card->type == CARD_TRICK && card->sub.trick.trick_type == TRICK_SHUNSHOU)
    {
        int dist = game_calc_distance(g, from_idx, target_idx);
        if(dist > 1) return 0;
        if(target->hand_count > 0) return 1;
        if(target->equip.weapon || target->equip.armor ||
           target->equip.horse_atk || target->equip.horse_def) return 1;
        return 0;
    }

    /* 决斗：无额外限制 */
    if(card->type == CARD_TRICK && card->sub.trick.trick_type == TRICK_JUEDOU)
    {
        return 1;
    }

    /* 火攻：目标有手牌 */
    if(card->type == CARD_TRICK && card->sub.trick.trick_type == TRICK_HUOGONG)
    {
        if(target->hand_count > 0) return 1;
        return 0;
    }

    /* 延时锦囊：判定区未满且没有相同类型 */
    if(card->type == CARD_DELAYED)
    {
        if(target->judge.count >= MAX_JUDGE_CARDS) return 0;
        /* 检查是否已有相同类型的延时锦囊 */
        DelayedType dt = card->sub.delayed.delayed_type;
        for(int i = 0; i < target->judge.count; i++)
        {
            if(target->judge.cards[i] && target->judge.cards[i]->sub.delayed.delayed_type == dt)
                return 0;  /* 已有相同类型，不能再指定 */
        }
        return 1;
    }

    return 0;
}


/* ================================================================
 * 选目标：确认选择的目标，合法则使用牌，不合法则打日志
 * 调用时机：input.c 里玩家点击角色时（仅 RESPONSE_NEED_TARGET）
 * ================================================================ */
void game_select_target(GameState* g, int target_idx)
{
    if(!g) return;
    if(g->resp_state != RESPONSE_NEED_TARGET) return;
    if(!g->pending_card) return;

    if(!game_can_target(g, g->pending_card, 0, target_idx))
    {
        game_log(g, "无法选择【%s】作为【%s】的目标",
                 g->players[target_idx].name, card_get_full_name(g->pending_card));
        return;
    }

    int hand_index = g->pending_hand_index;
    game_log(g, "已选择【%s】为目标，点击确定打出【%s】",
             g->players[target_idx].name, card_get_full_name(g->pending_card));

    /* 进入确认出牌状态，点击确定后才打出 */
    game_start_confirm_play(g, hand_index, target_idx);
}


/* ================================================================
 * 选目标：取消选择目标，清除待使用的牌和状态
 * 调用时机：input.c 里玩家右键/ESC 时（仅 RESPONSE_NEED_TARGET）
 * ================================================================ */
void game_cancel_target_select(GameState* g)
{
    if(!g) return;
    if(g->resp_state != RESPONSE_NEED_TARGET) return;

    game_log(g, "取消选择目标");

    /* 丈八蛇矛的虚拟杀牌：释放并清理状态 */
    if(g->pending_hand_index == -1 && g->zhangba_virtual_sha)
    {
        game_log(g, "【丈八蛇矛】取消，释放虚拟杀牌");
        free(g->zhangba_virtual_sha);
        g->zhangba_virtual_sha = NULL;
        g->zhangba_active = 0;
        g->zhangba_selected_count = 0;
        g->zhangba_selected[0] = -1;
        g->zhangba_selected[1] = -1;
    }

    g->pending_hand_index = -1;
    g->pending_card = NULL;
    g->resp_state = RESPONSE_NONE;
}


/* ================================================================
 * 通用多目标选择（杀多目标/锦囊多目标等）
 * 流程：
 *   1. game_start_multi_target：进入多目标选择状态
 *   2. game_multi_target_toggle：点击头像切换选中/取消
 *   3. game_multi_target_confirm：点击确定（至少选min个）
 *   4. game_multi_target_cancel：点击取消
 * ================================================================ */
void game_start_multi_target(GameState* g, int source_idx, int hand_index, int min_targets, int max_targets)
{
    if(!g) return;
    if(source_idx < 0 || source_idx >= g->player_count) return;
    if(min_targets < 1) min_targets = 1;
    if(max_targets < min_targets) max_targets = min_targets;
    if(max_targets > g->player_count - 1) max_targets = g->player_count - 1;

    Player* src = &g->players[source_idx];
    Card* card = NULL;
    if(hand_index >= 0 && hand_index < src->hand_count)
        card = src->hand[hand_index];

    g->multi_target_source = source_idx;
    g->multi_target_hand_index = hand_index;
    g->multi_target_card = card;
    g->multi_target_count = 0;
    g->multi_target_min = min_targets;
    g->multi_target_max = max_targets;
    memset(g->multi_targets, -1, sizeof(g->multi_targets));
    g->resp_state = RESPONSE_NEED_MULTI_TARGET;

    const char* card_name = card ? card_get_full_name(card) : "虚拟牌";
    game_log(g, "请选择%d~%个目标（当前已选0个），点击确定确认，点击取消放弃",
             min_targets, max_targets);
}

void game_multi_target_toggle(GameState* g, int player_idx)
{
    if(!g || g->resp_state != RESPONSE_NEED_MULTI_TARGET) return;
    if(player_idx < 0 || player_idx >= g->player_count) return;
    if(!g->players[player_idx].alive) return;
    if(player_idx == g->multi_target_source) return;  /* 不能选自己 */

    /* 检查是否已选中 */
    for(int i = 0; i < g->multi_target_count; i++)
    {
        if(g->multi_targets[i] == player_idx)
        {
            /* 取消选中 */
            for(int j = i; j < g->multi_target_count - 1; j++)
                g->multi_targets[j] = g->multi_targets[j + 1];
            g->multi_target_count--;
            g->multi_targets[g->multi_target_count] = -1;
            game_log(g, "取消选择【%s】为目标（已选%d/%d）",
                     g->players[player_idx].name, g->multi_target_count, g->multi_target_max);
            return;
        }
    }

    /* 未选中：检查是否还能选 */
    if(g->multi_target_count >= g->multi_target_max)
    {
        game_log(g, "已选满%d个目标，不能再选", g->multi_target_max);
        return;
    }

    /* 检查目标合法性（杀：距离/攻击范围） */
    if(g->multi_target_card)
    {
        if(!game_can_target(g, g->multi_target_card, g->multi_target_source, player_idx))
        {
            game_log(g, "【%s】不能作为目标", g->players[player_idx].name);
            return;
        }
    }

    /* 选中 */
    g->multi_targets[g->multi_target_count++] = player_idx;
    game_log(g, "选择【%s】为目标（已选%d/%d）",
             g->players[player_idx].name, g->multi_target_count, g->multi_target_max);
}

int game_multi_target_is_selected(GameState* g, int player_idx)
{
    if(!g || g->resp_state != RESPONSE_NEED_MULTI_TARGET) return 0;
    for(int i = 0; i < g->multi_target_count; i++)
        if(g->multi_targets[i] == player_idx) return 1;
    return 0;
}

void game_multi_target_cancel(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_MULTI_TARGET) return;
    game_log(g, "取消多目标选择");
    g->multi_target_hand_index = -1;
    g->multi_target_card = NULL;
    g->multi_target_count = 0;
    g->resp_state = RESPONSE_NONE;
}

/* ================================================================
 * 登仙牌型转换选择
 * choice: 0=原牌, 1=当桃, 2=当桃园结义
 * ================================================================ */
void game_start_dengxian_convert(GameState* g, int hand_index)
{
    if(!g) return;
    if(hand_index < 0 || hand_index >= g->players[0].hand_count) return;
    g->dengxian_convert_hand_index = hand_index;
    g->resp_state = RESPONSE_NEED_DENGXIAN_CONVERT;
    Card* c = g->players[0].hand[hand_index];
    game_log(g, "【登仙】请选择【%s】的使用方式（当原牌/当桃/当桃园结义）",
             card_get_full_name(c));
}

void game_dengxian_convert_choose(GameState* g, int choice)
{
    if(!g || g->resp_state != RESPONSE_NEED_DENGXIAN_CONVERT) return;
    int hand_index = g->dengxian_convert_hand_index;
    Player* p = &g->players[0];
    if(hand_index < 0 || hand_index >= p->hand_count) return;

    Card* c = p->hand[hand_index];
    g->dengxian_convert_hand_index = -1;
    g->resp_state = RESPONSE_NONE;

    if(choice == 0)
    {
        game_log(g, "【登仙】按原牌【%s】使用", card_get_full_name(c));
        if(card_needs_target(c))
            game_start_target_select(g, hand_index);
        else
            game_start_confirm_play(g, hand_index, -1);
    }
    else if(choice == 1)
    {
        if(p->hp >= p->max_hp)
        {
            game_log(g, "【登仙】体力已满，不能当桃使用");
            return;
        }
        Card* used = player_remove_hand(p, hand_index);
        discard_add(&g->discard, used);
        player_recover(p, 1);
        game_log(g, "【登仙】将【%s】当桃使用，回复1点体力", card_get_full_name(used));
    }
    else if(choice == 2)
    {
        /* 当桃园结义使用：走正常群体锦囊流程（含无懈可击） */
        Card* used = player_remove_hand(p, hand_index);
        game_log(g, "【登仙】将【%s】当桃园结义使用", card_get_full_name(used));
        g->group_active = 1;
        g->group_phase = 0;
        g->group_current = -1;
        g->group_source = 0;
        g->group_trick_type = TRICK_TAOYUAN;
        g->group_trigger_card = used;
        g->group_wuxie_mask = 0;
        g->group_wuxie_counter_from = -1;
        g->group_wugu_count = 0;
        g->resp_state = RESPONSE_NONE;
    }
}

void game_dengxian_convert_cancel(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_DENGXIAN_CONVERT) return;
    game_log(g, "【登仙】取消牌型转换");
    g->dengxian_convert_hand_index = -1;
    g->resp_state = RESPONSE_NONE;
}

/* ================================================================
 * 玉盏目标调整：增加/减少/不调整
 * choice: 0=增加目标,1=减少目标,2=不调整
 * ================================================================ */
void game_start_yuzhan_target(GameState* g, int hand_index)
{
    if(!g) return;
    if(hand_index < 0 || hand_index >= g->players[0].hand_count) return;
    g->dengxian_convert_hand_index = hand_index;  /* 借用此字段保存手牌下标 */
    g->resp_state = RESPONSE_NEED_YUZHAN_TARGET;
    Card* c = g->players[0].hand[hand_index];
    game_log(g, "【玉盏】请选择【%s】的目标调整方式（增加/减少/不调整）",
             card_get_full_name(c));
}

void game_yuzhan_target_choose(GameState* g, int choice)
{
    if(!g || g->resp_state != RESPONSE_NEED_YUZHAN_TARGET) return;
    int hand_index = g->dengxian_convert_hand_index;
    Player* p = &g->players[0];
    if(hand_index < 0 || hand_index >= p->hand_count) return;

    Card* c = p->hand[hand_index];
    g->dengxian_convert_hand_index = -1;
    g->resp_state = RESPONSE_NONE;

    if(choice == 0)  /* 增加目标：走通用多目标选择，1~2个目标 */
    {
        game_log(g, "【玉盏】选择增加目标");
        /* 杀和单体锦囊都可以选2个目标 */
        game_start_multi_target(g, 0, hand_index, 1, 2);
    }
    else if(choice == 1)  /* 减少目标 */
    {
        /* 杀必须有目标，不能减少 */
        if(c->type == CARD_BASIC && c->sub.basic.basic_type == BASIC_SHA)
        {
            game_log(g, "【玉盏】杀必须指定目标，不能减少");
            game_start_target_select(g, hand_index);
            return;
        }
        /* 其他牌：减少目标 = 不需要目标，直接使用 */
        game_log(g, "【玉盏】选择减少目标（不需要指定目标）");
        game_start_confirm_play(g, hand_index, -1);
    }
    else  /* 不调整：走正常单目标选择 */
    {
        game_log(g, "【玉盏】选择不调整目标");
        if(card_needs_target(c))
            game_start_target_select(g, hand_index);
        else
            game_start_confirm_play(g, hand_index, -1);
    }
}

void game_yuzhan_target_cancel(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_YUZHAN_TARGET) return;
    game_log(g, "【玉盏】取消使用牌");
    g->dengxian_convert_hand_index = -1;
    g->resp_state = RESPONSE_NONE;
}

void game_multi_target_confirm(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_MULTI_TARGET) return;
    if(g->multi_target_count < g->multi_target_min)
    {
        game_log(g, "至少需要选择%d个目标", g->multi_target_min);
        return;
    }

    int source_idx = g->multi_target_source;
    int hand_index = g->multi_target_hand_index;
    Card* card = g->multi_target_card;
    int target_count = g->multi_target_count;
    int targets[MAX_PLAYERS];
    for(int i = 0; i < target_count; i++)
        targets[i] = g->multi_targets[i];

    /* 清除状态 */
    g->multi_target_hand_index = -1;
    g->multi_target_card = NULL;
    g->multi_target_count = 0;
    g->resp_state = RESPONSE_NONE;

    /* 如果是手牌，先移除 */
    Player* src = &g->players[source_idx];
    Card* used_card = NULL;
    if(hand_index >= 0 && hand_index < src->hand_count)
    {
        used_card = player_remove_hand(src, hand_index);
    }
    else if(card)
    {
        used_card = card;  /* 虚拟牌，不移除 */
    }

    if(!used_card) return;

    game_log(g, "%s 对 %d 个目标使用【%s】",
             src->name, target_count, card_get_full_name(used_card));

    /* 判断牌类型，走对应结算 */
    if(used_card->type == CARD_BASIC && used_card->sub.basic.basic_type == BASIC_SHA)
    {
        /* 多目标杀：对每个目标依次结算 */
        src->sha_used++;  /* 只消耗一次出杀次数 */
        g->central_show_card = used_card;

        /* 保存杀牌和目标列表，用于异步结算（弃牌/响应后继续） */
        g->pick_enemy_sha_card = used_card;
        g->pick_enemy_sha_source = source_idx;
        g->pick_enemy_sha_target_count = target_count;
        g->pick_enemy_sha_current = 0;
        for(int i = 0; i < target_count; i++)
            g->pick_enemy_sha_targets[i] = targets[i];

        /* 处理第一个目标 */
        int first_tgt = targets[0];
        if(g->players[first_tgt].alive)
        {
            game_log(g, "  → 对【%s】结算杀", g->players[first_tgt].name);

            /* 无罅飞光花色3：先弃置目标一张牌 */
            if(src->hero_id == HERO_JINGLIU && src->jingliu.allow_zone_card)
            {
                Player* ft = &g->players[first_tgt];
                int has_card = ft->hand_count > 0 || ft->equip.weapon || ft->equip.armor
                              || ft->equip.horse_atk || ft->equip.horse_def;
                if(has_card)
                {
                    game_log(g, "【无罅飞光】请弃置%s一张手牌/装备区牌", ft->name);
                    g->pick_enemy_callback_type = 1;
                    game_start_pick_enemy_card(g, source_idx, first_tgt, 0);
                    /* game_start_pick_enemy_card会设置resp_state，这里直接返回 */
                    return;
                }
            }

            /* 不需要弃牌，直接结算 */
            Player* target = &g->players[first_tgt];
            if(target->is_ai)
            {
                /* AI目标：自动出闪 */
                int shan_idx = -1;
                for(int h = 0; h < target->hand_count; h++)
                {
                    if(target->hand[h]->type == CARD_BASIC &&
                       target->hand[h]->sub.basic.basic_type == BASIC_SHAN)
                    {
                        shan_idx = h; break;
                    }
                }
                if(shan_idx >= 0)
                {
                    Card* sc = player_remove_hand(target, shan_idx);
                    discard_add(&g->discard, sc);
                    g->central_show_card = sc;
                    game_log(g, "  %s 打出【闪】", target->name);
                }
                else
                {
                    int dmg = game_calc_sha_damage(g, source_idx, first_tgt);
                    g->current_damage_source = DMG_SRC_SHA;
                    game_deal_damage(g, first_tgt, dmg, source_idx, DMG_NORMAL);
                }
                /* 继续处理下一个目标（通过回调机制） */
                g->pick_enemy_sha_current = 1;
                g->pick_enemy_callback_type = 1;
                /* 模拟选牌完成回调，继续下一个目标 */
                game_confirm_pick_enemy_card(g);
                return;
            }
            else
            {
                /* 玩家目标：进入响应状态 */
                g->resp_state = RESPONSE_NEED_BASIC;
                g->resp_trigger_card = used_card;
                g->resp_source_player = source_idx;
                g->resp_target_player = first_tgt;
                g->resp_required_basic = BASIC_SHAN;
                g->duel_turn = -1;
                g->pick_enemy_sha_current = 1;
                game_log(g, "  请点击闪牌选中，点击确认打出，点击取消放弃");
                return;
            }
        }
        else
        {
            /* 第一个目标已死亡，直接跳过 */
            g->pick_enemy_sha_current = 1;
            g->pick_enemy_callback_type = 1;
            game_confirm_pick_enemy_card(g);
            return;
        }
    }
    else
    {
        /* 其他牌类型：暂不支持多目标，走单目标结算（用第一个目标） */
        game_log(g, "该牌类型暂不支持多目标，按单目标结算");
        if(hand_index >= 0)
        {
            /* 把牌放回手牌，走正常流程 */
            player_draw_card(src, used_card);
            game_use_card(g, source_idx, hand_index, targets[0]);
        }
    }
}


/* ================================================================
 * 确认出牌：选完目标后，点击确定才打出
 * 流程：
 *   1. game_start_confirm_play：进入确认出牌状态
 *   2. game_confirm_play：点击确定，打出牌
 *   3. game_cancel_confirm_play：点击其他地方/右键，取消
 * ================================================================ */

/* 进入确认出牌状态 */
void game_start_confirm_play(GameState* g, int hand_index, int target_index)
{
    if(!g) return;
    g->confirm_play_hand_index = hand_index;
    g->confirm_play_target_index = target_index;
    g->resp_state = RESPONSE_NEED_CONFIRM_PLAY;
}

/* 确认出牌：打出牌 */
void game_confirm_play(GameState* g)
{
    if(!g) return;
    if(g->resp_state != RESPONSE_NEED_CONFIRM_PLAY) return;

    int hand_index = g->confirm_play_hand_index;
    int target_index = g->confirm_play_target_index;

    /* 清除状态 */
    g->confirm_play_hand_index = -1;
    g->confirm_play_target_index = -1;
    g->resp_state = RESPONSE_NONE;

    if(hand_index >= 0)
    {
        /* 普通牌：调用 game_use_card */
        game_use_card(g, 0, hand_index, target_index);
    }
    else
    {
        /* 丈八蛇矛的虚拟杀牌：直接结算杀 */
        Card* pending_card = g->pending_card;
        if(pending_card)
        {
            game_use_virtual_sha(g, 0, target_index, pending_card);
            /* 释放虚拟杀牌 */
            free(pending_card);
            if(g->zhangba_virtual_sha == pending_card)
                g->zhangba_virtual_sha = NULL;
        }
        g->pending_hand_index = -1;
        g->pending_card = NULL;
    }
}

/* 取消确认出牌 */
void game_cancel_confirm_play(GameState* g)
{
    if(!g) return;
    if(g->resp_state != RESPONSE_NEED_CONFIRM_PLAY) return;

    game_log(g, "取消出牌");
    g->confirm_play_hand_index = -1;
    g->confirm_play_target_index = -1;
    g->pending_hand_index = -1;
    g->pending_card = NULL;
    g->resp_state = RESPONSE_NONE;
}


/* ================================================================
 * 铁索连环选目标（选择1-2名目标，改变横置状态）
 * 流程：
 *   1. game_start_tiesuo_target：进入选目标状态
 *   2. game_tiesuo_toggle_target：点击头像切换选中状态（最多2人）
 *   3. game_tiesuo_confirm：确认选择，改变目标的横置状态
 *   4. game_tiesuo_cancel：取消选择
 * ================================================================ */

/* 进入铁索连环选目标状态 */
void game_start_tiesuo_target(GameState* g, int hand_index)
{
    if(!g) return;
    Player* me = &g->players[0];
    if(hand_index < 0 || hand_index >= me->hand_count) return;

    g->tiesuo_hand_index = hand_index;
    g->tiesuo_targets[0] = -1;
    g->tiesuo_targets[1] = -1;
    g->tiesuo_target_count = 0;
    g->resp_state = RESPONSE_NEED_TIESUO_TARGET;

    game_log(g, "【铁索连环】请选择1-2名角色（点击头像选中/取消，最多2人），选好后点击确定");
}

/* 切换目标选中状态 */
void game_tiesuo_toggle_target(GameState* g, int player_idx)
{
    if(!g || g->resp_state != RESPONSE_NEED_TIESUO_TARGET) return;
    if(player_idx < 0 || player_idx >= g->player_count) return;
    if(!g->players[player_idx].alive) return;

    /* 检查是否已经选中 */
    for(int i = 0; i < 2; i++)
    {
        if(g->tiesuo_targets[i] == player_idx)
        {
            /* 已选中：取消选中 */
            g->tiesuo_targets[i] = -1;
            g->tiesuo_target_count--;
            /* 整理数组，把-1移到后面 */
            if(g->tiesuo_targets[0] == -1 && g->tiesuo_targets[1] != -1)
            {
                g->tiesuo_targets[0] = g->tiesuo_targets[1];
                g->tiesuo_targets[1] = -1;
            }
            game_log(g, "【铁索连环】取消选择【%s】（已选%d人）",
                     g->players[player_idx].name, g->tiesuo_target_count);
            return;
        }
    }

    /* 未选中：检查是否已满2人 */
    if(g->tiesuo_target_count >= 2)
    {
        game_log(g, "【铁索连环】最多只能选择2名角色");
        return;
    }

    /* 选中 */
    g->tiesuo_targets[g->tiesuo_target_count] = player_idx;
    g->tiesuo_target_count++;
    game_log(g, "【铁索连环】选中【%s】（已选%d人）",
             g->players[player_idx].name, g->tiesuo_target_count);
}

/* 确认选择，进入无懈可击询问状态 */
void game_tiesuo_confirm(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_TIESUO_TARGET) return;
    if(g->tiesuo_target_count <= 0)
    {
        game_log(g, "【铁索连环】请至少选择1名角色");
        return;
    }

    Player* me = &g->players[0];
    Card* card = me->hand[g->tiesuo_hand_index];
    if(!card) return;

    /* 移除铁索连环手牌 */
    Card* c = player_remove_hand(me, g->tiesuo_hand_index);
    if(!game_discard_check_valid(g, c))
    {
        g->resp_state = RESPONSE_NONE;
        return;
    }
    discard_add(&g->discard, c);

    game_log(g, "%s 使用【铁索连环】", me->name);

    /* 进入无懈可击询问状态 */
    g->tiesuo_wuxie_index = 0;
    g->tiesuo_wuxie_mask = 0;
    g->resp_state = RESPONSE_NEED_TIESUO_WUXIE;

    /* 开始询问第一个目标 */
    game_tiesuo_wuxie_advance(g);
}

/* 推进铁索连环无懈可击询问：询问下一个目标 */
void game_tiesuo_wuxie_advance(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_TIESUO_WUXIE) return;

    /* 找到下一个未询问的目标 */
    while(g->tiesuo_wuxie_index < g->tiesuo_target_count)
    {
        int target_idx = g->tiesuo_targets[g->tiesuo_wuxie_index];
        if(target_idx < 0 || !g->players[target_idx].alive)
        {
            g->tiesuo_wuxie_index++;
            continue;
        }

        Player* target = &g->players[target_idx];
        game_log(g, "【铁索连环】询问%s是否使用无懈可击", target->name);

        if(target->is_ai)
        {
            /* AI自动决定是否使用无懈可击 */
            int wuxie_idx = -1;
            for(int i = 0; i < target->hand_count; i++)
            {
                if(target->hand[i] && target->hand[i]->type == CARD_TRICK &&
                   target->hand[i]->sub.trick.trick_type == TRICK_WUXIE)
                {
                    wuxie_idx = i;
                    break;
                }
            }

            if(wuxie_idx != -1 && rand() % 10 < 5)
            {
                /* AI使用无懈可击 */
                Card* w = player_remove_hand(target, wuxie_idx);
                discard_add(&g->discard, w);
                g->central_show_card = w;
                g->tiesuo_wuxie_mask |= (1 << g->tiesuo_wuxie_index);
                game_log(g, "%s 打出【无懈可击】，抵消铁索连环对自己的效果", target->name);
            }
            else
            {
                game_log(g, "%s 不使用无懈可击", target->name);
            }

            g->tiesuo_wuxie_index++;
            /* 继续询问下一个 */
            continue;
        }
        else
        {
            /* 玩家目标：询问是否使用无懈可击 */
            g->resp_state = RESPONSE_NEED_WUXIE;
            g->resp_target_player = target_idx;
            g->resp_source_player = 0;  /* 铁索连环使用者是玩家0 */
            g->resp_trigger_card = NULL;
            g->group_active = 0;  /* 标记为单体锦囊的无懈可击 */
            game_log(g, "请点击【无懈可击】选中抵消铁索连环，点击确认打出，点击取消放弃");
            return;
        }
    }

    /* 所有目标询问完毕，最终结算 */
    game_tiesuo_final_resolve(g);
}

/* 玩家对铁索连环使用无懈可击的结果处理 */
void game_tiesuo_wuxie_result(GameState* g, int used_wuxie)
{
    if(!g) return;

    if(used_wuxie)
    {
        /* 玩家使用了无懈可击，标记当前目标被无懈 */
        g->tiesuo_wuxie_mask |= (1 << g->tiesuo_wuxie_index);
        game_log(g, "你打出【无懈可击】，抵消铁索连环对自己的效果");
    }
    else
    {
        game_log(g, "你不使用无懈可击");
    }

    g->tiesuo_wuxie_index++;
    g->resp_state = RESPONSE_NEED_TIESUO_WUXIE;

    /* 继续询问下一个目标 */
    game_tiesuo_wuxie_advance(g);
}

/* 铁索连环最终结算：改变未被无懈的目标的横置状态 */
void game_tiesuo_final_resolve(GameState* g)
{
    if(!g) return;

    for(int i = 0; i < g->tiesuo_target_count; i++)
    {
        int idx = g->tiesuo_targets[i];
        if(idx < 0 || !g->players[idx].alive) continue;

        /* 被无懈的目标不改变横置状态 */
        if(g->tiesuo_wuxie_mask & (1 << i))
        {
            game_log(g, "【铁索连环】%s 被无懈可击抵消，不改变横置状态", g->players[idx].name);
            continue;
        }

        Player* target = &g->players[idx];
        target->chained = !target->chained;
        if(target->chained)
            game_log(g, "【铁索连环】%s 被横置（进入连环状态）", target->name);
        else
            game_log(g, "【铁索连环】%s 被重置（解除连环状态）", target->name);
    }

    /* 清除状态 */
    g->tiesuo_hand_index = -1;
    g->tiesuo_targets[0] = -1;
    g->tiesuo_targets[1] = -1;
    g->tiesuo_target_count = 0;
    g->tiesuo_wuxie_index = 0;
    g->tiesuo_wuxie_mask = 0;
    g->resp_state = RESPONSE_NONE;
}

/* 取消选择 */
void game_tiesuo_cancel(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_TIESUO_TARGET) return;

    game_log(g, "【铁索连环】取消选择");
    g->tiesuo_hand_index = -1;
    g->tiesuo_targets[0] = -1;
    g->tiesuo_targets[1] = -1;
    g->tiesuo_target_count = 0;
    g->resp_state = RESPONSE_NONE;
}

/* ================================================================
 * 铁索连环重铸：弃置铁索连环，摸一张牌
 * ================================================================ */
void game_tiesuo_chongzhu(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_TIESUO_TARGET) return;

    Player* me = &g->players[0];
    int hand_index = g->tiesuo_hand_index;
    if(hand_index < 0 || hand_index >= me->hand_count) return;

    Card* card = me->hand[hand_index];
    if(!card || card->type != CARD_TRICK ||
       card->sub.trick.trick_type != TRICK_TIESUO)
        return;

    /* 弃置铁索连环 */
    Card* removed = player_remove_hand(me, hand_index);
    discard_add(&g->discard, removed);
    game_log(g, "【铁索连环】重铸：弃置【%s】，摸一张牌", card_get_full_name(removed));

    /* 摸一张牌 */
    if(g->deck.count > 0)
    {
        Card* draw = deck_draw(&g->deck);
        if(draw) player_draw_card(me, draw);
    }

    /* 清除状态 */
    g->tiesuo_hand_index = -1;
    g->tiesuo_targets[0] = -1;
    g->tiesuo_targets[1] = -1;
    g->tiesuo_target_count = 0;
    g->resp_state = RESPONSE_NONE;
}


/* ================================================================
 * 吉尔伽美什技能交互式选择
 * ================================================================ */

/* 进入所见：选择牌类型（2*2：基本牌/锦囊/装备/延时锦囊） */
void game_gilgamesh_start_suojian(GameState* g)
{
    if(!g) return;
    g->gilgamesh_skill_idx = 0;
    g->resp_state = RESPONSE_NEED_GILGAMESH_SUOJIAN;
    game_log(g, "【所见】请选择要检索的牌类型");
}

/* 选择牌类型 */
void game_gilgamesh_select_type(GameState* g, int type_idx)
{
    if(!g || g->resp_state != RESPONSE_NEED_GILGAMESH_SUOJIAN) return;
    Player* p = &g->players[0];

    /* type_idx: 0=基本牌, 1=锦囊牌, 2=装备牌, 3=延时锦囊 */
    CardType types[4] = {CARD_BASIC, CARD_TRICK, CARD_EQUIP, CARD_DELAYED};
    const char* type_names[4] = {"基本牌", "锦囊牌", "装备牌", "延时锦囊"};

    if(type_idx < 0 || type_idx >= 4) return;

    game_log(g, "【所见】选择检索【%s】（随机获取一张）", type_names[type_idx]);
    gilgamesh_suojian(g, 0, types[type_idx], 0);
    g->resp_state = RESPONSE_NONE;
}

/* 进入乖离：选择花色（2*2：黑桃/红桃/梅花/方块） */
void game_gilgamesh_start_guaili(GameState* g)
{
    if(!g) return;
    g->gilgamesh_skill_idx = 1;
    g->resp_state = RESPONSE_NEED_GILGAMESH_GUAILI;
    game_log(g, "【乖离】请选择要弃置的花色");
}

/* 选择花色 */
void game_gilgamesh_select_suit(GameState* g, int suit_idx)
{
    if(!g || g->resp_state != RESPONSE_NEED_GILGAMESH_GUAILI) return;

    const char* suit_names[4] = {"黑桃", "红桃", "梅花", "方块"};
    if(suit_idx < 0 || suit_idx >= 4) return;

    game_log(g, "【乖离】选择弃置【%s】花色", suit_names[suit_idx]);
    gilgamesh_guaili(g, 0, (Suit)suit_idx);
    g->resp_state = RESPONSE_NONE;
}

/* 进入天辟：选择目标 */
void game_gilgamesh_start_tianpi_target(GameState* g)
{
    if(!g) return;
    g->gilgamesh_skill_idx = 2;
    g->gilgamesh_target_idx = -1;
    g->resp_state = RESPONSE_NEED_GILGAMESH_TIANPI_TARGET;
    game_log(g, "【天辟】请选择目标角色（点击头像）");
}

/* 选择天辟目标 */
void game_gilgamesh_select_tianpi_target(GameState* g, int player_idx)
{
    if(!g || g->resp_state != RESPONSE_NEED_GILGAMESH_TIANPI_TARGET) return;
    if(player_idx < 0 || player_idx >= g->player_count) return;
    if(!g->players[player_idx].alive) return;

    g->gilgamesh_target_idx = player_idx;
    game_log(g, "【天辟】选择目标【%s】，请点击一张手牌选择点数", g->players[player_idx].name);
    g->resp_state = RESPONSE_NEED_GILGAMESH_TIANPI_RANK;
}

/* 进入天辟：选择点数（点击手牌） */
void game_gilgamesh_start_tianpi_rank(GameState* g)
{
    /* 这个状态是从 select_tianpi_target 直接进入的，不需要单独函数 */
}

/* 点击手牌选择点数 */
void game_gilgamesh_select_rank(GameState* g, int hand_index)
{
    if(!g || g->resp_state != RESPONSE_NEED_GILGAMESH_TIANPI_RANK) return;
    Player* p = &g->players[0];
    if(hand_index < 0 || hand_index >= p->hand_count) return;

    Card* c = p->hand[hand_index];
    if(!c) return;

    const char* rank_names[] = {"A","2","3","4","5","6","7","8","9","10","J","Q","K","小王","大王","JOKER"};
    game_log(g, "【天辟】选择点数【%s】，对【%s】造成伤害",
             rank_names[c->rank], g->players[g->gilgamesh_target_idx].name);

    gilgamesh_tianpi(g, 0, g->gilgamesh_target_idx, c->rank);
    g->resp_state = RESPONSE_NONE;
}

/* 取消技能选择 */
void game_gilgamesh_cancel(GameState* g)
{
    if(!g) return;
    if(g->resp_state != RESPONSE_NEED_GILGAMESH_SUOJIAN &&
       g->resp_state != RESPONSE_NEED_GILGAMESH_GUAILI &&
       g->resp_state != RESPONSE_NEED_GILGAMESH_TIANPI_TARGET &&
       g->resp_state != RESPONSE_NEED_GILGAMESH_TIANPI_RANK)
        return;

    game_log(g, "【吉尔伽美什】取消技能发动");
    g->gilgamesh_skill_idx = -1;
    g->gilgamesh_target_idx = -1;
    g->resp_state = RESPONSE_NONE;
}


/* ================================================================
 * 通用：从牌堆中随机获取一张指定大类的牌
 *   type: CARD_BASIC / CARD_TRICK / CARD_EQUIP / CARD_DELAYED
 *   返回获取到的牌指针，加入手牌；NULL=没找到
 * ================================================================ */
Card* game_draw_random_card_by_type(GameState* g, int player_idx, CardType type)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return NULL;
    Player* p = &g->players[player_idx];

    /* 收集牌堆中所有匹配大类的牌 */
    int match_indices[256];
    int match_count = 0;
    for (int i = g->deck.top; i < g->deck.count; i++) {
        Card* c = g->deck.cards[i];
        if (!c) continue;
        if (c->type == type) {
            match_indices[match_count++] = i;
        }
    }

    if (match_count <= 0) {
        game_log(g, "牌堆中没有【%d】类型的牌", type);
        return NULL;
    }

    /* 随机选一张匹配的牌 */
    int rand_idx = match_indices[rand() % match_count];
    Card* c = g->deck.cards[rand_idx];

    /* 从牌堆中取出：前移覆盖 */
    for (int j = rand_idx; j > g->deck.top; j--) {
        g->deck.cards[j] = g->deck.cards[j - 1];
    }
    g->deck.cards[g->deck.top] = c;
    g->deck.top++;

    player_draw_card(p, c);
    game_log(g, "从牌堆随机获取一张【%s】", card_get_full_name(c));
    return c;
}


/* ================================================================
 * 通用倒计时系统
 * ================================================================ */
void game_countdown_start(GameState* g, float seconds, int callback_type)
{
    if(!g) return;
    g->countdown.active = 1;
    g->countdown.remaining = seconds;
    g->countdown.callback_type = callback_type;
}

void game_countdown_stop(GameState* g)
{
    if(!g) return;
    g->countdown.active = 0;
    g->countdown.remaining = 0;
    g->countdown.callback_type = 0;
}

void game_countdown_update(GameState* g, float delta_seconds)
{
    if(!g || !g->countdown.active) return;
    g->countdown.remaining -= delta_seconds;
    if(g->countdown.remaining <= 0)
    {
        g->countdown.remaining = 0;
        g->countdown.active = 0;
        /* 倒计时结束，根据回调类型执行相应操作 */
        if(g->countdown.callback_type == 0)
        {
            /* 无懈可击超时：自动不响应 */
            if(g->resp_state == RESPONSE_NEED_MULTI_WUXIE)
            {
                game_multi_wuxie_pass(g);
            }
        }
        else if(g->countdown.callback_type == 1)
        {
            /* 五谷丰登选牌超时：自动选第一张牌 */
            if(g->resp_state == RESPONSE_NEED_WUGU_PICK && g->group_wugu_count > 0)
            {
                int cur = g->resp_target_player;
                Card* picked = g->group_wugu_pile[0];
                for(int s = 0; s < g->group_wugu_count - 1; s++)
                    g->group_wugu_pile[s] = g->group_wugu_pile[s + 1];
                g->group_wugu_count--;
                player_draw_card(&g->players[cur], picked);
                game_log(g, "%s 五谷丰登选牌超时，自动获得【%s】",
                         g->players[cur].name, card_get_full_name(picked));
                g->resp_state = RESPONSE_NONE;
                game_group_advance(g);
            }
        }
    }
}

/* ================================================================
 * 多目标锦囊无懈可击结算（通用，支持无限层反无懈）
 * ================================================================ */
void game_start_multi_wuxie(GameState* g, Card* card, int source,
                            int* targets, int target_count, int trick_type)
{
    if(!g || !card || target_count <= 0) return;

    g->multi_wuxie_card = card;
    g->multi_wuxie_source = source;
    g->multi_wuxie_target_count = target_count;
    for(int i = 0; i < target_count && i < MAX_PLAYERS; i++)
        g->multi_wuxie_targets[i] = targets[i];
    g->multi_wuxie_current_target = 0;
    g->multi_wuxie_wuxie_mask = 0;
    g->multi_wuxie_stack_depth = 0;
    g->multi_wuxie_trick_type = trick_type;
    g->multi_wuxie_resolved = 0;

    game_log(g, "【无懈可击】开始结算，共%d个目标", target_count);
    game_multi_wuxie_advance(g);
}

void game_multi_wuxie_advance(GameState* g)
{
    if(!g) return;

    /* 如果所有目标都结算完毕，进行最终结算 */
    if(g->multi_wuxie_current_target >= g->multi_wuxie_target_count)
    {
        game_multi_wuxie_final_resolve(g);
        return;
    }

    int target = g->multi_wuxie_targets[g->multi_wuxie_current_target];

    /* 初始化当前目标的无懈可击栈 */
    g->multi_wuxie_stack_depth = 0;
    g->multi_wuxie_stack[0].user_idx = -1;
    g->multi_wuxie_stack[0].target_idx = target;
    g->multi_wuxie_stack[0].trick_target_idx = target;
    g->multi_wuxie_stack[0].asker_idx = -1;
    g->multi_wuxie_stack[0].ask_count = 0;

    /* 开始询问：从目标的下家开始逆时针询问 */
    g->resp_state = RESPONSE_NEED_MULTI_WUXIE;
    g->resp_target_player = target;
    g->resp_source_player = g->multi_wuxie_source;
    g->resp_trigger_card = g->multi_wuxie_card;

    /* 找到第一个需要询问的玩家（从使用者开始逆时针询问） */
    int asker = g->multi_wuxie_source;
    int asked = 0;
    while(asked < g->player_count)
    {
        if(g->players[asker].alive)
        {
            g->multi_wuxie_stack[0].asker_idx = asker;
            break;
        }
        asker = (asker + 1) % g->player_count;
        asked++;
    }

    if(g->multi_wuxie_stack[0].asker_idx == -1)
    {
        /* 没有人可以询问，直接结算下一个目标 */
        g->multi_wuxie_current_target++;
        game_multi_wuxie_advance(g);
        return;
    }

    /* 如果询问者是AI，1.5秒后自动决定 */
    if(g->players[g->multi_wuxie_stack[0].asker_idx].is_ai)
    {
        game_countdown_start(g, 1.5f, 0);
        game_log(g, "%s 思考中...（1.5秒）",
                 g->players[g->multi_wuxie_stack[0].asker_idx].name);
    }
    else
    {
        /* 玩家：开始1.5秒倒计时 */
        game_countdown_start(g, 1.5f, 0);
        game_log(g, "请点击【无懈可击】选中打出，点击取消放弃（1.5秒倒计时）");
    }
}

void game_multi_wuxie_use(GameState* g, int user_idx)
{
    if(!g || g->resp_state != RESPONSE_NEED_MULTI_WUXIE) return;

    Player* user = &g->players[user_idx];
    int wuxie_idx = -1;
    /* 玩家（0号）且已选中响应牌：使用选中的牌 */
    if(user_idx == 0 && g->response_pick_selected && g->response_pick_index >= 0 &&
       g->response_pick_index < user->hand_count)
    {
        Card* sel = user->hand[g->response_pick_index];
        if(sel && sel->type == CARD_TRICK && sel->sub.trick.trick_type == TRICK_WUXIE)
            wuxie_idx = g->response_pick_index;
    }
    /* AI或未选中：找第一张无懈可击 */
    if(wuxie_idx == -1)
    {
        for(int i = 0; i < user->hand_count; i++)
        {
            if(user->hand[i] && user->hand[i]->type == CARD_TRICK &&
               user->hand[i]->sub.trick.trick_type == TRICK_WUXIE)
            {
                wuxie_idx = i;
                break;
            }
        }
    }

    if(wuxie_idx == -1)
    {
        game_log(g, "%s 没有无懈可击", user->name);
        game_multi_wuxie_pass(g);
        return;
    }

    Card* w = player_remove_hand(user, wuxie_idx);
    discard_add(&g->discard, w);
    g->central_show_card = w;
    game_log(g, "%s 打出【无懈可击】", user->name);

    /* 将无懈可击压入栈 */
    int depth = g->multi_wuxie_stack_depth;
    if(depth < MAX_WUXIE_STACK - 1)
    {
        g->multi_wuxie_stack_depth++;
        g->multi_wuxie_stack[depth + 1].user_idx = user_idx;
        g->multi_wuxie_stack[depth + 1].target_idx = g->multi_wuxie_stack[depth].target_idx;
        g->multi_wuxie_stack[depth + 1].trick_target_idx = g->multi_wuxie_stack[depth].trick_target_idx;
        g->multi_wuxie_stack[depth + 1].asker_idx = -1;
        g->multi_wuxie_stack[depth + 1].ask_count = 0;

        /* 询问是否有人反无懈：从打出无懈者的下家开始逆时针 */
        int asker = (user_idx + 1) % g->player_count;
        int asked = 0;
        while(asked < g->player_count)
        {
            if(asker != user_idx && g->players[asker].alive)
            {
                g->multi_wuxie_stack[depth + 1].asker_idx = asker;
                break;
            }
            asker = (asker + 1) % g->player_count;
            asked++;
        }

        if(g->multi_wuxie_stack[depth + 1].asker_idx != -1)
        {
            if(g->players[g->multi_wuxie_stack[depth + 1].asker_idx].is_ai)
            {
                game_countdown_start(g, 1.5f, 0);
            }
            else
            {
                game_countdown_start(g, 1.5f, 0);
                game_log(g, "请点击【无懈可击】反无懈，点击取消放弃");
            }
            return;
        }
    }

    /* 没有人可以反无懈，弹出栈，继续询问 */
    game_multi_wuxie_pass(g);
}

void game_multi_wuxie_pass(GameState* g)
{
    if(!g || g->resp_state != RESPONSE_NEED_MULTI_WUXIE) return;

    game_countdown_stop(g);

    int depth = g->multi_wuxie_stack_depth;
    if(depth < 0) return;

    WuxieStackFrame* frame = &g->multi_wuxie_stack[depth];
    frame->ask_count++;

    /* 检查是否所有玩家都询问过了 */
    if(frame->ask_count >= g->player_count - 1)
    {
        /*
         * 当前层询问完毕：
         *   奇数层(depth=1,3,5...)：最底层无懈生效，锦囊被抵消，直接结算结束
         *   偶数层(depth=2,4,6...)：反无懈生效，上一层无懈被抵消，锦囊恢复生效，
         *                            需要弹回第0层继续询问是否还有人打无懈
         *   depth=0层：初始层询问完毕，无人再打无懈，锦囊生效，结算结束
         */
        int target = frame->target_idx;

        if(depth % 2 == 1)
        {
            /* 奇数层：最底层的无懈生效，目标被无懈，结算结束 */
            g->multi_wuxie_wuxie_mask |= (1 << target);
            game_log(g, "%s 被无懈可击抵消（共%d层无懈）",
                     g->players[target].name, depth);
            g->multi_wuxie_stack_depth = 0;
            g->multi_wuxie_current_target++;
            game_multi_wuxie_advance(g);
            return;
        }
        else if(depth > 0)
        {
            /* 偶数层(depth=2,4,6...)：反无懈生效，锦囊恢复生效
             * 弹回第0层，继续询问是否还有人打无懈 */
            game_log(g, "无懈可击被反无懈抵消（共%d层），锦囊恢复生效，继续询问是否还有无懈可击", depth);
            g->multi_wuxie_stack_depth = 0;
            WuxieStackFrame* base_frame = &g->multi_wuxie_stack[0];
            /* 从第0层当前询问者的下家继续询问 */
            int next_asker = (base_frame->asker_idx + 1) % g->player_count;
            int asked = 0;
            while(asked < g->player_count)
            {
                if(g->players[next_asker].alive)
                {
                    base_frame->asker_idx = next_asker;
                    if(g->players[next_asker].is_ai)
                    {
                        game_countdown_start(g, 1.5f, 0);
                        game_log(g, "%s 思考中...（1.5秒）",
                                 g->players[next_asker].name);
                    }
                    else
                    {
                        game_countdown_start(g, 1.5f, 0);
                        game_log(g, "请点击【无懈可击】选中打出，点击取消放弃");
                    }
                    return;
                }
                next_asker = (next_asker + 1) % g->player_count;
                asked++;
            }
            /* 第0层也没人可问了，锦囊生效 */
            game_log(g, "%s 锦囊正常生效", g->players[target].name);
            g->multi_wuxie_current_target++;
            game_multi_wuxie_advance(g);
            return;
        }
        else
        {
            /* depth=0层询问完毕：锦囊生效 */
            game_log(g, "%s 锦囊正常生效", g->players[target].name);
            g->multi_wuxie_stack_depth = 0;
            g->multi_wuxie_current_target++;
            game_multi_wuxie_advance(g);
            return;
        }
    }

    /* 继续询问下一个玩家（逆时针） */
    int next_asker = (frame->asker_idx + 1) % g->player_count;
    while(next_asker != frame->asker_idx)
    {
        if(g->players[next_asker].alive)
        {
            frame->asker_idx = next_asker;
            if(g->players[next_asker].is_ai)
            {
                game_countdown_start(g, 1.5f, 0);
                game_log(g, "%s 思考中...（1.5秒）",
                         g->players[next_asker].name);
            }
            else
            {
                game_countdown_start(g, 1.5f, 0);
                if(depth > 0)
                    game_log(g, "请点击【无懈可击】反无懈，点击取消放弃");
                else
                    game_log(g, "请点击【无懈可击】选中打出，点击取消放弃");
            }
            return;
        }
        next_asker = (next_asker + 1) % g->player_count;
    }

    /* 没有更多玩家可以询问了（理论上不会到这里） */
    int target2 = frame->target_idx;
    if(depth % 2 == 1)
    {
        g->multi_wuxie_wuxie_mask |= (1 << target2);
    }
    g->multi_wuxie_stack_depth = 0;
    g->multi_wuxie_current_target++;
    game_multi_wuxie_advance(g);
}

void game_multi_wuxie_final_resolve(GameState* g)
{
    if(!g) return;

    g->multi_wuxie_resolved = 1;
    g->resp_state = RESPONSE_NONE;
    game_countdown_stop(g);

    game_log(g, "【无懈可击】结算完毕，开始锦囊效果结算");

    /* 根据锦囊类型进行最终结算 */
    /* 这里简化处理，实际应该根据 trick_type 调用对应的结算函数 */
    g->multi_wuxie_card = NULL;
    g->multi_wuxie_source = -1;
    g->multi_wuxie_target_count = 0;
    g->multi_wuxie_current_target = 0;
    g->multi_wuxie_stack_depth = 0;
}
