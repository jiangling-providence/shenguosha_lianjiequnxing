/* ================================================================
 * GameState 完整序列化/反序列化
 * 用于MCTS模拟时保存/恢复游戏状态，避免每次启动新进程
 * ================================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "card.h"
#include "player.h"

/* ================================================================
 * 状态验证函数：反序列化后检查状态一致性，防止崩溃
 * 返回1=有效，0=无效
 * ================================================================ */
static int game_state_validate(GameState* g)
{
    if(!g) return 0;

    /* 检查玩家数量 */
    if(g->player_count < 1 || g->player_count > MAX_PLAYERS) {
        fprintf(stderr, "[VALIDATE] player_count=%d 无效\n", g->player_count);
        return 0;
    }

    /* 检查每个玩家 */
    for(int i = 0; i < g->player_count; i++) {
        Player* p = &g->players[i];
        if(!p) return 0;

        /* 检查手牌数量 */
        if(p->hand_count < 0 || p->hand_count > MAX_HAND_CARDS) {
            fprintf(stderr, "[VALIDATE] 玩家%d hand_count=%d 无效\n", i, p->hand_count);
            return 0;
        }

        /* 检查手牌指针 */
        for(int j = 0; j < p->hand_count; j++) {
            if(!p->hand[j]) {
                fprintf(stderr, "[VALIDATE] 玩家%d 手牌%d为NULL\n", i, j);
                return 0;
            }
        }

        /* 检查体力 */
        if(p->hp < 0 || p->hp > p->max_hp + 5) {
            fprintf(stderr, "[VALIDATE] 玩家%d hp=%d max_hp=%d 无效\n", i, p->hp, p->max_hp);
            return 0;
        }

        /* 检查hero指针 */
        if(!p->hero) {
            fprintf(stderr, "[VALIDATE] 玩家%d hero为NULL (hero_id=%d)\n", i, p->hero_id);
            return 0;
        }

        /* 检查判定区数量 */
        if(p->judge.count < 0 || p->judge.count > MAX_JUDGE_CARDS) {
            fprintf(stderr, "[VALIDATE] 玩家%d judge.count=%d 无效\n", i, p->judge.count);
            return 0;
        }
    }

    /* 检查阶段 */
    if(g->phase < 0 || g->phase > 6) {
        fprintf(stderr, "[VALIDATE] phase=%d 无效\n", g->phase);
        return 0;
    }

    /* 检查响应状态 */
    if(g->resp_state < 0 || g->resp_state > 100) {
        fprintf(stderr, "[VALIDATE] resp_state=%d 无效\n", g->resp_state);
        return 0;
    }

    /* 检查牌堆 */
    if(g->deck.count < 0 || g->deck.count > 500) {
        fprintf(stderr, "[VALIDATE] deck.count=%d 无效\n", g->deck.count);
        return 0;
    }

    /* 检查弃牌堆 */
    if(g->discard.count < 0 || g->discard.count > 500) {
        fprintf(stderr, "[VALIDATE] discard.count=%d 无效\n", g->discard.count);
        return 0;
    }

    /* 检查当前玩家 */
    if(g->current_player < 0 || g->current_player >= g->player_count) {
        fprintf(stderr, "[VALIDATE] current_player=%d player_count=%d 无效\n",
                g->current_player, g->player_count);
        return 0;
    }

    return 1;  /* 验证通过 */
}

/* ================================================================
 * 缓冲区读写辅助函数
 * ================================================================ */
static void write_int(unsigned char* buf, int* offset, int val)
{
    memcpy(buf + *offset, &val, sizeof(int));
    *offset += sizeof(int);
}

static int read_int(unsigned char* buf, int* offset)
{
    int val;
    memcpy(&val, buf + *offset, sizeof(int));
    *offset += sizeof(int);
    return val;
}

static void write_float(unsigned char* buf, int* offset, float val)
{
    memcpy(buf + *offset, &val, sizeof(float));
    *offset += sizeof(float);
}

static float read_float(unsigned char* buf, int* offset)
{
    float val;
    memcpy(&val, buf + *offset, sizeof(float));
    *offset += sizeof(float);
    return val;
}

static void write_bytes(unsigned char* buf, int* offset, void* data, int size)
{
    memcpy(buf + *offset, data, size);
    *offset += size;
}

static void read_bytes(unsigned char* buf, int* offset, void* data, int size)
{
    memcpy(data, buf + *offset, size);
    *offset += size;
}

/* ================================================================
 * Card 序列化/反序列化
 * ================================================================ */
static int card_serialized_size(void)
{
    /* id, type, suit, color, card_nature, rank, is_valid = 7 ints
     * + sub union: 取最大的equip子结构大小
     * basic: basic_type + sha_element = 2 ints
     * trick: trick_type = 1 int
     * delayed: delayed_type = 1 int
     * equip: equip_type + weapon_type + range + armor_type = 4 ints
     * 取最大 = 4 ints
     */
    return (7 + 4) * sizeof(int);
}

