#include "state_encoder.h"
#include <string.h>
#include <math.h>

/* ===== 工具函数 ===== */

static void one_hot(int value, int dim, float* out) {
    memset(out, 0, dim * sizeof(float));
    if (value >= 0 && value < dim) {
        out[value] = 1.0f;
    }
}

/* 编码一张牌：23维 [suit(5) + rank(14) + type(3) + is_none(1)] */
/* Python端花色：0=无,1=黑桃,2=红桃,3=梅花,4=方片 */
/* C端花色：SUIT_SPADE=0, SUIT_HEART=1, SUIT_CLUB=2, SUIT_DIAMOND=3, SUIT_NONE=-1 */
static void encode_card(Card* c, float* out) {
    memset(out, 0, NN_CARD_DIM * sizeof(float));

    if (!c) {
        out[NN_CARD_DIM - 1] = 1.0f; /* is_none = 1 */
        return;
    }

    /* 花色转换：C端+1 = Python端（无=0） */
    int py_suit = (c->suit == SUIT_NONE) ? 0 : (c->suit + 1);
    one_hot(py_suit, 5, out);

    /* 点数：0=无, 1-13=A-K */
    int py_rank = (c->rank >= 1 && c->rank <= 13) ? c->rank : 0;
    one_hot(py_rank, 14, out + 5);

    /* 类型：0=基本, 1=锦囊(含延时), 2=装备 */
    int py_type;
    if (c->type == CARD_EQUIP) py_type = 2;
    else if (c->type == CARD_DELAYED) py_type = 1;
    else py_type = c->type; /* CARD_BASIC=0, CARD_TRICK=1 */
    one_hot(py_type, 3, out + 5 + 14);

    out[5 + 14 + 3] = 0.0f; /* is_none = 0 */
}

/* 编码一个玩家：796维 */
static void encode_player(Player* p, int is_self, float* out) {
    int idx = 0;

    if (!p) {
        memset(out, 0, NN_PLAYER_DIM * sizeof(float));
        return;
    }

    /* 血量（2维）：hp/max_hp, max_hp/10 */
    out[idx++] = (float)p->hp / (float)(p->max_hp > 0 ? p->max_hp : 1);
    out[idx++] = (float)p->max_hp / 10.0f;

    /* 盾（1维）：shield/5 */
    out[idx++] = (float)p->shield / 5.0f;

    /* 存活状态（1维） */
    out[idx++] = p->alive ? 1.0f : 0.0f;

    /* 武将ID（8维one-hot） */
    one_hot(p->hero_id, 8, out + idx);
    idx += 8;

    /* 手牌数（1维）：hand_count/30 */
    out[idx++] = (float)p->hand_count / 30.0f;

    /* 手牌（30张 × 23维 = 690维） */
    /* 自己的手牌明牌，其他玩家用空牌填充 */
    for (int i = 0; i < NN_MAX_HAND; i++) {
        Card* c = NULL;
        if (is_self && i < p->hand_count) {
            c = p->hand[i];
        }
        encode_card(c, out + idx);
        idx += NN_CARD_DIM;
    }

    /* 装备区（4张 × 23维 = 92维） */
    /* 武器、防具、进攻马、防御马 */
    Card* equips[4] = {p->equip.weapon, p->equip.armor,
                       p->equip.horse_atk, p->equip.horse_def};
    for (int i = 0; i < 4; i++) {
        encode_card(equips[i], out + idx);
        idx += NN_CARD_DIM;
    }

    /* 判定区数量（1维）：judge_count/3 */
    out[idx++] = (float)p->judge.count / 3.0f;

    /* 是否是自己（1维） */
    out[idx++] = is_self ? 1.0f : 0.0f;
}

/* ===== 主函数：编码完整游戏状态 ===== */
void encode_game_state(GameState* g, int ai_player_idx,
                       float* global_data,
                       float* players_data,
                       float* mask_data) {
    if (!g || !global_data || !players_data || !mask_data) return;

    int idx = 0;

    /* ===== 全局特征（53维）===== */

    /* 回合数（1维）：turn_count/50 */
    global_data[idx++] = (float)g->turn_count / 50.0f;

    /* 当前玩家（8维one-hot） */
    one_hot(g->current_player, 8, global_data + idx);
    idx += 8;

    /* 阶段（7维one-hot） */
    int phase = g->phase;
    if (phase < 0 || phase > 6) phase = 6; /* PHASE_GAME_OVER */
    one_hot(phase, 7, global_data + idx);
    idx += 7;

    /* 响应状态（34维one-hot） */
    int resp = g->resp_state;
    if (resp < 0 || resp >= 34) resp = 0;
    one_hot(resp, 34, global_data + idx);
    idx += 34;

    /* 牌堆数（1维）：deck_count/100 */
    global_data[idx++] = (float)g->deck.count / 100.0f;

    /* 弃牌堆数（1维）：discard_count/100 */
    global_data[idx++] = (float)g->discard.count / 100.0f;

    /* 存活人数（1维）：alive_count/player_count */
    int alive_count = 0;
    for (int i = 0; i < g->player_count; i++) {
        if (g->players[i].alive) alive_count++;
    }
    global_data[idx++] = (float)alive_count / (float)(g->player_count > 0 ? g->player_count : 1);

    /* ===== 玩家特征（8 × 796维）===== */
    for (int i = 0; i < NN_MAX_PLAYERS; i++) {
        int is_self = (i == ai_player_idx);
        if (i < g->player_count) {
            encode_player(&g->players[i], is_self, players_data + i * NN_PLAYER_DIM);
            mask_data[i] = g->players[i].alive ? 1.0f : 0.0f;
        } else {
            /* 填充空玩家 */
            memset(players_data + i * NN_PLAYER_DIM, 0, NN_PLAYER_DIM * sizeof(float));
            mask_data[i] = 0.0f;
        }
    }
}
