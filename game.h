#ifndef GAME_H
#define GAME_H


#include "card.h"
#include "player.h"


#define MAX_PLAYERS 2

typedef enum {
    PHASE_CHARACTER_SELECT = 99,  /* 角色选择（用大数避免数组越界） */
    PHASE_PREPARE = 0,
    PHASE_JUDGE,
    PHASE_DRAW,
    PHASE_PLAY,
    PHASE_DISCARD,
    PHASE_END,
    PHASE_GAME_OVER
} Phase;

typedef enum {
    OP_USE_CARD = 0,
    OP_EQUIP_CARD,
    OP_DISCARD_CARD,
    OP_END_PHASE,
    OP_RESTART
} OpType;

typedef struct SDL_Renderer SDL_Renderer;

/* ===== 响应状态 ===== */
typedef enum{
    RESPONSE_NONE               = 0,
    RESPONSE_NEED_WUXIE         = 1,
    RESPONSE_NEED_BASIC         = 2,
    RESPONSE_NEED_WUGU_PICK     = 3,
    RESPONSE_NEED_HUOGONG_SHOW  = 4,   /* 火攻：等待目标(玩家)选牌展示 */
    RESPONSE_NEED_HUOGONG_PICK  = 5,   /* 火攻：等待使用者(玩家)选牌弃置或右键放弃 */
    RESPONSE_NEED_TARGET        = 6,   /* 出牌：等待玩家选择目标角色 */
    RESPONSE_NEED_DISCARD       = 7,   /* 弃牌阶段：等待玩家选择要弃置的牌 */
    RESPONSE_NEED_GUANSHI       = 8,   /* 贯石斧：杀被闪后，等待点击贯石斧或选牌 */
    RESPONSE_NEED_HANBING       = 9,   /* 寒冰剑：杀造成伤害时，等待点击寒冰剑或选对方牌 */
    RESPONSE_NEED_PICK_ENEMY_CARD = 10, /* 过河拆桥/顺手牵羊：等待玩家选择对方的一张牌 */
    RESPONSE_NEED_ZHUQUE        = 11,  /* 朱雀羽扇：打出杀时，等待点击朱雀羽扇或确认/取消 */
    RESPONSE_NEED_ZHANGBA       = 12,  /* 丈八蛇矛：选两张手牌当杀打出 */
    RESPONSE_NEED_BAGUA         = 13,  /* 八卦阵：需要出闪时，等待点击八卦阵或取消 */
    RESPONSE_NEED_PALADIN_CHOICE = 14, /* 圣骑士神圣护盾：等待当前回合角色选择4个选项 */
    RESPONSE_NEED_PALADIN_DISCARD = 15, /* 圣骑士神圣护盾：等待玩家选择弃哪张牌 */
    RESPONSE_NEED_FEIWUU_PICK    = 16, /* 雨蝶飞舞：等待玩家选择要置入装备区的手牌 */
    RESPONSE_NEED_GENERIC_DISCARD = 17, /* 通用主动弃牌：选牌→确认→弃牌 */
    RESPONSE_NEED_CONFIRM_PLAY    = 18, /* 确认出牌：选完目标后，点击确定才打出 */
    RESPONSE_NEED_TIESUO_TARGET   = 19, /* 铁索连环：选择1-2名目标，改变横置状态 */
    RESPONSE_NEED_GILGAMESH_SUOJIAN = 20, /* 吉尔伽美什所见：选择牌类型 */
    RESPONSE_NEED_GILGAMESH_GUAILI = 21, /* 吉尔伽美什乖离：选择花色 */
    RESPONSE_NEED_GILGAMESH_TIANPI_TARGET = 22, /* 吉尔伽美什天辟：选择目标 */
    RESPONSE_NEED_GILGAMESH_TIANPI_RANK = 23, /* 吉尔伽美什天辟：选择点数（点击手牌选点数） */
    RESPONSE_NEED_TIESUO_WUXIE = 24, /* 铁索连环：依次询问目标是否被无懈 */
    RESPONSE_NEED_FEIWUU_DRAG = 25, /* 雨蝶飞舞：拖拽手牌到装备区 */
    RESPONSE_NEED_GROUP_TARGET = 26, /* 群体锦囊：从自己开始顺时针依次指定所有角色 */
    RESPONSE_NEED_HUAXING_SUIT = 27, /* 化形①：选择要执行的花色 */
    RESPONSE_NEED_HUAXING_HAND = 28, /* 化形①：选择该花色的一张手牌 */
    RESPONSE_NEED_HUAXING_TRICK = 29, /* 化形①：选择本回合使用过的锦囊牌名 */
    RESPONSE_NEED_HUAXING_TARGET = 30, /* 化形②：指定一名角色 */
    RESPONSE_NEED_LINYUXIA_YUZHAN = 31, /* 林雨霞玉盏：确认是否发动 */
    RESPONSE_NEED_PALADIN_POMIE_CARD = 32, /* 圣骑士破灭护盾：选择要打出的牌名 */
    RESPONSE_NEED_MULTI_WUXIE = 33, /* 多目标锦囊无懈可击结算（含反无懈栈） */
    RESPONSE_NEED_MULTI_TARGET = 34, /* 通用多目标选择：选1~N个目标，点击确定确认 */
    RESPONSE_NEED_DENGXIAN_CONVERT = 35, /* 登仙牌型转换选择：当原牌/当桃/当桃园 */
    RESPONSE_NEED_YUZHAN_TARGET = 36, /* 玉盏目标调整：增加/减少/不调整 */
    RESPONSE_NEED_LIUYING_BENGFA = 37, /* 流萤迸发：选择火属性/雷属性 */
    RESPONSE_NEED_JINGLIU_GUJING = 38, /* 镜流古镜照神：选择选项1/选项2/取消 */
}RespState;

