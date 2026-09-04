#ifndef FEIXIAO_H
#define FEIXIAO_H

#include "../hero.h"

/* 注册角色信息到 Hero 结构体 */
void feixiao_register(Hero* h);

/* 斧钺：X = 已损体力值 */
int feixiao_get_x(const Player* p);

/* 摸牌阶段摸牌数 +X */
int feixiao_draw_bonus(const Player* p);

/* 出杀次数 +X */
int feixiao_sha_bonus(const Player* p);

/* 造成伤害 +X */
int feixiao_damage_bonus(const Player* p);

#endif /* FEIXIAO_H */
