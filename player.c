#include <string.h>
#include <stdlib.h>
#include <string.h>
#include "player.h"
#include "heroes/hero.h"


void player_init(Player* p, int id, const char* name, int max_hp, HeroId hero_id)
{
    memset(p, 0, sizeof(Player));
    p->id = id;
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->hero_id = hero_id;

    /* 设置角色指针（关键！技能系统依赖这个指针） */
    p->hero = hero_get(hero_id);

    /* 初始化盾量 */
    if(p->hero && p->hero->initial_shield > 0)
        p->shield = p->hero->initial_shield;
    else
        p->shield = 0;

    /* 表体力 */
    p->max_hp = max_hp;
    if(p->hero && p->hero->initial_hp > 0)
        p->hp = p->hero->initial_hp;
    else
        p->hp = max_hp;

    /* 里体力：初始上限647，初始值满 */
    p->max_hidden_hp = MAX_HIDDEN_HP;
    p->hidden_hp = MAX_HIDDEN_HP;

    p->alive = 1;

    /* 初始化特殊字段：-1表示无免疫/无展示 */
    p->immune_suit = -1;  /* 玉盏花色免疫：-1=无免疫 */
}


void player_destroy(Player* p)
{
    if (!p) return;
    /* 手牌 */
    for (int i = 0; i < p->hand_count; i++) {
        free(p->hand[i]);
    }
    /* 装备 */
    if (p->equip.weapon) free(p->equip.weapon);
    if (p->equip.armor) free(p->equip.armor);
    if (p->equip.horse_atk) free(p->equip.horse_atk);
    if (p->equip.horse_def) free(p->equip.horse_def);
    /* 判定区 */
    for (int i = 0; i < p->judge.count; i++) {
        free(p->judge.cards[i]);
    }
    memset(p, 0, sizeof(Player));
}


void player_draw_card(Player* p, Card* c)
{
    if (!p || !c) return;
    if (p->hand_count >= MAX_HAND_CARDS) return;
    p->hand[p->hand_count++] = c;
}


Card* player_remove_hand(Player* p, int index)
{
    if (!p || index < 0 || index >= p->hand_count) return NULL;
    Card* c = p->hand[index];
    for (int i = index; i < p->hand_count - 1; i++) {
        p->hand[i] = p->hand[i + 1];
    }
    p->hand_count--;
    return c;
}


int player_find_hand_by_id(Player* p, int card_id)
{
    if (!p) return -1;
    for (int i = 0; i < p->hand_count; i++) {
        if (p->hand[i] && p->hand[i]->id == card_id) return i;
    }
    return -1;
}


Card* player_equip(Player* p, Card* card)
{
    if (!p || !card || card->type != CARD_EQUIP) return NULL;
    Card* old = NULL;
    switch (card->sub.equip.equip_type) {
    case EQUIP_WEAPON:
        old = p->equip.weapon;
        p->equip.weapon = card;
        p->equip.feiwuu_placed[0] = 0;  /* 正常装备清除飞舞标记 */
        break;
    case EQUIP_ARMOR:
        old = p->equip.armor;
        p->equip.armor = card;
        p->equip.feiwuu_placed[1] = 0;  /* 正常装备清除飞舞标记 */
        break;
    case EQUIP_HORSE_ATK:
        old = p->equip.horse_atk;
        p->equip.horse_atk = card;
        p->equip.feiwuu_placed[2] = 0;  /* 正常装备清除飞舞标记 */
        break;
    case EQUIP_HORSE_DEF:
        old = p->equip.horse_def;
        p->equip.horse_def = card;
        p->equip.feiwuu_placed[3] = 0;  /* 正常装备清除飞舞标记 */
        break;
    }
    return old;
}


void player_add_judge(Player* p, Card* card)
{
    if (!p || !card) return;
    if (p->judge.count >= MAX_JUDGE_CARDS) return;
    p->judge.cards[p->judge.count++] = card;
}


