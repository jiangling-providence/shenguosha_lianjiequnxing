#ifndef PLAYER_H
#define PLAYER_H


#include "card.h"
#include "heroes/hero.h"


#define MAX_HAND_CARDS  30   /* 手牌上限 */
#define MAX_JUDGE_CARDS 3    /* 判定区上限（每种延时锦囊各一张，最多3张） */
#define MAX_HIDDEN_HP   647  /* 里体力上限 */
#define MAX_SHIELD      5    /* 盾量上限 */


/* ===== 装备区 ===== */
typedef struct {
    Card* weapon;        /* 武器 */
    Card* armor;         /* 防具 */
    Card* horse_atk;     /* 进攻马 (-1马) */
    Card* horse_def;     /* 防御马 (+1马) */
    int   feiwuu_placed[4];  /* 飞舞放置标记：1=该槽是飞舞放置的，不生效；0=正常装备 */
} EquipArea;


/* ===== 判定区 ===== */
typedef struct {
    Card* cards[MAX_JUDGE_CARDS];
    int count;
} JudgeArea;    /*这里没有做上限判定！*/


/* ===== 玩家 ===== */
typedef struct Player {
    int id;
    char name[32];

    /* 表体力（正常体力，受到正常伤害时减少，≤0进入濒死） */
    int hp;
    int max_hp;

    /* 里体力（隐藏体力，受到"视为伤害"时减少，不影响濒死，可触发卖血技能） */
    int hidden_hp;
    int max_hidden_hp;

    int alive;            /* 是否存活 */
    int is_ai;            /* 是否AI玩家 */

    /* 四个区域 */
    Card* hand[MAX_HAND_CARDS];   /* 手牌区 */
    int hand_count;
    EquipArea equip;               /* 装备区 */
    JudgeArea judge;               /* 判定区 */

    /* 回合状态 */
    int sha_used;         /* 本回合已出杀次数 */
    int jiu_used;         /* 本回合是否用过酒增伤 (0/1) */
    int skip_draw;        /* 是否跳过摸牌阶段 */
    int skip_play;        /* 是否跳过出牌阶段 */
    int chained;          /* 是否处于连环状态 */
    int flipped;          /* 是否处于翻面状态 */

    /* 累计受伤次数（每次真正扣血+1，藤甲免伤时不增加） */
    int damage_taken_count;

    /* ===== 武将系统 ===== */
    HeroId hero_id;           /* 武将ID */
    Hero* hero;               /* 指向武将信息（含技能回调函数） */
    int shield;               /* 当前盾数（0=没有盾） */
    int skill_used[4];        /* 主动技能本回合已使用次数 */
    int suits_used[4];        /* 本回合使用过的花色（0=黑桃,1=红桃,2=梅花,3=方块） */
    int immune_suit;          /* 免疫的花色（-1=无免疫） */
    int yuzhan_active;        /* 玉盏是否已发动（0=未，1=已发动，本回合用牌可增减目标） */

    /* ===== 雨蝶专用数据 ===== */
    struct {
        int break_suit;           /* 破茧展示的花色（-1=无） */
        int feiwuu_count;          /* 飞舞发动次数 */
        int feiwuu_cards;          /* 飞舞累计获得牌数 */
        char card_names[20][32];   /* 使用过的牌名（统计10种） */
        int card_name_count;       /* 使用过的不同牌名数 */
        int chengdie;              /* 是否已进化（0=未，1=已） */
        int last_suit;             /* 上一张使用牌的花色（成形用） */
        int chengxing_count;       /* 成形摸牌次数（每回合限5） */
        int first_card_played;     /* 本回合是否已用第一张牌 */
        int huaxing_used_suits;    /* 化形已使用花色（bitmask） */
        int huaxing_target;        /* 化形②指定目标（-1=无） */
        int huaxing_free_suits;    /* 化形②未选择花色（bitmask） */
        int huadie_active;         /* 当前是否是化蝶结算（1=化蝶，0=飞舞） */
        /* 化形①：本回合使用过的锦囊牌名 */
        char huaxing_used_tricks[10][32]; /* 本回合使用过的锦囊牌名 */
        int huaxing_used_trick_count;     /* 使用过的锦囊牌名数 */
        char huaxing_played_names[10][32];/* 化形①已使用的牌名（避免重复） */
        int huaxing_played_name_count;    /* 已使用的牌名数 */
        /* 化形①：记录本回合使用过的牌的详细信息（牌名、花色、点数） */
        char huaxing_record_names[20][32];
        int  huaxing_record_suits[20];
        int  huaxing_record_ranks[20];
        int  huaxing_record_count;
        /* 化形②：每角色每回合使用次数 */
        int huaxing_response_used[2];     /* 每个角色本回合化形②使用次数 */
    } yudie;

    /* ===== 流萤专用数据 ===== */
    struct {
        int bengfa_element;        /* 迸发：下次伤害属性（0=无,1=火,2=雷） */
        int guozai_sha_active;     /* 过载：当前是否有杀正在结算 */
        int guozai_sha_target;     /* 过载：杀的目标下标 */
        int chaoxing_used_play;    /* 超新星：出牌阶段是否已使用 */
        int chaoxing_used_end;     /* 超新星：结束阶段是否已使用 */
    } liuying;

    /* ===== 镜流专用数据 ===== */
    struct {
        int hong_marks;             /* "薨"标记数 */
        int transformation;         /* 形态：0=普通,1=登仙,2=入魔 */
        int wuxia_suit_count;       /* 无罅飞光：展示的花色数 */
        int wuxia_used;             /* 无罅飞光：本回合是否已使用 */
        int sha_extra_target;       /* 无罅飞光花色1：杀额外目标数 */
        int sha_damage_plus;        /* 无罅飞光花色2：杀伤害+1 */
        int allow_zone_card;        /* 无罅飞光花色3：可弃置目标区域牌 */
        int next_sha_unblockable;   /* 无罅飞光花色4：下一张杀不可响应 */
        int gujing_play_opt1;       /* 古镜照神：出牌阶段选项1是否已使用 */
        int gujing_play_opt2;       /* 古镜照神：出牌阶段选项2是否已使用 */
        int gujing_resp_used;       /* 古镜照神：回合外响应是否已使用 */
        int gujing_resp_round_count; /* 古镜照神：整轮响应次数（最多3次） */
        int dying_judged;           /* 魔阴：是否已进行过死亡判定 */
    } jingliu;

    /* ===== 赵云专用数据 ===== */
    int longdan_active;            /* 龙胆模式是否激活（0=未激活，1=激活） */
} Player;