static void card_serialize(Card* c, unsigned char* buf, int* offset)
{
    if(!c) { write_int(buf, offset, 0); return; }
    write_int(buf, offset, 1);  /* 非NULL标记 */
    write_int(buf, offset, c->id);
    write_int(buf, offset, (int)c->type);
    write_int(buf, offset, (int)c->suit);
    write_int(buf, offset, (int)c->color);
    write_int(buf, offset, (int)c->card_nature);
    write_int(buf, offset, (int)c->rank);
    write_int(buf, offset, c->is_valid);

    /* 序列化sub union，按type区分 */
    if(c->type == CARD_BASIC) {
        write_int(buf, offset, (int)c->sub.basic.basic_type);
        write_int(buf, offset, (int)c->sub.basic.sha_element);
        write_int(buf, offset, 0);  /* 填充 */
        write_int(buf, offset, 0);  /* 填充 */
    } else if(c->type == CARD_TRICK) {
        write_int(buf, offset, (int)c->sub.trick.trick_type);
        write_int(buf, offset, 0);  /* 填充 */
        write_int(buf, offset, 0);  /* 填充 */
        write_int(buf, offset, 0);  /* 填充 */
    } else if(c->type == CARD_DELAYED) {
        write_int(buf, offset, (int)c->sub.delayed.delayed_type);
        write_int(buf, offset, 0);  /* 填充 */
        write_int(buf, offset, 0);  /* 填充 */
        write_int(buf, offset, 0);  /* 填充 */
    } else if(c->type == CARD_EQUIP) {
        write_int(buf, offset, (int)c->sub.equip.equip_type);
        if(c->sub.equip.equip_type == EQUIP_WEAPON) {
            write_int(buf, offset, (int)c->sub.equip.detail.weapon.weapon_type);
            write_int(buf, offset, c->sub.equip.detail.weapon.range);
            write_int(buf, offset, 0);  /* 填充 */
        } else if(c->sub.equip.equip_type == EQUIP_ARMOR) {
            write_int(buf, offset, (int)c->sub.equip.detail.armor.armor_type);
            write_int(buf, offset, 0);  /* 填充 */
            write_int(buf, offset, 0);  /* 填充 */
        } else {
            write_int(buf, offset, 0);
            write_int(buf, offset, 0);
            write_int(buf, offset, 0);
        }
    } else {
        write_int(buf, offset, 0);
        write_int(buf, offset, 0);
        write_int(buf, offset, 0);
        write_int(buf, offset, 0);
    }
}

static Card* card_deserialize(unsigned char* buf, int* offset)
{
    int exists = read_int(buf, offset);
    if(!exists) return NULL;

    Card* c = (Card*)malloc(sizeof(Card));
    memset(c, 0, sizeof(Card));
    c->id = read_int(buf, offset);
    c->type = (CardType)read_int(buf, offset);
    c->suit = (Suit)read_int(buf, offset);
    c->color = (CardColor)read_int(buf, offset);
    c->card_nature = (CardNature)read_int(buf, offset);
    c->rank = (Rank)read_int(buf, offset);
    c->is_valid = read_int(buf, offset);

    /* 反序列化sub union */
    if(c->type == CARD_BASIC) {
        c->sub.basic.basic_type = (BasicType)read_int(buf, offset);
        c->sub.basic.sha_element = (ShaElement)read_int(buf, offset);
        read_int(buf, offset);  /* 填充 */
        read_int(buf, offset);  /* 填充 */
    } else if(c->type == CARD_TRICK) {
        c->sub.trick.trick_type = (TrickType)read_int(buf, offset);
        read_int(buf, offset);
        read_int(buf, offset);
        read_int(buf, offset);
    } else if(c->type == CARD_DELAYED) {
        c->sub.delayed.delayed_type = (DelayedType)read_int(buf, offset);
        read_int(buf, offset);
        read_int(buf, offset);
        read_int(buf, offset);
    } else if(c->type == CARD_EQUIP) {
        c->sub.equip.equip_type = (EquipType)read_int(buf, offset);
        if(c->sub.equip.equip_type == EQUIP_WEAPON) {
            c->sub.equip.detail.weapon.weapon_type = (WeaponType)read_int(buf, offset);
            c->sub.equip.detail.weapon.range = read_int(buf, offset);
            read_int(buf, offset);
        } else if(c->sub.equip.equip_type == EQUIP_ARMOR) {
            c->sub.equip.detail.armor.armor_type = (ArmorType)read_int(buf, offset);
            read_int(buf, offset);
            read_int(buf, offset);
        } else {
            read_int(buf, offset);
            read_int(buf, offset);
            read_int(buf, offset);
        }
    } else {
        read_int(buf, offset);
        read_int(buf, offset);
        read_int(buf, offset);
        read_int(buf, offset);
    }
    return c;
}

/* Card*指针序列化（处理NULL） */
static void card_ptr_serialize(Card* c, unsigned char* buf, int* offset)
{
    card_serialize(c, buf, offset);
}

static Card* card_ptr_deserialize(unsigned char* buf, int* offset)
{
    return card_deserialize(buf, offset);
}

/* ================================================================
 * Deck 序列化/反序列化
 * ================================================================ */
static void deck_serialize(Deck* d, unsigned char* buf, int* offset)
{
    write_int(buf, offset, d->count);
    write_int(buf, offset, d->capacity);
    write_int(buf, offset, d->top);
    for(int i = 0; i < d->count; i++) {
        card_ptr_serialize(d->cards[i], buf, offset);
    }
}

static void deck_deserialize(Deck* d, unsigned char* buf, int* offset)
{
    d->count = read_int(buf, offset);
    d->capacity = read_int(buf, offset);
    d->top = read_int(buf, offset);
    if(d->capacity > 0) {
        d->cards = (Card**)malloc(sizeof(Card*) * d->capacity);
        memset(d->cards, 0, sizeof(Card*) * d->capacity);
    } else {
        d->cards = NULL;
    }
    for(int i = 0; i < d->count; i++) {
        d->cards[i] = card_ptr_deserialize(buf, offset);
    }
}

/* ================================================================
 * EquipArea 序列化/反序列化
 * ================================================================ */
static void equip_serialize(EquipArea* e, unsigned char* buf, int* offset)
{
    card_ptr_serialize(e->weapon, buf, offset);
    card_ptr_serialize(e->armor, buf, offset);
    card_ptr_serialize(e->horse_atk, buf, offset);
    card_ptr_serialize(e->horse_def, buf, offset);
    for(int i = 0; i < 4; i++) write_int(buf, offset, e->feiwuu_placed[i]);
}

static void equip_deserialize(EquipArea* e, unsigned char* buf, int* offset)
{
    e->weapon = card_ptr_deserialize(buf, offset);
    e->armor = card_ptr_deserialize(buf, offset);
    e->horse_atk = card_ptr_deserialize(buf, offset);
    e->horse_def = card_ptr_deserialize(buf, offset);
    for(int i = 0; i < 4; i++) e->feiwuu_placed[i] = read_int(buf, offset);
}

/* ================================================================
 * JudgeArea 序列化/反序列化
 * ================================================================ */