/* ===== 无懈可击栈帧：用于多层反无懈 ===== */
#define MAX_WUXIE_STACK 16
typedef struct {
    int user_idx;           /* 打出无懈/反无懈的人 */
    int target_idx;         /* 这张无懈的目标（对谁使用） */
    int trick_target_idx;   /* 当前正在结算的锦囊目标 */
    int asker_idx;          /* 当前询问到谁（逆时针） */
    int ask_count;          /* 已询问人数（用于判断是否一轮结束） */
} WuxieStackFrame;

/* ===== 通用倒计时 ===== */
typedef struct {
    int active;             /* 是否激活 */
    float remaining;        /* 剩余秒数 */
    float duration;         /* 总时长 */
    int callback_type;      /* 回调类型：0=无懈可击超时不响应 */
} CountdownTimer;

typedef struct GameState{
    Player players[MAX_PLAYERS];
    int player_count;

    Deck deck;
    Deck discard;

    int current_player;
    Phase phase;

    int game_over;
    int winner_id;

    int turn_count;

    /* ===== 角色选择 ===== */
    int player_hero_id;   /* 玩家选择的角色(-1=未选) */
    int ai_hero_id;       /* AI选择的角色(-1=未选) */
    int select_hover;     /* 鼠标悬停的角色索引(-1=无) */
    int selected_hero_idx; /* 第一下点击后选中的角色索引(-1=未选) */

    char log_buf[200][128];
    int log_count;
    int log_panel_open;   /* 日志弹窗是否打开 */
    int log_scroll;       /* 日志滚动位置 */

    RespState resp_state;

    Card* central_show_card;

    /* 屏幕中心展示牌（通用展示函数） */
    Card* show_card_center;
    char  show_card_who[32];
    int   show_card_timer;  /* 展示剩余帧数，0=不展示 */

    /* 屏幕中心批量展示牌（用于飞舞/化蝶亮出的牌） */
    Card* show_cards_center[10];
    int   show_cards_count;
    int   show_cards_timer;  /* 展示剩余帧数，0=不展示 */
    int   show_cards_total;  /* 展示总帧数（用于时间条计算） */
    char  show_cards_who[32];

    /* 屏幕中心文字提示（通用消息提示） */
    char  center_message[256];
    int   center_message_timer;  /* 展示剩余帧数，0=不展示 */

    /* 长按技能描述展示 */
    int   long_press_skill_idx;  /* 长按显示的技能下标（-1=不显示） */

    int duel_turn;
    int resp_source_player;
    int resp_target_player;
    Card* resp_trigger_card;
    int resp_need_basic_after_wuxie;
    int resp_required_basic;
    int ai_play_finished;

    int   group_active;
    int   group_phase;
    int   group_current;
    int   group_source;
    int   group_trick_type;
    Card* group_trigger_card;
    int   group_wuxie_mask;
    int   group_wuxie_counter_from;  /* 当前打无懈的玩家(-1表示无反无懈阶段)，用于反无懈可击 */
    Card* group_wugu_pile[16];
    int   group_wugu_count;

    /* ===== 当前伤害来源（用于藤甲等防具判断，造成伤害前设置） ===== */
    int   current_damage_source;
    int   current_damage_source_player;  /* 当前伤害来源角色下标（-1=无来源） */

    /* ===== 火攻状态 ===== */
    int   huogong_active;
    int   huogong_source;
    int   huogong_target;
    Card* huogong_show_card;
    int   huogong_need_suit;
    int   huogong_picked_hand;   /* PICK阶段选中的手牌下标（-1=未选中） */

    /* ===== 贯石斧状态（杀被闪后，弃两张牌强制命中） ===== */
    int   guanshi_active;        /* 是否处于贯石斧发动阶段 */
    int   guanshi_source;        /* 贯石斧使用者 */
    int   guanshi_target;        /* 贯石斧目标 */
    int   guanshi_damage;        /* 原本应造成的伤害 */
    int   guanshi_picking;       /* 是否正在选牌阶段（点击贯石斧后进入） */
    int   guanshi_picked_count;  /* 已选择的手牌数量 */
    int   guanshi_picked[2];     /* 已选择的手牌下标 */

    /* ===== 雨蝶飞舞：选择要置入装备区的手牌 ===== */
    int   feiwuu_selected_count;  /* 已选择的手牌数量（0-4） */
    int   feiwuu_selected[4];     /* 已选择的手牌下标 */

    /* ===== 寒冰剑状态（杀造成伤害时，弃对方2牌免伤） ===== */
    int   hanbing_active;         /* 是否处于寒冰剑发动阶段 */
    int   hanbing_source;         /* 寒冰剑使用者 */
    int   hanbing_target;         /* 寒冰剑目标 */
    int   hanbing_damage;         /* 原本应造成的伤害 */
    int   hanbing_picking;        /* 是否正在选牌阶段（点击寒冰剑后进入） */
    int   hanbing_picked_count;   /* 已选择的牌数量 */
    int   hanbing_picked_type[2]; /* 已选择的牌类型：0=手牌,1=武器,2=防具,3=进攻马,4=防御马 */
    int   hanbing_picked_index[2];/* 已选择的牌下标 */

    /* ===== 过河拆桥/顺手牵羊：选择对方的一张牌 ===== */
    int   pick_enemy_target;      /* 对方玩家下标 */
    int   pick_enemy_action;      /* 0=过河拆桥（弃置），1=顺手牵羊（获得） */
    int   pick_enemy_card_type;   /* 选中的牌类型：0=手牌,1=武器,2=防具,3=进攻马,4=防御马,5=延时锦囊 */
    int   pick_enemy_card_index;  /* 选中的牌下标 */
    int   pick_enemy_callback_type;  /* 选牌完成后回调：0=无,1=无罅飞光弃牌后结算杀,2=古镜照神选项A继续 */
    Card* pick_enemy_sha_card;       /* 回调用：待结算的杀牌 */
    int   pick_enemy_sha_targets[MAX_PLAYERS]; /* 回调用：杀目标列表 */
    int   pick_enemy_sha_target_count;  /* 回调用：杀目标总数 */
    int   pick_enemy_sha_current;    /* 回调用：当前处理到哪个目标 */
    int   pick_enemy_sha_source;     /* 回调用：杀的使用者 */
    int   gujing_pick_remaining[MAX_PLAYERS]; /* 古镜照神待处理角色 */
    int   gujing_pick_remaining_count;  /* 古镜照神待处理角色数 */
    int   gujing_pick_source;        /* 古镜照神使用者 */
    int   dengxian_convert_hand_index; /* 登仙转换：待转换的手牌下标 */

    /* ===== 破灭护盾响应模式：保存原响应状态 ===== */
    int   pomie_saved_resp_state;     /* 保存的原响应状态 */
    Card* pomie_saved_trigger_card;   /* 保存的响应触发牌 */
    int   pomie_saved_source_player;  /* 保存的响应来源 */
    int   pomie_saved_target_player;  /* 保存的响应目标 */
    int   pomie_saved_required_basic; /* 保存的需要响应的基本牌类型 */
    int   pomie_saved_duel_turn;      /* 保存的决斗回合 */

    /* ===== 通用单体锦囊无懈可击（待执行锦囊保存） ===== */
    int   single_trick_pending;    /* 是否有待执行的单体锦囊（0=无,1=有） */
    int   single_trick_type;       /* 锦囊类型（TRICK_WUZHONG等） */
    int   single_trick_source;     /* 使用者下标 */
    int   single_trick_target;     /* 目标下标（-1表示无目标） */
    Card* single_trick_card;       /* 锦囊牌指针（已从手牌移除） */

    /* ===== 圣骑士神圣护盾弃牌选择 ===== */
    int   paladin_discard_option; /* 当前需要弃牌的选项（2或3），-1表示无 */
    int   paladin_discard_selected; /* 当前选中的手牌下标，-1表示未选中 */

    /* ===== 通用主动弃牌（选牌→确认→弃牌） ===== */
    int   generic_discard_player;   /* 弃牌的玩家下标 */
    int   generic_discard_need;     /* 需要弃几张牌 */
    int   generic_discard_selected[10]; /* 已选中的手牌下标 */
    int   generic_discard_selected_count; /* 已选中的牌数 */
    int   generic_discard_source;   /* 来源标识（调用者自定义，用于后续判断） */
    int   generic_discard_done;     /* 是否完成弃牌（0=进行中，1=已完成） */

    /* ===== 确认出牌（选完目标后，点击确定才打出） ===== */
    int   confirm_play_hand_index;  /* 待打出的手牌下标 */
    int   confirm_play_target_index; /* 目标下标（-1表示没有目标） */

    /* ===== 通用多目标选择（杀多目标/锦囊多目标等） ===== */
    int   multi_target_hand_index;   /* 待使用的牌下标（-1表示虚拟牌） */
    Card* multi_target_card;         /* 待使用的牌指针 */
    int   multi_targets[MAX_PLAYERS]; /* 已选中的目标下标列表 */
    int   multi_target_count;        /* 已选中的目标数 */
    int   multi_target_min;          /* 最少需要选的目标数 */
    int   multi_target_max;          /* 最多可选目标数 */
    int   multi_target_source;       /* 使用者下标 */

    /* ===== 铁索连环选目标（选择1-2名目标，改变横置状态） ===== */
    int   tiesuo_hand_index;        /* 铁索连环手牌下标 */
    int   tiesuo_targets[2];        /* 选中的目标下标（-1表示未选） */
    int   tiesuo_target_count;       /* 已选中的目标数 */
    int   tiesuo_wuxie_index;       /* 当前询问无懈可击的目标下标 */
    int   tiesuo_wuxie_mask;        /* 被无懈的目标掩码（bit 0/1） */

    /* ===== 多目标锦囊无懈可击结算（通用） ===== */
    Card* multi_wuxie_card;         /* 锦囊牌 */
    int   multi_wuxie_source;       /* 锦囊使用者 */
    int   multi_wuxie_targets[MAX_PLAYERS]; /* 所有目标列表 */
    int   multi_wuxie_target_count;  /* 目标数量 */
    int   multi_wuxie_current_target; /* 当前正在结算的目标索引（在targets数组中的位置） */
    int   multi_wuxie_wuxie_mask;    /* 被无懈的目标掩码 */
    WuxieStackFrame multi_wuxie_stack[MAX_WUXIE_STACK]; /* 无懈可击栈 */
    int   multi_wuxie_stack_depth;   /* 当前栈深度 */
    int   multi_wuxie_trick_type;    /* 锦囊类型（用于最终结算） */
    int   multi_wuxie_resolved;      /* 是否已完成结算 */

    /* ===== 通用倒计时 ===== */
    CountdownTimer countdown;        /* 倒计时器 */

    /* ===== 群体锦囊指定目标（从自己开始顺时针依次指定所有角色） ===== */
    int   group_target_hand_index;  /* 群体锦囊手牌下标 */
    int   group_target_order[MAX_PLAYERS]; /* 指定顺序（从自己开始顺时针） */
    int   group_target_count;       /* 需要指定的目标数 */
    int   group_target_current;     /* 当前需要指定的目标下标（在order中的位置） */
    int   group_target_confirmed;   /* 已确认的目标数 */

    /* ===== 化形技能相关 ===== */
    int   huaxing_current_suit;     /* 化形①当前处理的花色 */
    int   huaxing_selected_hand;    /* 化形①选中的手牌下标 */
    int   huaxing_selected_trick;   /* 化形①选中的锦囊牌名下标 */
    int   huaxing_suit_order[4];    /* 化形①待处理的花色列表 */
    int   huaxing_suit_count;       /* 待处理的花色数 */
    int   huaxing_suit_index;       /* 当前处理的花色在列表中的位置 */
    int   huaxing_trick_type;       /* 化形①当前要转化的锦囊类型 */
    Card* huaxing_used_card;        /* 化形①被转化的手牌（已移除） */
    int   huaxing_selecting_target; /* 化形①是否正在选目标 */
    int   huaxing_after_group;      /* 群体锦囊结算后是否回到化形 */
    int   huaxing_after_pick;       /* 拆/牵牌后是否回到化形 */
    int   huaxing_after_duel;       /* 决斗后是否回到化形 */
    int   huaxing_after_huogong;    /* 火攻后是否回到化形 */

    /* ===== 雨蝶飞舞拖拽放置 ===== */
    Card* feiwuu_drag_cards[4];     /* 待放置的牌 */
    int   feiwuu_drag_count;        /* 待放置的牌数 */
    int   feiwuu_drag_index;        /* 当前拖拽的牌在 feiwuu_drag_cards 中的下标 */
    int   feiwuu_dragging;          /* 是否正在拖拽 */
    int   feiwuu_drag_x;            /* 拖拽牌的当前x位置 */
    int   feiwuu_drag_y;            /* 拖拽牌的当前y位置 */
    int   feiwuu_placed_slots[4];   /* 哪些装备槽已经放置了牌（0=武器,1=防具,2=进攻马,3=防御马） */

    /* ===== 吉尔伽美什技能选择 ===== */
    int   gilgamesh_skill_idx;      /* 当前发动的技能下标 */
    int   gilgamesh_target_idx;     /* 天辟选择的目标 */

    /* ===== 鼠标位置（用于悬停高亮） ===== */
    int   mouse_x;
    int   mouse_y;

    /* ===== AI操作延迟（帧计数，约0.75秒=45帧@60fps） ===== */
    int   ai_delay;              /* AI操作延迟计数器 */
    int   ai_delay_active;       /* AI延迟是否激活 */

    /* ===== 选目标状态（出牌阶段选择目标角色） ===== */
    int   pending_hand_index;   /* 待使用的牌在手牌中的下标，-1表示无 */
    Card* pending_card;         /* 待使用的牌指针 */

    /* ===== 弃牌阶段状态 ===== */
    int   discard_need_count;   /* 还需要弃多少张牌 */

    /* ===== 响应选牌状态（通用鼠标点击交互：无懈可击/杀/闪） ===== */
    int   response_pick_selected;  /* 是否已选中响应牌：0=未选中，1=已选中 */
    int   response_pick_index;     /* 选中的响应牌在手牌中的下标，-1表示未选中 */

    /* ===== on_card_used 回调标记（避免game_use_card重复调用） ===== */
    int   on_card_used_done;       /* internal中已触发on_card_used时置1，game_use_card检查后重置 */

    int   judge_active;
    int   judge_step;
    int   judge_idx;
    Card* judge_delay;
    Card* judge_card;
    char  judge_result[128];
    int   judge_delay_action;

    /* ===== 朱雀羽扇状态（打出杀时，可选择将杀变成火杀） ===== */
    int   zhuque_active;          /* 是否处于朱雀羽扇发动阶段 */
    int   zhuque_source;          /* 朱雀羽扇使用者 */
    int   zhuque_target;          /* 杀的目标 */
    Card* zhuque_sha_card;        /* 打出的杀牌指针 */
    int   zhuque_selected;        /* 是否已选中朱雀羽扇：0=未选中，1=已选中 */

    /* ===== 丈八蛇矛状态（出牌阶段选两张手牌当杀打出） ===== */
    int   zhangba_active;         /* 是否处于丈八蛇矛选牌模式 */
    int   zhangba_selected_count; /* 已选择的手牌数量 */
    int   zhangba_selected[2];    /* 已选择的手牌下标 */
    Card* zhangba_virtual_sha;    /* 构造的虚拟杀牌（动态分配） */

    /* ===== 八卦阵状态（需要出闪时，可选择判定） ===== */
    int   bagua_active;           /* 是否处于八卦阵发动阶段 */
    int   bagua_source;           /* 八卦阵使用者（需要出闪的目标） */
    int   bagua_attacker;         /* 杀的攻击者 */
    Card* bagua_trigger_card;     /* 触发八卦阵的杀牌 */
    int   bagua_selected;         /* 是否已选中八卦阵：0=未选中，1=已选中 */
    Card* bagua_judge_card;       /* 八卦阵的判定牌 */

    /* ===== 圣骑士神圣护盾选择状态 ===== */
    int   paladin_choice_paladin_idx;  /* 圣骑士的玩家索引 */
    int   paladin_choice_turn_idx;     /* 当前回合角色的索引（选择者） */

    /* ===== 圣骑士破灭护盾选择牌名状态 ===== */
    char  pomie_selected_card_name[32]; /* 选中的牌名 */
    int   pomie_mode;                   /* 破灭护盾模式：0=无，1=响应时，2=主动发动时 */

} GameState;

