#include <stdio.h>
#include <string.h>
#include "feixiao.h"
#include "../../game.h"
#include "../../player.h"

/* ===== 注册角色信息 ===== */
void feixiao_register(Hero* h)
{
    if (!h) return;
    memset(h, 0, sizeof(Hero));

    h->id = HERO_FEIXIAO;
    strncpy(h->name, "feixiao", 31);
    h->max_hp = 4;
    h->skill_count = 1;

    /* 一技能：斧钺（锁定技） */
    strncpy(h->skills[0].name, "斧钺", 31);
    strncpy(h->skills[0].desc,
            "锁定技，摸牌阶段摸牌数+X，出杀次数+X，造成伤害+X，X为你已损体力值。",
            255);
    h->skills[0].type = SKILL_LOCKED;
    h->skills[0].allowed_phases = 0;
    h->skills[0].max_uses = -1;
    h->skills[0].used_count = 0;
    h->skills[0].active = 0;

    /* 注册回调函数指针 */
    h->draw_bonus   = feixiao_draw_bonus;
    h->sha_bonus    = feixiao_sha_bonus;
    h->damage_bonus = feixiao_damage_bonus;
}

/* ===== 斧钺：X = 已损体力值 ===== */
int feixiao_get_x(const Player* p)
{
    if (!p) return 0;
    int x = p->max_hp - p->hp;
    return x < 0 ? 0 : x;
}

int feixiao_draw_bonus(const Player* p)
{
    if (!p || p->hero_id != HERO_FEIXIAO) return 0;
    return feixiao_get_x(p);
}

int feixiao_sha_bonus(const Player* p)
{
    if (!p || p->hero_id != HERO_FEIXIAO) return 0;
    return feixiao_get_x(p);
}

int feixiao_damage_bonus(const Player* p)
{
    if (!p || p->hero_id != HERO_FEIXIAO) return 0;
    return feixiao_get_x(p);
}
