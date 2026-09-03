#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hero.h"
#include "../game.h"
#include "../player.h"
#include "feixiao/feixiao.h"
#include "zhaoyun/zhaoyun.h"
#include "gilgamesh/gilgamesh.h"
#include "linyuxia/linyuxia.h"
#include "paladin/paladin.h"
#include "yudie/yudie.h"
#include "liuying/liuying.h"
#include "jingliu/jingliu.h"

/* ===== 全局角色表 ===== */
Hero hero_table[8];
static int hero_table_inited = 0;

/* ===== 初始化：注册所有角色（只执行一次，避免game_restart时重复memset） ===== */
void hero_init_table(void)
{
    if(hero_table_inited) return;
    hero_table_inited = 1;
    memset(hero_table, 0, sizeof(hero_table));
    feixiao_register(&hero_table[HERO_FEIXIAO]);
    zhaoyun_register(&hero_table[HERO_ZHAOYUN]);
    gilgamesh_register(&hero_table[HERO_GILGAMESH]);
    linyuxia_register(&hero_table[HERO_LINYUXIA]);
    paladin_register(&hero_table[HERO_PALADIN]);
    yudie_register(&hero_table[HERO_YUDIE]);
    liuying_register(&hero_table[HERO_LIUYING]);
    jingliu_register(&hero_table[HERO_JINGLIU]);

    /* 统一初始化所有技能的 enabled=1（默认生效） */
    for(int i = 0; i < 8; i++)
    {
        for(int s = 0; s < hero_table[i].skill_count; s++)
        {
            hero_table[i].skills[s].enabled = 1;
        }
    }

    /* ===== 调试：打印所有角色的ID和技能名 ===== */
    fprintf(stderr, "===== hero_table debug =====\n");
    for(int i = 0; i < 8; i++)
    {
        fprintf(stderr, "hero_table[%d]: id=%d name=%s skill_count=%d\n",
                i, hero_table[i].id, hero_table[i].name, hero_table[i].skill_count);
        for(int s = 0; s < hero_table[i].skill_count; s++)
        {
            fprintf(stderr, "  skills[%d]: %s (type=%d)\n",
                    s, hero_table[i].skills[s].name, hero_table[i].skills[s].type);
        }
    }
    fprintf(stderr, "============================\n");
}

/* ===== 获取角色 ===== */
Hero* hero_get(HeroId id)
{
    if (id < 0 || id >= 8) return NULL;
    return &hero_table[id];
}

/* ===== 重置技能使用次数（回合开始时调用） ===== */
void hero_reset_skills(Player* p)
{
    if (!p) return;
    Hero* h = hero_get(p->hero_id);
    if (!h) return;
    for (int i = 0; i < h->skill_count; i++) {
        h->skills[i].used_count = 0;
        h->skills[i].active = 0;
    }
}


/* ================================================================
 * 通用技能系统
 * ================================================================ */

/* 获取当前阶段对应的 bitmask */
int hero_get_phase_mask(GameState* g)
{
    if(!g) return 0;
    switch(g->phase)
    {
        case PHASE_PREPARE: return HERO_PHASE_PREPARE;
        case PHASE_DRAW:    return HERO_PHASE_DRAW;
        case PHASE_PLAY:    return HERO_PHASE_PLAY;
        case PHASE_DISCARD: return HERO_PHASE_DISCARD;
        case PHASE_END:     return HERO_PHASE_END;
        default:             return 0;
    }
}

/* 检查技能是否可以使用（阶段符合 + 还有使用次数 + 不是正在结算中） */
int hero_skill_can_use(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return 0;
    Player* p = &g->players[player_idx];
    if(!p->hero) return 0;
    if(skill_idx < 0 || skill_idx >= p->hero->skill_count) return 0;
    Skill* s = &p->hero->skills[skill_idx];

    /* 技能是否生效 */
    if(!s->enabled) return 0;

    /* 只有主动技可以点击使用 */
    if(s->type != SKILL_ACTIVE) return 0;

    /* 正在结算中，不能再次点击 */
    if(s->active) return 0;

    /* 阶段检查 */
    /* 响应状态下：跳过阶段检查（响应本身就是特殊时机，允许全阶段技能使用） */
    if(!(g->resp_state == RESPONSE_NEED_BASIC && g->resp_target_player == player_idx))
    {
        int phase_mask = hero_get_phase_mask(g);
        if(!(s->allowed_phases & phase_mask)) return 0;
    }

    /* 使用次数检查 */
    if(s->max_uses >= 0 && s->used_count >= s->max_uses) return 0;

    return 1;
}

/* 使用技能（增加使用次数，设置active=1） */
int hero_skill_use(GameState* g, int player_idx, int skill_idx)
{
    if(!hero_skill_can_use(g, player_idx, skill_idx)) return 0;
    Player* p = &g->players[player_idx];
    Skill* s = &p->hero->skills[skill_idx];

    s->used_count++;
    s->active = 1;
    return 1;
}

/* 技能结算完成（设置active=0） */
void hero_skill_finish(GameState* g, int player_idx, int skill_idx)
{
    if(!g || player_idx < 0 || player_idx >= g->player_count) return;
    Player* p = &g->players[player_idx];
    if(!p->hero) return;
    if(skill_idx < 0 || skill_idx >= p->hero->skill_count) return;
    p->hero->skills[skill_idx].active = 0;
}

/* 重置所有技能的使用次数 */
void hero_reset_skill_uses(Player* p)
{
    hero_reset_skills(p);
}

/* 设置技能生效/失效：enabled=1生效，0=失效 */
void hero_skill_set_enabled(Player* p, int skill_idx, int enabled)
{
    if(!p || !p->hero) return;
    if(skill_idx < 0 || skill_idx >= p->hero->skill_count) return;
    p->hero->skills[skill_idx].enabled = enabled ? 1 : 0;
}

/* 检查技能是否生效 */
int hero_skill_is_enabled(Player* p, int skill_idx)
{
    if(!p || !p->hero) return 0;
    if(skill_idx < 0 || skill_idx >= p->hero->skill_count) return 0;
    return p->hero->skills[skill_idx].enabled;
}

/* ================================================================
 * 统一AI响应选牌接口
 * 先找原生牌，找不到再按角色路由到技能转换
 * ================================================================ */
int hero_ai_pick_response(GameState* g, int player_idx,
                           BasicType need_type, int* used_skill)
{
    if (used_skill) *used_skill = 0;
    if (!g || player_idx < 0 || player_idx >= g->player_count) return -1;
    Player* p = &g->players[player_idx];

    /* 第一步：找原生牌 */
    for (int i = 0; i < p->hand_count; i++) {
        if (p->hand[i] && p->hand[i]->type == CARD_BASIC &&
            p->hand[i]->sub.basic.basic_type == need_type) {
            return i;
        }
    }

    /* 第二步：按角色路由到技能转换 */
    if (p->hero_id == HERO_ZHAOYUN) {
        return zhaoyun_ai_pick_response(g, player_idx, need_type, used_skill);
    }
    /* === 新角色在这里加 else if 分支即可，game.c 不用动 === */
    /* else if (p->hero_id == HERO_XXX) return xxx_ai_pick_response(...); */

    return -1;
}

/* ===== 统一AI出牌选牌（框架，默认走通用AI） ===== */
int hero_ai_pick_play(GameState* g, int player_idx)
{
    (void)g;
    (void)player_idx;
    return -1;  /* -1 表示走 game.c 的通用出牌逻辑 */
}