static void judge_serialize(JudgeArea* j, unsigned char* buf, int* offset)
{
    write_int(buf, offset, j->count);
    for(int i = 0; i < MAX_JUDGE_CARDS; i++) {
        card_ptr_serialize(j->cards[i], buf, offset);
    }
}

static void judge_deserialize(JudgeArea* j, unsigned char* buf, int* offset)
{
    j->count = read_int(buf, offset);
    for(int i = 0; i < MAX_JUDGE_CARDS; i++) {
        j->cards[i] = card_ptr_deserialize(buf, offset);
    }
}

/* ================================================================
 * Player 序列化/反序列化
 * ================================================================ */
static void player_serialize(Player* p, unsigned char* buf, int* offset)
{
    write_int(buf, offset, p->id);
    write_bytes(buf, offset, p->name, 32);
    write_int(buf, offset, p->hp);
    write_int(buf, offset, p->max_hp);
    write_int(buf, offset, p->hidden_hp);
    write_int(buf, offset, p->max_hidden_hp);
    write_int(buf, offset, p->alive);
    write_int(buf, offset, p->is_ai);

    /* 手牌 */
    write_int(buf, offset, p->hand_count);
    for(int i = 0; i < MAX_HAND_CARDS; i++) {
        card_ptr_serialize(p->hand[i], buf, offset);
    }

    /* 装备区、判定区 */
    equip_serialize(&p->equip, buf, offset);
    judge_serialize(&p->judge, buf, offset);

    /* 回合状态 */
    write_int(buf, offset, p->sha_used);
    write_int(buf, offset, p->jiu_used);
    write_int(buf, offset, p->skip_draw);
    write_int(buf, offset, p->skip_play);
    write_int(buf, offset, p->chained);
    write_int(buf, offset, p->flipped);
    write_int(buf, offset, p->damage_taken_count);

    /* 武将系统 */
    write_int(buf, offset, (int)p->hero_id);
    write_int(buf, offset, p->shield);
    for(int i = 0; i < 4; i++) write_int(buf, offset, p->skill_used[i]);
    /* 保存hero技能运行时状态（used_count, active, enabled） */
    {
        Hero* h = hero_get(p->hero_id);
        int skill_count = h ? h->skill_count : 0;
        write_int(buf, offset, skill_count);
        for(int i = 0; i < 4; i++) {
            if(h && i < skill_count) {
                write_int(buf, offset, h->skills[i].used_count);
                write_int(buf, offset, h->skills[i].active);
                write_int(buf, offset, h->skills[i].enabled);
            } else {
                write_int(buf, offset, 0);
                write_int(buf, offset, 0);
                write_int(buf, offset, 1);
            }
        }
    }
    for(int i = 0; i < 4; i++) write_int(buf, offset, p->suits_used[i]);
    write_int(buf, offset, p->immune_suit);

    /* 雨蝶专用数据 */
    write_int(buf, offset, p->yudie.break_suit);
    write_int(buf, offset, p->yudie.feiwuu_count);
    write_int(buf, offset, p->yudie.feiwuu_cards);
    for(int i = 0; i < 20; i++) write_bytes(buf, offset, p->yudie.card_names[i], 32);
    write_int(buf, offset, p->yudie.card_name_count);
    write_int(buf, offset, p->yudie.chengdie);
    write_int(buf, offset, p->yudie.last_suit);
    write_int(buf, offset, p->yudie.chengxing_count);
    write_int(buf, offset, p->yudie.first_card_played);
    write_int(buf, offset, p->yudie.huaxing_used_suits);
    write_int(buf, offset, p->yudie.huaxing_target);
    write_int(buf, offset, p->yudie.huaxing_free_suits);
    write_int(buf, offset, p->yudie.huadie_active);
    for(int i = 0; i < 10; i++) write_bytes(buf, offset, p->yudie.huaxing_used_tricks[i], 32);
    write_int(buf, offset, p->yudie.huaxing_used_trick_count);
    for(int i = 0; i < 10; i++) write_bytes(buf, offset, p->yudie.huaxing_played_names[i], 32);
    write_int(buf, offset, p->yudie.huaxing_played_name_count);
    for(int i = 0; i < 20; i++) write_bytes(buf, offset, p->yudie.huaxing_record_names[i], 32);
    for(int i = 0; i < 20; i++) write_int(buf, offset, p->yudie.huaxing_record_suits[i]);
    for(int i = 0; i < 20; i++) write_int(buf, offset, p->yudie.huaxing_record_ranks[i]);
    write_int(buf, offset, p->yudie.huaxing_record_count);
    for(int i = 0; i < 2; i++) write_int(buf, offset, p->yudie.huaxing_response_used[i]);

    /* 流萤专用数据 */
    write_int(buf, offset, p->liuying.bengfa_element);
    write_int(buf, offset, p->liuying.guozai_sha_active);
    write_int(buf, offset, p->liuying.guozai_sha_target);
    write_int(buf, offset, p->liuying.chaoxing_used_play);
    write_int(buf, offset, p->liuying.chaoxing_used_end);

    /* 镜流专用数据 */
    write_int(buf, offset, p->jingliu.hong_marks);
    write_int(buf, offset, p->jingliu.transformation);
    write_int(buf, offset, p->jingliu.wuxia_suit_count);
    write_int(buf, offset, p->jingliu.wuxia_used);
    write_int(buf, offset, p->jingliu.sha_extra_target);
    write_int(buf, offset, p->jingliu.sha_damage_plus);
    write_int(buf, offset, p->jingliu.allow_zone_card);
    write_int(buf, offset, p->jingliu.next_sha_unblockable);
    write_int(buf, offset, p->jingliu.gujing_play_opt1);
    write_int(buf, offset, p->jingliu.gujing_play_opt2);
    write_int(buf, offset, p->jingliu.gujing_resp_used);
    write_int(buf, offset, p->jingliu.dying_judged);

    /* 赵云专用数据 */
    write_int(buf, offset, p->longdan_active);
}