void game_init(GameState* g);
void game_start_with_heroes(GameState* g, int player_hero, int ai_hero);

/* 通用玩家初始化：支持任意数量玩家，hero_ids和is_ai_flags数组长度为player_count */
void game_init_players(GameState* g, int* hero_ids, int* is_ai_flags, int player_count);

void game_destroy(GameState* g);
void game_use_active_skill(GameState* g, int player_idx, int skill_idx);
void game_restart(GameState* g);
void game_nn_ai_init(void);  /* 初始化神经网络AI */
void game_log(GameState* g, const char* fmt, ...);
void game_show_card(GameState* g, Card* card, const char* who);
void game_show_cards(GameState* g, Card** cards, int count, const char* who, int duration_frames); /* 批量展示牌 */
void game_show_message(GameState* g, const char* message, int duration_frames); /* 屏幕中心文字提示 */
void game_next_phase(GameState* g);
void game_draw_cards(GameState* g, int player_idx, int n);
int game_use_card(GameState* g, int player_idx, int hand_index, int target_idx);
int game_use_card_internal(GameState* g, int player_idx, int hand_index, int target_idx); /* 内部实现，不触发on_card_used */
int game_equip_card(GameState* g, int player_idx, int hand_index);
void game_discard_to_limit(GameState* g, int player_idx);
int game_effective_hand_limit(GameState* g, int player_idx); /* 实际手牌上限（含武将技能修正） */
void game_dying_resolve(GameState* g, int victim_idx, int damage_source);
void game_deal_damage(GameState* g, int target_idx, int amount, int source_idx, DamageType dmg_type);
void game_lose_hp(GameState* g, int player_idx, int amount);  /* 流失体力：不触发伤害相关技能 */

