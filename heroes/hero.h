#ifndef HERO_BASE_H
#define HERO_BASE_H

#include "../card.h"

/* ===== 前向声明 ===== */
typedef struct GameState GameState;
typedef struct Player    Player;

/* ===== 角色ID ===== */
typedef enum {
    HERO_FEIXIAO   = 0,
    HERO_ZHAOYUN   = 1,
    HERO_GILGAMESH = 2,
    HERO_LINYUXIA  = 3,
    HERO_PALADIN   = 4,
    HERO_YUDIE     = 5,
    HERO_LIUYING   = 6,
    HERO_JINGLIU   = 7,
} HeroId;

/* ===== 技能类型 ===== */
typedef enum {
    SKILL_LOCKED  = 0,   /* 锁定技：自动触发 */
    SKILL_PASSIVE = 1,   /* 被动技：触发前询问 */
    SKILL_ACTIVE  = 2,   /* 主动技：点击发动 */
} SkillType;

/* ===== 阶段 bitmask（用于技能 allowed_phases） ===== */
#define HERO_PHASE_PREPARE  (1 << 0)  /* 准备阶段 */
#define HERO_PHASE_DRAW     (1 << 1)  /* 摸牌阶段 */
#define HERO_PHASE_PLAY     (1 << 2)  /* 出牌阶段 */
#define HERO_PHASE_DISCARD  (1 << 3)  /* 弃牌阶段 */
#define HERO_PHASE_END      (1 << 4)  /* 结束阶段 */
#define HERO_PHASE_ALL      (HERO_PHASE_PREPARE | HERO_PHASE_DRAW | HERO_PHASE_PLAY | HERO_PHASE_DISCARD | HERO_PHASE_END)

/* ===== 技能结构体 ===== */
typedef struct {
    char      name[32];
    char      desc[256];
    SkillType type;
    int       allowed_phases;    /* 允许使用的阶段（bitmask） */
    int       max_uses;          /* 每回合最大使用次数（-1=无限） */
    int       used_count;        /* 本回合已使用次数（内计数） */
    int       active;            /* 是否正在结算中（点击后变暗） */
    int       enabled;           /* 技能是否生效：0=失效，1=生效 */
} Skill;

/* ===== 角色结构体 ===== */
typedef struct {
    HeroId id;
    char   name[32];
    int    max_hp;
    int    initial_hp;         /* 初始体力（0=等于max_hp） */
    int    max_shield;         /* 最大盾数（0=没有盾机制） */
    int    initial_shield;     /* 初始盾数（0=没有盾） */
    Skill  skills[4];
    int    skill_count;

    /* ===== 技能回调函数指针（NULL=没有该技能） ===== */
    int  (*draw_bonus)(const Player* p);                    /* 摸牌数+X */
    int  (*sha_bonus)(const Player* p);                     /* 出杀次数+X */
    int  (*damage_bonus)(const Player* p);                  /* 造成伤害+X */
    int  (*damage_reduce)(GameState* g, int victim_idx, int amount);  /* 受到伤害-X */
    void (*on_damage_reduced)(GameState* g, int victim_idx, int reduced);  /* 减免后触发 */
    void (*on_turn_start)(GameState* g, int player_idx);   /* 自己回合开始 */
    void (*on_turn_end)(GameState* g, int player_idx);     /* 自己回合结束 */
    void (*on_round_start)(GameState* g, int player_idx);  /* 每轮开始 */
    void (*on_any_turn_start)(GameState* g, int paladin_idx, int turn_player_idx);  /* 任何角色回合开始 */
    void (*on_card_used)(GameState* g, int player_idx, Card* card);  /* 使用牌时 */
    void (*on_becoming_target)(GameState* g, int target_idx, int source_idx, Card* card);  /* 成为目标时 */
    int  (*hand_limit_mod)(GameState* g, int player_idx);  /* 手牌上限增加 */
    void (*on_dying)(GameState* g, int player_idx);        /* 进入濒死时 */
    int  (*can_use_skill)(GameState* g, int player_idx, int skill_idx);  /* 能否发动主动技能 */
    void (*use_skill)(GameState* g, int player_idx, int skill_idx);      /* 发动主动技能 */
    int  (*ai_use_skill)(GameState* g, int player_idx);                  /* AI自动使用技能，返回1表示用了 */
} Hero;

/* ===== 全局角色表 ===== */
extern Hero hero_table[8];

/* ===== 公共接口 ===== */
void  hero_init_table(void);
Hero* hero_get(HeroId id);
void  hero_reset_skills(Player* p);

/* ===== 通用技能系统（阶段限制 + 使用次数） ===== */
/* 检查技能是否可以使用（阶段符合 + 还有使用次数） */
int  hero_skill_can_use(GameState* g, int player_idx, int skill_idx);
/* 使用技能（增加使用次数，设置active=1），返回1表示成功 */
int  hero_skill_use(GameState* g, int player_idx, int skill_idx);
/* 技能结算完成（设置active=0） */
void hero_skill_finish(GameState* g, int player_idx, int skill_idx);
/* 获取当前阶段对应的 bitmask */
int  hero_get_phase_mask(GameState* g);
/* 重置所有技能的使用次数（回合开始时调用） */
void  hero_reset_skill_uses(Player* p);
/* 设置技能生效/失效：enabled=1生效，0=失效 */
void hero_skill_set_enabled(Player* p, int skill_idx, int enabled);
/* 检查技能是否生效 */
int  hero_skill_is_enabled(Player* p, int skill_idx);

/*
 * 统一AI响应选牌接口：
 *   need_type  : 需要的基本牌类型(BASIC_SHA / BASIC_SHAN)
 *   used_skill : 输出，1=用了技能转换(用于触发后续效果)
 *   返回手牌索引，-1=没有
 * 内部根据 hero_id 自动路由到对应角色的实现
 */
int hero_ai_pick_response(GameState* g, int player_idx,
                           BasicType need_type, int* used_skill);

/* 统一AI出牌选牌接口（框架，默认返回-1走通用AI） */
int hero_ai_pick_play(GameState* g, int player_idx);

#endif /* HERO_BASE_H */