Card* player_remove_judge(Player* p, int index)
{
    if (!p || index < 0 || index >= p->judge.count) return NULL;
    Card* c = p->judge.cards[index];
    for (int i = index; i < p->judge.count - 1; i++) {
        p->judge.cards[i] = p->judge.cards[i + 1];
    }
    p->judge.count--;
    return c;
}


/* 受到伤害：根据伤害类型扣表体力或里体力 */
void player_take_damage(Player* p, int amount, DamageType dmg_type)
{
    if (!p || !p->alive || amount <= 0) return;

    if (dmg_type == DMG_VIRTUAL) {
        /* 视为伤害：只扣里体力，不扣表体力 */
        p->hidden_hp -= amount;
        if (p->hidden_hp < 0) p->hidden_hp = 0;
    } else {
        /* 正常伤害：扣表体力（允许负数，原版规则） */
        p->hp -= amount;
    }
}


void player_recover(Player* p, int amount)
{
    if (!p || !p->alive) return;
    p->hp += amount;
    if (p->hp > p->max_hp) p->hp = p->max_hp;
}


int player_hand_limit(const Player* p)
{
    if (!p) return 0;
    return p->hp > 0 ? p->hp : 0;
}


int player_attack_range(const Player* p)
{
    if (!p) return 1;
    /* 飞舞放置的武器不生效 */
    if (p->equip.weapon &&
        !p->equip.feiwuu_placed[0] &&
        p->equip.weapon->type == CARD_EQUIP &&
        p->equip.weapon->sub.equip.equip_type == EQUIP_WEAPON) {
        return p->equip.weapon->sub.equip.detail.weapon.range;
    }
    return 1;
}

/* 获取有效武器（忽略飞舞放置的，且必须是装备牌） */
Card* player_get_weapon(const Player* p)
{
    if(!p) return NULL;
    if(p->equip.weapon && !p->equip.feiwuu_placed[0] &&
       p->equip.weapon->type == CARD_EQUIP &&
       p->equip.weapon->sub.equip.equip_type == EQUIP_WEAPON)
        return p->equip.weapon;
    return NULL;
}

/* 获取有效防具（忽略飞舞放置的，且必须是装备牌） */
Card* player_get_armor(const Player* p)
{
    if(!p) return NULL;
    if(p->equip.armor && !p->equip.feiwuu_placed[1] &&
       p->equip.armor->type == CARD_EQUIP &&
       p->equip.armor->sub.equip.equip_type == EQUIP_ARMOR)
        return p->equip.armor;
    return NULL;
}

/* 获取有效进攻马（忽略飞舞放置的，且必须是装备牌） */
Card* player_get_horse_atk(const Player* p)
{
    if(!p) return NULL;
    if(p->equip.horse_atk && !p->equip.feiwuu_placed[2] &&
       p->equip.horse_atk->type == CARD_EQUIP &&
       p->equip.horse_atk->sub.equip.equip_type == EQUIP_HORSE_ATK)
        return p->equip.horse_atk;
    return NULL;
}

/* 获取有效防御马（忽略飞舞放置的，且必须是装备牌） */
Card* player_get_horse_def(const Player* p)
{
    if(!p) return NULL;
    if(p->equip.horse_def && !p->equip.feiwuu_placed[3] &&
       p->equip.horse_def->type == CARD_EQUIP &&
       p->equip.horse_def->sub.equip.equip_type == EQUIP_HORSE_DEF)
        return p->equip.horse_def;
    return NULL;
}

/* 安全获取武器类型，无有效武器返回 -1 */
int player_weapon_type(const Player* p)
{
    Card* wp = player_get_weapon(p);
    if(!wp) return -1;
    return wp->sub.equip.detail.weapon.weapon_type;
}

/* 安全获取防具类型，无有效防具返回 -1 */
int player_armor_type(const Player* p)
{
    Card* ap = player_get_armor(p);
    if(!ap) return -1;
    return ap->sub.equip.detail.armor.armor_type;
}