/* ===== 通用倒计时 ===== */
void game_countdown_start(GameState* g, float seconds, int callback_type);
void game_countdown_stop(GameState* g);
void game_countdown_update(GameState* g, float delta_seconds);

/* ===== 多目标锦囊无懈可击结算（通用，支持无限层反无懈） ===== */
void game_start_multi_wuxie(GameState* g, Card* card, int source,
                            int* targets, int target_count, int trick_type);
void game_multi_wuxie_advance(GameState* g);
void game_multi_wuxie_use(GameState* g, int user_idx);
void game_multi_wuxie_pass(GameState* g);
void game_multi_wuxie_final_resolve(GameState* g);

/* ===== 伤害来源获取函数（反伤将用） ===== */
int game_get_damage_source_type(GameState* g);       /* 获取当前伤害来源类型（DMG_SRC_*） */
int game_get_damage_source_player(GameState* g);     /* 获取当前伤害来源角色下标（-1=无来源） */

void game_check_victory(GameState* g);
const char* game_get_player_name(GameState* g, int idx);
int game_calc_distance(GameState* g, int from_idx, int to_idx);

/* 通用玩家位置计算：2人场保持现有位置，多人场环形布局 */
void game_get_player_position(int player_idx, int player_count, int *x, int *y);

int game_calc_sha_damage(GameState* g, int attacker_idx, int target_idx);
int game_play_card(GameState* game, int hand_index);
int game_can_play_card(const GameState* game);
void render_response_ui(SDL_Renderer* ren, GameState* g);
void game_update(GameState* g);
int  game_ai_try_play_one(GameState* g, int player_idx);
void input_handle_wuxie_y(GameState* g);
void input_handle_wuxie_n(GameState* g);
void input_handle_response_y(GameState* g);
void input_handle_response_n(GameState* g);

