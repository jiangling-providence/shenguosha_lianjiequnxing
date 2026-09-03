#ifndef CARD_H
#define CARD_H


/* ===== 花色 ===== */
typedef enum {
    SUIT_SPADE = 0,   /* 黑桃 */
    SUIT_HEART,        /* 红桃 */
    SUIT_CLUB,         /* 梅花 */
    SUIT_DIAMOND,      /* 方片 */
    SUIT_NONE = -1     /* 无色 */
} Suit;


/* ===== 颜色 ===== */
typedef enum {
    COLOR_NONE = 0,    /* 无色 */
    COLOR_RED,         /* 红色 */
    COLOR_BLACK        /* 黑色 */
} CardColor;


/* ===== 牌的性质（真牌/虚拟牌/转化牌） ===== */
typedef enum {
    CARD_NATURE_REAL = 0,      /* 真牌：牌堆里的牌 */
    CARD_NATURE_VIRTUAL,        /* 虚拟牌：八卦阵判定出的闪等 */
    CARD_NATURE_CONVERTED       /* 转化牌：丈八蛇矛打出的杀等 */
} CardNature;


/* ===== 点数 ===== */
typedef enum {
    RANK_A = 1,
    RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7,
    RANK_8, RANK_9, RANK_10,
    RANK_J, RANK_Q, RANK_K
} Rank;


/* ===== 牌大类 ===== */
typedef enum {
    CARD_BASIC = 0,   /* 基本牌 */
    CARD_TRICK,        /* 普通锦囊牌 */
    CARD_DELAYED,      /* 延时锦囊牌 */
    CARD_EQUIP         /* 装备牌 */
} CardType;


/* ===== 基本牌子类型 ===== */
typedef enum {
    BASIC_SHA = 0,    /* 杀 */
    BASIC_SHAN,        /* 闪 */
    BASIC_TAO,         /* 桃 */
    BASIC_JIU          /* 酒 */
} BasicType;


/* ===== 杀的属性 ===== */
typedef enum {
    SHA_NORMAL = 0,   /* 普通杀 */
    SHA_THUNDER,       /* 雷杀 */
    SHA_FIRE           /* 火杀 */
} ShaElement;


/* ===== 锦囊牌子类型 ===== */
typedef enum {
    TRICK_JUEDOU = 0,         /* 决斗 */
    TRICK_WANJIAN,             /* 万箭齐发 */
    TRICK_NANMAN,              /* 南蛮入侵 */
    TRICK_TAOYUAN,             /* 桃园结义 */
    TRICK_WUGU,                /* 五谷丰登 */
    TRICK_SHUNSHOU,            /* 顺手牵羊 */
    TRICK_WUZHONG,             /* 无中生有 */
    TRICK_GUOHE,               /* 过河拆桥 */
    TRICK_WUXIE,               /* 无懈可击 */
    TRICK_HUOGONG,             /* 火攻 */
    TRICK_TIESUO               /* 铁锁连环 */
} TrickType;


/* ===== 延时锦囊子类型 ===== */
typedef enum {
    DELAYED_LEBU = 0,          /* 乐不思蜀 */
    DELAYED_BINGLIANG,         /* 兵粮寸断 */
    DELAYED_SHANDIAN           /* 闪电 */
} DelayedType;


/* ===== 装备牌子类型 ===== */
typedef enum {
    EQUIP_WEAPON = 0,          /* 武器 */
    EQUIP_ARMOR,                /* 防具 */
    EQUIP_HORSE_ATK,            /* 进攻马 (-1马) */
    EQUIP_HORSE_DEF             /* 防御马 (+1马) */
} EquipType;


/* ===== 武器种类 ===== */
typedef enum {
    WEAPON_CIXIONG = 0,        /* 雌雄双股剑 */
    WEAPON_ZHUQUE,              /* 朱雀羽扇 */
    WEAPON_ZHUGELIANNU,         /* 诸葛连弩 */
    WEAPON_GUANSHI,             /* 贯石斧 */
    WEAPON_FANGTIAN,            /* 方天画戟 */
    WEAPON_QINGGANG,            /* 青缸剑 */
    WEAPON_HANBING,             /* 寒冰剑 */
    WEAPON_QINGLONG,            /* 青龙偃月刀 */
    WEAPON_ZHANGBA,             /* 丈八蛇矛 */
    WEAPON_GUDING,              /* 古锭刀 */
    WEAPON_QILIN                 /* 麒麟弓 */
} WeaponType;


/* ===== 防具种类 ===== */
typedef enum {
    ARMOR_BAGUA = 0,            /* 八卦阵 */
    ARMOR_TENGJIA,              /* 藤甲 */
    ARMOR_BAIYIN,               /* 白银狮子 */
    ARMOR_RENWANG                /* 仁王盾 */
} ArmorType;


/* ===== 卡牌结构体 ===== */
typedef struct Card {
    int id;                      /* 全局唯一ID */
    CardType type;               /* 牌大类 */
    Suit suit;                   /* 花色 */
    CardColor color;             /* 颜色 */
    CardNature card_nature;      /* 牌的性质：真牌/虚拟牌/转化牌 */
    Rank rank;                   /* 点数 */

    /* ===== 有效标记 =====
     * 1 = 这张牌还可以生效（默认值）
     * 0 = 这张牌已经被无效，跳过后续所有效果结算
     * 牌进入弃牌堆或牌堆时会自动重置为 1
     */
    int is_valid;

    union {
        struct {
            BasicType basic_type;
            ShaElement sha_element;  /* 仅杀有效 */
        } basic;
        struct {
            TrickType trick_type;
        } trick;
        struct {
            DelayedType delayed_type;
        } delayed;
        struct {
            EquipType equip_type;
            union {
                struct {
                    WeaponType weapon_type;
                    int range;        /* 攻击距离 */
                } weapon;
                struct {
                    ArmorType armor_type;
                } armor;
            } detail;
        } equip;
    } sub;
} Card;


/* ===== 牌堆 ===== */
typedef struct {
    Card** cards;
    int count;
    int capacity;
    int top;  /* 下一张要摸的牌的下标 */
} Deck;


/* ===== 函数声明 ===== */

/* 初始化标准牌堆（160张左右） */
void deck_init_standard(Deck* deck);

/* 销毁牌堆 */
void deck_destroy(Deck* deck);

/* 洗牌 */
void deck_shuffle(Deck* deck);

/* 摸一张牌，返回指针；牌堆空返回NULL */
Card* deck_draw(Deck* deck);

/* 弃牌堆加一张牌（进入弃牌堆时 is_valid 自动重置为 1） */
void discard_add(Deck* discard, Card* card);

/* 获取牌的显示名称（中文） */
const char* card_get_name(const Card* card);

/* 获取花色字符 */
char card_get_suit_char(const Card* card);

/* 获取点数字符串 */
const char* card_get_rank_str(const Card* card);

/* 获取花色中文名 */
const char* card_get_suit_name(const Card* card);

/* 获取颜色中文名 */
const char* card_get_color_name(const Card* card);

/* 获取牌的完整信息字符串（花色+点数+牌名+颜色），用于日志 */
const char* card_get_full_name(const Card* card);

/* 获取牌大类名称 */
const char* card_get_type_name(const Card* card);

/* ===== 无效化相关 ===== */

/* 将牌标记为无效（is_valid = 0），跳过这张牌后续的所有效果结算 */
void card_invalidate(Card* card);

/* 检查牌是否仍然有效，返回 1=有效，0=已被无效 */
int card_is_valid(const Card* card);


#endif /* CARD_H */