static void player_deserialize(Player* p, unsigned char* buf, int* offset)
{
    memset(p, 0, sizeof(Player));
    p->id = read_int(buf, offset);
    read_bytes(buf, offset, p->name, 32);
    p->hp = read_int(buf, offset);
    p->max_hp = read_int(buf, offset);
    p->hidden_hp = read_int(buf, offset);
    p->max_hidden_hp = read_int(buf, offset);
    p->alive = read_int(buf, offset);
    p->is_ai = read_int(buf, offset);

    /* 手牌 */
    p->hand_count = read_int(buf, offset);
    for(int i = 0; i < MAX_HAND_CARDS; i++) {
        p->hand[i] = card_ptr_deserialize(buf, offset);
    }

    /* 装备区、判定区 */
    equip_deserialize(&p->equip, buf, offset);
    judge_deserialize(&p->judge, buf, offset);

    /* 回合状态 */
    p->sha_used = read_int(buf, offset);
    p->jiu_used = read_int(buf, offset);
    p->skip_draw = read_int(buf, offset);
    p->skip_play = read_int(buf, offset);
    p->chained = read_int(buf, offset);
    p->flipped = read_int(buf, offset);
    p->damage_taken_count = read_int(buf, offset);

    /* 武将系统 */
    p->hero_id = (HeroId)read_int(buf, offset);
    p->hero = hero_get(p->hero_id);  /* 重新设置hero指针 */
    p->shield = read_int(buf, offset);
    for(int i = 0; i < 4; i++) p->skill_used[i] = read_int(buf, offset);
    /* 恢复hero技能运行时状态（used_count, active, enabled） */
    {
        int saved_skill_count = read_int(buf, offset);
        Hero* h = hero_get(p->hero_id);
        if(h) {
            for(int i = 0; i < 4; i++) {
                int uc = read_int(buf, offset);
                int ac = read_int(buf, offset);
                int en = read_int(buf, offset);
                if(i < h->skill_count) {
                    h->skills[i].used_count = uc;
                    h->skills[i].active = ac;
                    h->skills[i].enabled = en;
                }
            }
        } else {
            /* 没有hero指针，跳过读取 */
            for(int i = 0; i < 4; i++) {
                read_int(buf, offset);
                read_int(buf, offset);
                read_int(buf, offset);
            }
        }
    }
    for(int i = 0; i < 4; i++) p->suits_used[i] = read_int(buf, offset);
    p->immune_suit = read_int(buf, offset);

    /* 雨蝶专用数据 */
    p->yudie.break_suit = read_int(buf, offset);
    p->yudie.feiwuu_count = read_int(buf, offset);
    p->yudie.feiwuu_cards = read_int(buf, offset);
    for(int i = 0; i < 20; i++) read_bytes(buf, offset, p->yudie.card_names[i], 32);
    p->yudie.card_name_count = read_int(buf, offset);
    p->yudie.chengdie = read_int(buf, offset);
    p->yudie.last_suit = read_int(buf, offset);
    p->yudie.chengxing_count = read_int(buf, offset);
    p->yudie.first_card_played = read_int(buf, offset);
    p->yudie.huaxing_used_suits = read_int(buf, offset);
    p->yudie.huaxing_target = read_int(buf, offset);
    p->yudie.huaxing_free_suits = read_int(buf, offset);
    p->yudie.huadie_active = read_int(buf, offset);
    for(int i = 0; i < 10; i++) read_bytes(buf, offset, p->yudie.huaxing_used_tricks[i], 32);
    p->yudie.huaxing_used_trick_count = read_int(buf, offset);
    for(int i = 0; i < 10; i++) read_bytes(buf, offset, p->yudie.huaxing_played_names[i], 32);
    p->yudie.huaxing_played_name_count = read_int(buf, offset);
    for(int i = 0; i < 20; i++) read_bytes(buf, offset, p->yudie.huaxing_record_names[i], 32);
    for(int i = 0; i < 20; i++) p->yudie.huaxing_record_suits[i] = read_int(buf, offset);
    for(int i = 0; i < 20; i++) p->yudie.huaxing_record_ranks[i] = read_int(buf, offset);
    p->yudie.huaxing_record_count = read_int(buf, offset);
    for(int i = 0; i < 2; i++) p->yudie.huaxing_response_used[i] = read_int(buf, offset);

    /* 流萤专用数据 */
    p->liuying.bengfa_element = read_int(buf, offset);
    p->liuying.guozai_sha_active = read_int(buf, offset);
    p->liuying.guozai_sha_target = read_int(buf, offset);
    p->liuying.chaoxing_used_play = read_int(buf, offset);
    p->liuying.chaoxing_used_end = read_int(buf, offset);

    /* 镜流专用数据 */
    p->jingliu.hong_marks = read_int(buf, offset);
    p->jingliu.transformation = read_int(buf, offset);
    p->jingliu.wuxia_suit_count = read_int(buf, offset);
    p->jingliu.wuxia_used = read_int(buf, offset);
    p->jingliu.sha_extra_target = read_int(buf, offset);
    p->jingliu.sha_damage_plus = read_int(buf, offset);
    p->jingliu.allow_zone_card = read_int(buf, offset);
    p->jingliu.next_sha_unblockable = read_int(buf, offset);
    p->jingliu.gujing_play_opt1 = read_int(buf, offset);
    p->jingliu.gujing_play_opt2 = read_int(buf, offset);
    p->jingliu.gujing_resp_used = read_int(buf, offset);
    p->jingliu.dying_judged = read_int(buf, offset);

    /* 赵云专用数据 */
    p->longdan_active = read_int(buf, offset);
}

/* ================================================================
 * GameState 序列化/反序列化
 * ================================================================ */
int game_state_serialized_size(GameState* g)
{
    /* 估算大小，实际可能更大，用1MB足够 */
    (void)g;
    return 1024 * 1024;
}