/* ===== 火攻三函数 ===== */
void input_handle_huogong_show(GameState* g, int hand_index);
void input_handle_huogong_pick(GameState* g, int hand_index);
void input_handle_huogong_confirm(GameState* g);  /* 点击确定确认弃置 */
void input_handle_huogong_cancel(GameState* g);

/* ===== 选目标（出牌阶段选择目标角色） ===== */
int  card_needs_target(Card* card);
int  card_is_group_trick(Card* card);  /* 判断是否是群体锦囊 */
void game_start_target_select(GameState* g, int hand_index);
int  game_can_target(GameState* g, Card* card, int from_idx, int target_idx);
void game_select_target(GameState* g, int target_idx);
void game_cancel_target_select(GameState* g);

/* ===== 确认出牌（选完目标后，点击确定才打出） ===== */
void game_start_confirm_play(GameState* g, int hand_index, int target_index);
void game_confirm_play(GameState* g);
void game_cancel_confirm_play(GameState* g);

/* ===== 通用多目标选择 ===== */
void game_start_multi_target(GameState* g, int source_idx, int hand_index, int min_targets, int max_targets);
void game_multi_target_toggle(GameState* g, int player_idx);
void game_multi_target_confirm(GameState* g);
void game_multi_target_cancel(GameState* g);
int  game_multi_target_is_selected(GameState* g, int player_idx);