/* 伤害类型 */
typedef enum
{
    DMG_NORMAL = 0,     // 普通伤害（扣表体力）
    DMG_FIRE,            // 火焰伤害（扣表体力，触发铁索传导）
    DMG_THUNDER,         // 雷电伤害（扣表体力，触发铁索传导）
    DMG_VIRTUAL          // 视为伤害（只扣里体力，不扣表体力，不濒死不传导，可触发卖血技能）
}DamageType;


/* 伤害来源（用于藤甲等防具判断） */
typedef enum
{
    DMG_SRC_OTHER = 0,   // 其他来源
    DMG_SRC_SHA,          // 杀
    DMG_SRC_NANMAN,       // 南蛮入侵
    DMG_SRC_WANJIAN,      // 万箭齐发
    DMG_SRC_JUEDOU,       // 决斗
    DMG_SRC_HUOGONG       // 火攻
} DamageSource;


/* ===== 函数声明 ===== */

/* 初始化玩家 */
void player_init(Player* p, int id, const char* name, int max_hp, HeroId hero_id);

/* 销毁玩家（释放所有卡牌内存） */
void player_destroy(Player* p);

/* 摸一张牌到手牌 */
void player_draw_card(Player* p, Card* c);

/* 从手牌移除一张牌（按下标），返回该牌指针 */
Card* player_remove_hand(Player* p, int index);

/* 从手牌找一张牌（按ID），返回下标，找不到返回-1 */
int player_find_hand_by_id(Player* p, int card_id);

/* 装备武器/防具/马，旧装备返回（调用者处理） */
Card* player_equip(Player* p, Card* card);

/* 判定区添加一张延时锦囊 */
void player_add_judge(Player* p, Card* card);

/* 判定区移除一张牌（按下标） */
Card* player_remove_judge(Player* p, int index);

/* 受到伤害（正常伤害扣表体力，视为伤害扣里体力） */
void player_take_damage(Player* p, int amount, DamageType dmg_type);

/* 回复表体力 */
void player_recover(Player* p, int amount);

/* 获取手牌上限 */
int player_hand_limit(const Player* p);

/* 实际手牌上限（包含武将技能修正，如雨蝶破茧同花色牌不计入） */
int player_effective_hand_limit(GameState* g, int player_idx);

/* 获取攻击距离 */
int player_attack_range(const Player* p);

/* 获取有效装备（忽略飞舞放置的） */
Card* player_get_weapon(const Player* p);   /* 有效武器 */
Card* player_get_armor(const Player* p);    /* 有效防具 */
Card* player_get_horse_atk(const Player* p); /* 有效进攻马 */
Card* player_get_horse_def(const Player* p); /* 有效防御马 */

/* 安全获取装备类型，无有效装备返回 -1 */
int player_weapon_type(const Player* p);
int player_armor_type(const Player* p);


#endif /* PLAYER_H */