void game_state_serialize(GameState* g, unsigned char* buf)
{
    int offset = 0;

    /* 玩家 */
    write_int(buf, &offset, g->player_count);
    for(int i = 0; i < MAX_PLAYERS; i++) {
        player_serialize(&g->players[i], buf, &offset);
    }

    /* 牌堆、弃牌堆 */
    deck_serialize(&g->deck, buf, &offset);
    deck_serialize(&g->discard, buf, &offset);

    /* 基本状态 */
    write_int(buf, &offset, g->current_player);
    write_int(buf, &offset, (int)g->phase);
    write_int(buf, &offset, g->game_over);
    write_int(buf, &offset, g->winner_id);
    write_int(buf, &offset, g->turn_count);

    /* 角色选择 */
    write_int(buf, &offset, g->player_hero_id);
    write_int(buf, &offset, g->ai_hero_id);
    write_int(buf, &offset, g->select_hover);
    write_int(buf, &offset, g->selected_hero_idx);

    /* 日志（不序列化，跳过） */
    /* log_buf, log_count, log_panel_open, log_scroll 不影响游戏逻辑 */

    /* 响应状态 */
    write_int(buf, &offset, (int)g->resp_state);
    card_ptr_serialize(g->central_show_card, buf, &offset);

    /* 展示牌 */
    card_ptr_serialize(g->show_card_center, buf, &offset);
    write_bytes(buf, &offset, g->show_card_who, 32);
    write_int(buf, &offset, g->show_card_timer);

    for(int i = 0; i < 10; i++) card_ptr_serialize(g->show_cards_center[i], buf, &offset);
    write_int(buf, &offset, g->show_cards_count);
    write_int(buf, &offset, g->show_cards_timer);
    write_int(buf, &offset, g->show_cards_total);
    write_bytes(buf, &offset, g->show_cards_who, 32);

    write_bytes(buf, &offset, g->center_message, 256);
    write_int(buf, &offset, g->center_message_timer);

    write_int(buf, &offset, g->long_press_skill_idx);

    /* 决斗/响应 */
    write_int(buf, &offset, g->duel_turn);
    write_int(buf, &offset, g->resp_source_player);
    write_int(buf, &offset, g->resp_target_player);
    card_ptr_serialize(g->resp_trigger_card, buf, &offset);
    write_int(buf, &offset, g->resp_need_basic_after_wuxie);
    write_int(buf, &offset, g->resp_required_basic);
    write_int(buf, &offset, g->ai_play_finished);

    /* 群体锦囊 */
    write_int(buf, &offset, g->group_active);
    write_int(buf, &offset, g->group_phase);
    write_int(buf, &offset, g->group_current);
    write_int(buf, &offset, g->group_source);
    write_int(buf, &offset, g->group_trick_type);
    card_ptr_serialize(g->group_trigger_card, buf, &offset);
    write_int(buf, &offset, g->group_wuxie_mask);
    write_int(buf, &offset, g->group_wuxie_counter_from);
    for(int i = 0; i < 16; i++) card_ptr_serialize(g->group_wugu_pile[i], buf, &offset);
    write_int(buf, &offset, g->group_wugu_count);

    /* 伤害来源 */
    write_int(buf, &offset, g->current_damage_source);
    write_int(buf, &offset, g->current_damage_source_player);

    /* 火攻 */
    write_int(buf, &offset, g->huogong_active);
    write_int(buf, &offset, g->huogong_source);
    write_int(buf, &offset, g->huogong_target);
    card_ptr_serialize(g->huogong_show_card, buf, &offset);
    write_int(buf, &offset, g->huogong_need_suit);
    write_int(buf, &offset, g->huogong_picked_hand);

    /* 贯石斧 */
    write_int(buf, &offset, g->guanshi_active);
    write_int(buf, &offset, g->guanshi_source);
    write_int(buf, &offset, g->guanshi_target);
    write_int(buf, &offset, g->guanshi_damage);
    write_int(buf, &offset, g->guanshi_picking);
    write_int(buf, &offset, g->guanshi_picked_count);
    for(int i = 0; i < 2; i++) write_int(buf, &offset, g->guanshi_picked[i]);

    /* 雨蝶飞舞选牌 */
    write_int(buf, &offset, g->feiwuu_selected_count);
    for(int i = 0; i < 4; i++) write_int(buf, &offset, g->feiwuu_selected[i]);

    /* 寒冰剑 */
    write_int(buf, &offset, g->hanbing_active);
    write_int(buf, &offset, g->hanbing_source);
    write_int(buf, &offset, g->hanbing_target);
    write_int(buf, &offset, g->hanbing_damage);
    write_int(buf, &offset, g->hanbing_picking);
    write_int(buf, &offset, g->hanbing_picked_count);
    for(int i = 0; i < 2; i++) write_int(buf, &offset, g->hanbing_picked_type[i]);
    for(int i = 0; i < 2; i++) write_int(buf, &offset, g->hanbing_picked_index[i]);

    /* 过河拆桥/顺手牵羊 */
    write_int(buf, &offset, g->pick_enemy_target);
    write_int(buf, &offset, g->pick_enemy_action);
    write_int(buf, &offset, g->pick_enemy_card_type);
    write_int(buf, &offset, g->pick_enemy_card_index);

    /* 圣骑士弃牌 */
    write_int(buf, &offset, g->paladin_discard_option);
    write_int(buf, &offset, g->paladin_discard_selected);

    /* 通用弃牌 */
    write_int(buf, &offset, g->generic_discard_player);
    write_int(buf, &offset, g->generic_discard_need);
    for(int i = 0; i < 10; i++) write_int(buf, &offset, g->generic_discard_selected[i]);
    write_int(buf, &offset, g->generic_discard_selected_count);
    write_int(buf, &offset, g->generic_discard_source);
    write_int(buf, &offset, g->generic_discard_done);

    /* 确认出牌 */
    write_int(buf, &offset, g->confirm_play_hand_index);
    write_int(buf, &offset, g->confirm_play_target_index);

    /* 铁索连环 */
    write_int(buf, &offset, g->tiesuo_hand_index);
    for(int i = 0; i < 2; i++) write_int(buf, &offset, g->tiesuo_targets[i]);
    write_int(buf, &offset, g->tiesuo_target_count);
    write_int(buf, &offset, g->tiesuo_wuxie_index);
    write_int(buf, &offset, g->tiesuo_wuxie_mask);

    /* 多目标锦囊无懈 */
    card_ptr_serialize(g->multi_wuxie_card, buf, &offset);
    write_int(buf, &offset, g->multi_wuxie_source);
    for(int i = 0; i < MAX_PLAYERS; i++) write_int(buf, &offset, g->multi_wuxie_targets[i]);
    write_int(buf, &offset, g->multi_wuxie_target_count);
    write_int(buf, &offset, g->multi_wuxie_current_target);
    write_int(buf, &offset, g->multi_wuxie_wuxie_mask);
    for(int i = 0; i < MAX_WUXIE_STACK; i++) {
        write_int(buf, &offset, g->multi_wuxie_stack[i].user_idx);
        write_int(buf, &offset, g->multi_wuxie_stack[i].target_idx);
        write_int(buf, &offset, g->multi_wuxie_stack[i].trick_target_idx);
        write_int(buf, &offset, g->multi_wuxie_stack[i].asker_idx);
        write_int(buf, &offset, g->multi_wuxie_stack[i].ask_count);
    }
    write_int(buf, &offset, g->multi_wuxie_stack_depth);
    write_int(buf, &offset, g->multi_wuxie_trick_type);
    write_int(buf, &offset, g->multi_wuxie_resolved);

    /* 倒计时 */
    write_int(buf, &offset, g->countdown.active);
    write_float(buf, &offset, g->countdown.remaining);
    write_float(buf, &offset, g->countdown.duration);
    write_int(buf, &offset, g->countdown.callback_type);

    /* 群体锦囊指定目标 */
    write_int(buf, &offset, g->group_target_hand_index);
    for(int i = 0; i < MAX_PLAYERS; i++) write_int(buf, &offset, g->group_target_order[i]);
    write_int(buf, &offset, g->group_target_count);
    write_int(buf, &offset, g->group_target_current);
    write_int(buf, &offset, g->group_target_confirmed);

    /* 化形技能 */
    write_int(buf, &offset, g->huaxing_current_suit);
    write_int(buf, &offset, g->huaxing_selected_hand);
    write_int(buf, &offset, g->huaxing_selected_trick);
    for(int i = 0; i < 4; i++) write_int(buf, &offset, g->huaxing_suit_order[i]);
    write_int(buf, &offset, g->huaxing_suit_count);
    write_int(buf, &offset, g->huaxing_suit_index);
    write_int(buf, &offset, g->huaxing_trick_type);
    card_ptr_serialize(g->huaxing_used_card, buf, &offset);
    write_int(buf, &offset, g->huaxing_selecting_target);
    write_int(buf, &offset, g->huaxing_after_group);
    write_int(buf, &offset, g->huaxing_after_pick);
    write_int(buf, &offset, g->huaxing_after_duel);
    write_int(buf, &offset, g->huaxing_after_huogong);

    /* 雨蝶飞舞拖拽 */
    for(int i = 0; i < 4; i++) card_ptr_serialize(g->feiwuu_drag_cards[i], buf, &offset);
    write_int(buf, &offset, g->feiwuu_drag_count);
    write_int(buf, &offset, g->feiwuu_drag_index);
    write_int(buf, &offset, g->feiwuu_dragging);
    write_int(buf, &offset, g->feiwuu_drag_x);
    write_int(buf, &offset, g->feiwuu_drag_y);
    for(int i = 0; i < 4; i++) write_int(buf, &offset, g->feiwuu_placed_slots[i]);

    /* 吉尔伽美什 */
    write_int(buf, &offset, g->gilgamesh_skill_idx);
    write_int(buf, &offset, g->gilgamesh_target_idx);

    /* 鼠标位置（不序列化，跳过） */

    /* AI延迟 */
    write_int(buf, &offset, g->ai_delay);
    write_int(buf, &offset, g->ai_delay_active);

    /* 选目标状态 */
    write_int(buf, &offset, g->pending_hand_index);
    card_ptr_serialize(g->pending_card, buf, &offset);

    /* 弃牌阶段 */
    write_int(buf, &offset, g->discard_need_count);

    /* 响应选牌 */
    write_int(buf, &offset, g->response_pick_selected);

    /* 判定 */
    write_int(buf, &offset, g->judge_active);
    write_int(buf, &offset, g->judge_step);
    write_int(buf, &offset, g->judge_idx);
    card_ptr_serialize(g->judge_delay, buf, &offset);
    card_ptr_serialize(g->judge_card, buf, &offset);
    write_bytes(buf, &offset, g->judge_result, 128);
    write_int(buf, &offset, g->judge_delay_action);

    /* 朱雀羽扇 */
    write_int(buf, &offset, g->zhuque_active);
    write_int(buf, &offset, g->zhuque_source);
    write_int(buf, &offset, g->zhuque_target);
    card_ptr_serialize(g->zhuque_sha_card, buf, &offset);
    write_int(buf, &offset, g->zhuque_selected);

    /* 丈八蛇矛 */
    write_int(buf, &offset, g->zhangba_active);
    write_int(buf, &offset, g->zhangba_selected_count);
    for(int i = 0; i < 2; i++) write_int(buf, &offset, g->zhangba_selected[i]);
    card_ptr_serialize(g->zhangba_virtual_sha, buf, &offset);

    /* 八卦阵 */
    write_int(buf, &offset, g->bagua_active);
    write_int(buf, &offset, g->bagua_source);
    write_int(buf, &offset, g->bagua_attacker);
    card_ptr_serialize(g->bagua_trigger_card, buf, &offset);
    write_int(buf, &offset, g->bagua_selected);
    card_ptr_serialize(g->bagua_judge_card, buf, &offset);

    /* 圣骑士神圣护盾 */
    write_int(buf, &offset, g->paladin_choice_paladin_idx);
    write_int(buf, &offset, g->paladin_choice_turn_idx);

    /* 圣骑士破灭护盾 */
    write_bytes(buf, &offset, g->pomie_selected_card_name, 32);
    write_int(buf, &offset, g->pomie_mode);
}