/* 登仙牌型转换 */
void game_start_dengxian_convert(GameState* g, int hand_index);
void game_dengxian_convert_choose(GameState* g, int choice); /* 0=原牌,1=当桃,2=当桃园 */
void game_dengxian_convert_cancel(GameState* g);

/* 玉盏目标调整 */
void game_start_yuzhan_target(GameState* g, int hand_index); /* 进入玉盏目标调整选择 */
void game_yuzhan_target_choose(GameState* g, int choice); /* 0=增加目标,1=减少目标,2=不调整 */
void game_yuzhan_target_cancel(GameState* g); /* 取消使用牌 */

/* ===== 铁索连环选目标（选择1-2名目标，改变横置状态） ===== */
void game_start_tiesuo_target(GameState* g, int hand_index);  /* 进入铁索连环选目标状态 */
void game_tiesuo_toggle_target(GameState* g, int player_idx); /* 切换目标选中状态 */
void game_tiesuo_confirm(GameState* g);   /* 确认选择，进入无懈可击询问 */
void game_tiesuo_cancel(GameState* g);    /* 取消选择 */
void game_tiesuo_chongzhu(GameState* g);  /* 重铸：弃置铁索连环，摸一张牌 */
void game_tiesuo_wuxie_advance(GameState* g); /* 推进无懈可击询问 */
void game_tiesuo_wuxie_result(GameState* g, int used_wuxie); /* 玩家无懈可击结果处理 */
void game_tiesuo_final_resolve(GameState* g); /* 最终结算横置状态 */

/* ===== 群体锦囊指定目标 ===== */
void game_start_group_target(GameState* g, int hand_index);  /* 进入群体锦囊指定目标状态 */
void game_group_target_confirm(GameState* g);   /* 指定当前目标 */
void game_group_target_cancel(GameState* g);    /* 取消指定目标 */
void game_huaxing_cancel(GameState* g);         /* 化形技能取消 */

/* ===== 雨蝶飞舞拖拽放置 ===== */
void game_feiwuu_start_drag(GameState* g);  /* 进入拖拽放置状态 */
void game_feiwuu_begin_drag(GameState* g, int card_idx, int mx, int my); /* 开始拖拽一张牌 */
void game_feiwuu_update_drag(GameState* g, int mx, int my); /* 更新拖拽位置 */
void game_feiwuu_end_drag(GameState* g, int mx, int my); /* 结束拖拽，检测放置位置 */
void game_feiwuu_place_card(GameState* g, int slot_idx); /* 将牌放置到指定装备槽 */
void game_feiwuu_finish_resolve(GameState* g); /* 飞舞后续结算 */
void game_feiwuu_drag_cancel(GameState* g); /* 取消拖拽放置 */
int  game_feiwuu_hit_equip_slot(GameState* g, int mx, int my); /* 检测点击的装备槽 */

/* ===== 吉尔伽美什技能交互式选择 ===== */
void game_gilgamesh_start_suojian(GameState* g);  /* 进入所见：选择牌类型 */
void game_gilgamesh_select_type(GameState* g, int type_idx); /* 选择牌类型 */
void game_gilgamesh_start_guaili(GameState* g);   /* 进入乖离：选择花色 */
void game_gilgamesh_select_suit(GameState* g, int suit_idx); /* 选择花色 */
void game_gilgamesh_start_tianpi_target(GameState* g); /* 进入天辟：选择目标 */
void game_gilgamesh_select_tianpi_target(GameState* g, int player_idx); /* 选择天辟目标 */
void game_gilgamesh_start_tianpi_rank(GameState* g); /* 进入天辟：选择点数（点击手牌） */
void game_gilgamesh_select_rank(GameState* g, int hand_index); /* 点击手牌选择点数 */
void game_gilgamesh_cancel(GameState* g); /* 取消技能选择 */

/* ===== 通用：从牌堆中随机获取一张指定大类的牌 ===== */
Card* game_draw_random_card_by_type(GameState* g, int player_idx, CardType type);

/* ===== 弃牌阶段（玩家手动选牌弃置） ===== */
void game_discard_one(GameState* g, int hand_index);  /* 玩家弃一张牌 */
void game_discard_auto(GameState* g, int player_idx); /* AI自动弃牌到上限 */

/* ===== 通用主动弃牌（选牌→确认→弃牌） ===== */
void game_start_generic_discard(GameState* g, int player_idx, int need_count, int source);
void game_generic_discard_pick(GameState* g, int hand_index);
void game_generic_discard_confirm(GameState* g);
void game_generic_discard_cancel(GameState* g);

void game_group_advance(GameState* g);
void game_clear_duel(GameState* g);
void game_resolve_judge(GameState* g, int player_idx);
void game_judge_advance(GameState* g);

/* ===== 杀伤害计算（含酒、古锭刀效果） ===== */
int game_calc_sha_damage(GameState* g, int attacker_idx, int target_idx);

/* ===== 无视防具判定（青缸剑：攻击者装备青缸剑时，目标防具失效） ===== */
int game_ignore_armor(GameState* g, int attacker_idx, int target_idx);