void game_state_deserialize(GameState* g, unsigned char* buf)
{
    int offset = 0;

    /* 注意：不释放旧内存，直接覆盖（避免double free）
     * 虽然有少量内存泄漏，但在MCTS模拟中可接受
     */

    /* 玩家 */
    g->player_count = read_int(buf, &offset);
    for(int i = 0; i < MAX_PLAYERS; i++) {
        player_deserialize(&g->players[i], buf, &offset);
    }

    /* 牌堆、弃牌堆 */
    deck_deserialize(&g->deck, buf, &offset);
    deck_deserialize(&g->discard, buf, &offset);

    /* 基本状态 */
    g->current_player = read_int(buf, &offset);
    g->phase = (Phase)read_int(buf, &offset);
    g->game_over = read_int(buf, &offset);
    g->winner_id = read_int(buf, &offset);
    g->turn_count = read_int(buf, &offset);

    /* 角色选择 */
    g->player_hero_id = read_int(buf, &offset);
    g->ai_hero_id = read_int(buf, &offset);
    g->select_hover = read_int(buf, &offset);
    g->selected_hero_idx = read_int(buf, &offset);

    /* 响应状态 */
    g->resp_state = (RespState)read_int(buf, &offset);
    g->central_show_card = card_ptr_deserialize(buf, &offset);

    /* 展示牌 */
    g->show_card_center = card_ptr_deserialize(buf, &offset);
    read_bytes(buf, &offset, g->show_card_who, 32);
    g->show_card_timer = read_int(buf, &offset);

    for(int i = 0; i < 10; i++) g->show_cards_center[i] = card_ptr_deserialize(buf, &offset);
    g->show_cards_count = read_int(buf, &offset);
    g->show_cards_timer = read_int(buf, &offset);
    g->show_cards_total = read_int(buf, &offset);
    read_bytes(buf, &offset, g->show_cards_who, 32);

    read_bytes(buf, &offset, g->center_message, 256);
    g->center_message_timer = read_int(buf, &offset);

    g->long_press_skill_idx = read_int(buf, &offset);

    /* 决斗/响应 */
    g->duel_turn = read_int(buf, &offset);
    g->resp_source_player = read_int(buf, &offset);
    g->resp_target_player = read_int(buf, &offset);
    g->resp_trigger_card = card_ptr_deserialize(buf, &offset);
    g->resp_need_basic_after_wuxie = read_int(buf, &offset);
    g->resp_required_basic = read_int(buf, &offset);
    g->ai_play_finished = read_int(buf, &offset);

    /* 群体锦囊 */
    g->group_active = read_int(buf, &offset);
    g->group_phase = read_int(buf, &offset);
    g->group_current = read_int(buf, &offset);
    g->group_source = read_int(buf, &offset);
    g->group_trick_type = read_int(buf, &offset);
    g->group_trigger_card = card_ptr_deserialize(buf, &offset);
    g->group_wuxie_mask = read_int(buf, &offset);
    g->group_wuxie_counter_from = read_int(buf, &offset);
    for(int i = 0; i < 16; i++) g->group_wugu_pile[i] = card_ptr_deserialize(buf, &offset);
    g->group_wugu_count = read_int(buf, &offset);

    /* 伤害来源 */
    g->current_damage_source = read_int(buf, &offset);
    g->current_damage_source_player = read_int(buf, &offset);

    /* 火攻 */
    g->huogong_active = read_int(buf, &offset);
    g->huogong_source = read_int(buf, &offset);
    g->huogong_target = read_int(buf, &offset);
    g->huogong_show_card = card_ptr_deserialize(buf, &offset);
    g->huogong_need_suit = read_int(buf, &offset);
    g->huogong_picked_hand = read_int(buf, &offset);

    /* 贯石斧 */
    g->guanshi_active = read_int(buf, &offset);
    g->guanshi_source = read_int(buf, &offset);
    g->guanshi_target = read_int(buf, &offset);
    g->guanshi_damage = read_int(buf, &offset);
    g->guanshi_picking = read_int(buf, &offset);
    g->guanshi_picked_count = read_int(buf, &offset);
    for(int i = 0; i < 2; i++) g->guanshi_picked[i] = read_int(buf, &offset);

    /* 雨蝶飞舞选牌 */
    g->feiwuu_selected_count = read_int(buf, &offset);
    for(int i = 0; i < 4; i++) g->feiwuu_selected[i] = read_int(buf, &offset);

    /* 寒冰剑 */
    g->hanbing_active = read_int(buf, &offset);
    g->hanbing_source = read_int(buf, &offset);
    g->hanbing_target = read_int(buf, &offset);
    g->hanbing_damage = read_int(buf, &offset);
    g->hanbing_picking = read_int(buf, &offset);
    g->hanbing_picked_count = read_int(buf, &offset);
    for(int i = 0; i < 2; i++) g->hanbing_picked_type[i] = read_int(buf, &offset);
    for(int i = 0; i < 2; i++) g->hanbing_picked_index[i] = read_int(buf, &offset);

    /* 过河拆桥/顺手牵羊 */
    g->pick_enemy_target = read_int(buf, &offset);
    g->pick_enemy_action = read_int(buf, &offset);
    g->pick_enemy_card_type = read_int(buf, &offset);
    g->pick_enemy_card_index = read_int(buf, &offset);

    /* 圣骑士弃牌 */
    g->paladin_discard_option = read_int(buf, &offset);
    g->paladin_discard_selected = read_int(buf, &offset);

    /* 通用弃牌 */
    g->generic_discard_player = read_int(buf, &offset);
    g->generic_discard_need = read_int(buf, &offset);
    for(int i = 0; i < 10; i++) g->generic_discard_selected[i] = read_int(buf, &offset);
    g->generic_discard_selected_count = read_int(buf, &offset);
    g->generic_discard_source = read_int(buf, &offset);
    g->generic_discard_done = read_int(buf, &offset);

    /* 确认出牌 */
    g->confirm_play_hand_index = read_int(buf, &offset);
    g->confirm_play_target_index = read_int(buf, &offset);

    /* 铁索连环 */
    g->tiesuo_hand_index = read_int(buf, &offset);
    for(int i = 0; i < 2; i++) g->tiesuo_targets[i] = read_int(buf, &offset);
    g->tiesuo_target_count = read_int(buf, &offset);
    g->tiesuo_wuxie_index = read_int(buf, &offset);
    g->tiesuo_wuxie_mask = read_int(buf, &offset);

    /* 多目标锦囊无懈 */
    g->multi_wuxie_card = card_ptr_deserialize(buf, &offset);
    g->multi_wuxie_source = read_int(buf, &offset);
    for(int i = 0; i < MAX_PLAYERS; i++) g->multi_wuxie_targets[i] = read_int(buf, &offset);
    g->multi_wuxie_target_count = read_int(buf, &offset);
    g->multi_wuxie_current_target = read_int(buf, &offset);
    g->multi_wuxie_wuxie_mask = read_int(buf, &offset);
    for(int i = 0; i < MAX_WUXIE_STACK; i++) {
        g->multi_wuxie_stack[i].user_idx = read_int(buf, &offset);
        g->multi_wuxie_stack[i].target_idx = read_int(buf, &offset);
        g->multi_wuxie_stack[i].trick_target_idx = read_int(buf, &offset);
        g->multi_wuxie_stack[i].asker_idx = read_int(buf, &offset);
        g->multi_wuxie_stack[i].ask_count = read_int(buf, &offset);
    }
    g->multi_wuxie_stack_depth = read_int(buf, &offset);
    g->multi_wuxie_trick_type = read_int(buf, &offset);
    g->multi_wuxie_resolved = read_int(buf, &offset);

    /* 倒计时 */
    g->countdown.active = read_int(buf, &offset);
    g->countdown.remaining = read_float(buf, &offset);
    g->countdown.duration = read_float(buf, &offset);
    g->countdown.callback_type = read_int(buf, &offset);

    /* 群体锦囊指定目标 */
    g->group_target_hand_index = read_int(buf, &offset);
    for(int i = 0; i < MAX_PLAYERS; i++) g->group_target_order[i] = read_int(buf, &offset);
    g->group_target_count = read_int(buf, &offset);
    g->group_target_current = read_int(buf, &offset);
    g->group_target_confirmed = read_int(buf, &offset);

    /* 化形技能 */
    g->huaxing_current_suit = read_int(buf, &offset);
    g->huaxing_selected_hand = read_int(buf, &offset);
    g->huaxing_selected_trick = read_int(buf, &offset);
    for(int i = 0; i < 4; i++) g->huaxing_suit_order[i] = read_int(buf, &offset);
    g->huaxing_suit_count = read_int(buf, &offset);
    g->huaxing_suit_index = read_int(buf, &offset);
    g->huaxing_trick_type = read_int(buf, &offset);
    g->huaxing_used_card = card_ptr_deserialize(buf, &offset);
    g->huaxing_selecting_target = read_int(buf, &offset);
    g->huaxing_after_group = read_int(buf, &offset);
    g->huaxing_after_pick = read_int(buf, &offset);
    g->huaxing_after_duel = read_int(buf, &offset);
    g->huaxing_after_huogong = read_int(buf, &offset);

    /* 雨蝶飞舞拖拽 */
    for(int i = 0; i < 4; i++) g->feiwuu_drag_cards[i] = card_ptr_deserialize(buf, &offset);
    g->feiwuu_drag_count = read_int(buf, &offset);
    g->feiwuu_drag_index = read_int(buf, &offset);
    g->feiwuu_dragging = read_int(buf, &offset);
    g->feiwuu_drag_x = read_int(buf, &offset);
    g->feiwuu_drag_y = read_int(buf, &offset);
    for(int i = 0; i < 4; i++) g->feiwuu_placed_slots[i] = read_int(buf, &offset);

    /* 吉尔伽美什 */
    g->gilgamesh_skill_idx = read_int(buf, &offset);
    g->gilgamesh_target_idx = read_int(buf, &offset);

    /* AI延迟 */
    g->ai_delay = read_int(buf, &offset);
    g->ai_delay_active = read_int(buf, &offset);

    /* 选目标状态 */
    g->pending_hand_index = read_int(buf, &offset);
    g->pending_card = card_ptr_deserialize(buf, &offset);

    /* 弃牌阶段 */
    g->discard_need_count = read_int(buf, &offset);

    /* 响应选牌 */
    g->response_pick_selected = read_int(buf, &offset);

    /* 判定 */
    g->judge_active = read_int(buf, &offset);
    g->judge_step = read_int(buf, &offset);
    g->judge_idx = read_int(buf, &offset);
    g->judge_delay = card_ptr_deserialize(buf, &offset);
    g->judge_card = card_ptr_deserialize(buf, &offset);
    read_bytes(buf, &offset, g->judge_result, 128);
    g->judge_delay_action = read_int(buf, &offset);

    /* 朱雀羽扇 */
    g->zhuque_active = read_int(buf, &offset);
    g->zhuque_source = read_int(buf, &offset);
    g->zhuque_target = read_int(buf, &offset);
    g->zhuque_sha_card = card_ptr_deserialize(buf, &offset);
    g->zhuque_selected = read_int(buf, &offset);

    /* 丈八蛇矛 */
    g->zhangba_active = read_int(buf, &offset);
    g->zhangba_selected_count = read_int(buf, &offset);
    for(int i = 0; i < 2; i++) g->zhangba_selected[i] = read_int(buf, &offset);
    g->zhangba_virtual_sha = card_ptr_deserialize(buf, &offset);

    /* 八卦阵 */
    g->bagua_active = read_int(buf, &offset);
    g->bagua_source = read_int(buf, &offset);
    g->bagua_attacker = read_int(buf, &offset);
    g->bagua_trigger_card = card_ptr_deserialize(buf, &offset);
    g->bagua_selected = read_int(buf, &offset);
    g->bagua_judge_card = card_ptr_deserialize(buf, &offset);

    /* 圣骑士神圣护盾 */
    g->paladin_choice_paladin_idx = read_int(buf, &offset);
    g->paladin_choice_turn_idx = read_int(buf, &offset);

    /* 圣骑士破灭护盾 */
    read_bytes(buf, &offset, g->pomie_selected_card_name, 32);
    g->pomie_mode = read_int(buf, &offset);

    /* 验证状态一致性 */
    if(!game_state_validate(g)) {
        fprintf(stderr, "[SERIALIZE] 警告：反序列化后状态验证失败，可能导致崩溃\n");
    }
}