/* ===== 贯石斧（杀被闪后，弃两张牌强制命中） ===== */
void game_start_guanshi(GameState* g, int source_idx, int target_idx, int damage);
void game_guanshi_click_weapon(GameState* g);  /* 左键点击贯石斧：进入通用弃牌选择 */
void game_guanshi_confirm(GameState* g);  /* 通用弃牌完成后：造成伤害 */
void game_guanshi_cancel(GameState* g);   /* 右键取消发动 */
void game_guanshi_clear(GameState* g);    /* 清除贯石斧状态 */

/* ===== 雨蝶飞舞（选择要置入装备区的手牌） ===== */
void game_start_feiwuu_pick(GameState* g);      /* 进入飞舞选牌状态 */
void game_feiwuu_pick_card(GameState* g, int hand_index);  /* 选中/取消选中一张手牌 */
void game_feiwuu_confirm(GameState* g);          /* 确认选择，执行结算 */
void game_feiwuu_cancel(GameState* g);           /* 取消选择 */

/* ===== 寒冰剑（杀造成伤害时，弃对方2牌免伤） ===== */
void game_start_hanbing(GameState* g, int source_idx, int target_idx, int damage);
void game_hanbing_click_weapon(GameState* g);  /* 左键点击寒冰剑：进入选牌阶段 */
void game_hanbing_pick_card(GameState* g, int card_type, int card_index);  /* 选一张牌（手牌/装备） */
void game_hanbing_confirm(GameState* g);  /* 选满两张后确认：弃对方牌+免伤 */
void game_hanbing_cancel(GameState* g);   /* 右键点击寒冰剑：取消发动，正常造成伤害 */
void game_hanbing_clear(GameState* g);    /* 清除寒冰剑状态 */

/* ===== 过河拆桥/顺手牵羊：选择对方的一张牌 ===== */
void game_start_pick_enemy_card(GameState* g, int source_idx, int target_idx, int action);
void game_pick_enemy_card(GameState* g, int card_type, int card_index);  /* 选择一张牌 */
void game_confirm_pick_enemy_card(GameState* g);  /* 确认选择，执行拆/牵 */
void game_cancel_pick_enemy_card(GameState* g);   /* 取消选择 */
void game_clear_pick_enemy_card(GameState* g);    /* 清除选择状态 */

/* ===== 朱雀羽扇（打出杀时，可选择将杀变成火杀） ===== */
void game_start_zhuque(GameState* g, int source_idx, int target_idx, Card* sha_card);
void game_zhuque_click_weapon(GameState* g);  /* 左键点击朱雀羽扇：选中 */
void game_zhuque_confirm(GameState* g);  /* 确认：杀变成火杀，继续结算 */
void game_zhuque_cancel(GameState* g);   /* 取消：普通杀继续结算 */
void game_zhuque_clear(GameState* g);    /* 清除朱雀羽扇状态 */

/* ===== 丈八蛇矛（出牌阶段选两张手牌当杀打出） ===== */
void game_start_zhangba(GameState* g);  /* 点击丈八蛇矛，进入选牌模式 */
void game_zhangba_pick_card(GameState* g, int hand_index);  /* 选择/取消选择一张手牌 */
void game_zhangba_confirm(GameState* g);  /* 确认：把两张牌当杀打出 */
void game_zhangba_cancel(GameState* g);   /* 取消：退出选牌模式 */
void game_zhangba_clear(GameState* g);    /* 清除丈八蛇矛状态 */

/* 使用虚拟杀牌（丈八蛇矛构造的杀，不需要从手牌移除） */
void game_use_virtual_sha(GameState* g, int source_idx, int target_idx, Card* sha_card);

/* ===== 通用判定函数（单独写，用于八卦阵等需要判定的场景） ===== */
Card* game_perform_judge(GameState* g, const char* judge_name);
/* 判定完成后，将判定牌置入弃牌堆 */
void game_finish_judge(GameState* g, Card* judge_card);

/* ===== 八卦阵（需要出闪时，可选择判定，红色视为虚拟闪） ===== */
void game_start_bagua(GameState* g, int source_idx, int attacker_idx, Card* trigger_card);
void game_bagua_click_armor(GameState* g);  /* 左键点击八卦阵：选中 */
void game_bagua_confirm(GameState* g);  /* 确认：进行判定 */
void game_bagua_cancel(GameState* g);   /* 取消：跳过八卦阵，继续结算杀 */
void game_bagua_clear(GameState* g);    /* 清除八卦阵状态 */


/* 训练环境标志：1=训练环境（群体技能自动响应），0=游戏本体 */
extern int g_training_mode;

#endif /* GAME_H */
