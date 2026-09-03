#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "card.h"


/* ===== 内部辅助 ===== */
static int g_next_card_id = 1;


static Card* card_new_basic_sha(Suit suit, Rank rank, ShaElement elem)
{
    Card* c = (Card*)malloc(sizeof(Card));
    memset(c, 0, sizeof(Card));
    c->id = g_next_card_id++;
    c->type = CARD_BASIC;
    c->suit = suit;
    c->rank = rank;
    c->is_valid = 1;   /* 新创建的牌默认为有效 */
    c->sub.basic.basic_type = BASIC_SHA;
    c->sub.basic.sha_element = elem;
    return c;
}


static Card* card_new_basic_shan(Suit suit, Rank rank)
{
    Card* c = (Card*)malloc(sizeof(Card));
    memset(c, 0, sizeof(Card));
    c->id = g_next_card_id++;
    c->type = CARD_BASIC;
    c->suit = suit;
    c->rank = rank;
    c->is_valid = 1;
    c->sub.basic.basic_type = BASIC_SHAN;
    return c;
}


static Card* card_new_basic_tao(Suit suit, Rank rank)
{
    Card* c = (Card*)malloc(sizeof(Card));
    memset(c, 0, sizeof(Card));
    c->id = g_next_card_id++;
    c->type = CARD_BASIC;
    c->suit = suit;
    c->rank = rank;
    c->is_valid = 1;
    c->sub.basic.basic_type = BASIC_TAO;
    return c;
}


static Card* card_new_basic_jiu(Suit suit, Rank rank)
{
    Card* c = (Card*)malloc(sizeof(Card));
    memset(c, 0, sizeof(Card));
    c->id = g_next_card_id++;
    c->type = CARD_BASIC;
    c->suit = suit;
    c->rank = rank;
    c->is_valid = 1;
    c->sub.basic.basic_type = BASIC_JIU;
    return c;
}


static Card* card_new_trick(Suit suit, Rank rank, TrickType t)
{
    Card* c = (Card*)malloc(sizeof(Card));
    memset(c, 0, sizeof(Card));
    c->id = g_next_card_id++;
    c->type = CARD_TRICK;
    c->suit = suit;
    c->rank = rank;
    c->is_valid = 1;
    c->sub.trick.trick_type = t;
    return c;
}


static Card* card_new_delayed(Suit suit, Rank rank, DelayedType t)
{
    Card* c = (Card*)malloc(sizeof(Card));
    memset(c, 0, sizeof(Card));
    c->id = g_next_card_id++;
    c->type = CARD_DELAYED;
    c->suit = suit;
    c->rank = rank;
    c->is_valid = 1;
    c->sub.delayed.delayed_type = t;
    return c;
}


static Card* card_new_weapon(Suit suit, Rank rank, WeaponType wt, int range)
{
    Card* c = (Card*)malloc(sizeof(Card));
    memset(c, 0, sizeof(Card));
    c->id = g_next_card_id++;
    c->type = CARD_EQUIP;
    c->suit = suit;
    c->rank = rank;
    c->is_valid = 1;
    c->sub.equip.equip_type = EQUIP_WEAPON;
    c->sub.equip.detail.weapon.weapon_type = wt;
    c->sub.equip.detail.weapon.range = range;
    return c;
}


static Card* card_new_armor(Suit suit, Rank rank, ArmorType at)
{
    Card* c = (Card*)malloc(sizeof(Card));
    memset(c, 0, sizeof(Card));
    c->id = g_next_card_id++;
    c->type = CARD_EQUIP;
    c->suit = suit;
    c->rank = rank;
    c->is_valid = 1;
    c->sub.equip.equip_type = EQUIP_ARMOR;
    c->sub.equip.detail.armor.armor_type = at;
    return c;
}


static Card* card_new_horse(Suit suit, Rank rank, EquipType horse_type)
{
    Card* c = (Card*)malloc(sizeof(Card));
    memset(c, 0, sizeof(Card));
    c->id = g_next_card_id++;
    c->type = CARD_EQUIP;
    c->suit = suit;
    c->rank = rank;
    c->is_valid = 1;
    c->sub.equip.equip_type = horse_type;
    return c;
}


static void deck_push(Deck* deck, Card* c)
{
    if (deck->count >= deck->capacity) {
        deck->capacity = deck->capacity ? deck->capacity * 2 : 64;
        deck->cards = (Card**)realloc(deck->cards, deck->capacity * sizeof(Card*));
    }
    deck->cards[deck->count++] = c;
}


/* ===== 牌堆初始化 ===== */
void deck_init_standard(Deck* deck)
{
    memset(deck, 0, sizeof(Deck));
    deck->top = 0;
    g_next_card_id = 1;


    /* ---------- 基本牌：杀 ---------- */
    /* 普通杀 - 黑桃 */
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_7, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_8, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_8, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_9, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_9, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_10, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_10, SHA_NORMAL));

    /* 普通杀 - 梅花 2-7各一张 */
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_2, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_3, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_4, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_5, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_6, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_7, SHA_NORMAL));
    /* 梅花 8-J各两张 */
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_8, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_8, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_9, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_9, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_10, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_10, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_J, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_J, SHA_NORMAL));

    /* 普通杀 - 方片 6-10及K各一张 */
    deck_push(deck, card_new_basic_sha(SUIT_DIAMOND, RANK_6, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_DIAMOND, RANK_7, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_DIAMOND, RANK_8, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_DIAMOND, RANK_9, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_DIAMOND, RANK_10, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_DIAMOND, RANK_K, SHA_NORMAL));

    /* 普通杀 - 红桃 10两张, K一张 */
    deck_push(deck, card_new_basic_sha(SUIT_HEART, RANK_10, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_HEART, RANK_10, SHA_NORMAL));
    deck_push(deck, card_new_basic_sha(SUIT_HEART, RANK_K, SHA_NORMAL));

    /* 雷杀 - 黑桃 4-8各一张 */
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_4, SHA_THUNDER));
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_5, SHA_THUNDER));
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_6, SHA_THUNDER));
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_7, SHA_THUNDER));
    deck_push(deck, card_new_basic_sha(SUIT_SPADE, RANK_8, SHA_THUNDER));
    /* 雷杀 - 梅花 5-8各一张 */
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_5, SHA_THUNDER));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_6, SHA_THUNDER));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_7, SHA_THUNDER));
    deck_push(deck, card_new_basic_sha(SUIT_CLUB, RANK_8, SHA_THUNDER));

    /* 火杀 - 方片 4-5各一张 */
    deck_push(deck, card_new_basic_sha(SUIT_DIAMOND, RANK_4, SHA_FIRE));
    deck_push(deck, card_new_basic_sha(SUIT_DIAMOND, RANK_5, SHA_FIRE));
    /* 火杀 - 红桃 4,7,10各一张 */
    deck_push(deck, card_new_basic_sha(SUIT_HEART, RANK_4, SHA_FIRE));
    deck_push(deck, card_new_basic_sha(SUIT_HEART, RANK_7, SHA_FIRE));
    deck_push(deck, card_new_basic_sha(SUIT_HEART, RANK_10, SHA_FIRE));


    /* ---------- 基本牌：闪 ---------- */
    /* 闪 - 方片 2两张 */
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_2));
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_2));
    /* 方片 6-8各两张 */
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_6));
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_6));
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_7));
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_7));
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_8));
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_8));
    /* 方片 10两张 */
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_10));
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_10));
    /* 方片 3-5各一张 */
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_3));
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_4));
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_5));
    /* 方片 9一张 */
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_9));
    /* 方片 J三张 */
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_J));
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_J));
    deck_push(deck, card_new_basic_shan(SUIT_DIAMOND, RANK_J));

    /* 闪 - 红桃 2两张 */
    deck_push(deck, card_new_basic_shan(SUIT_HEART, RANK_2));
    deck_push(deck, card_new_basic_shan(SUIT_HEART, RANK_2));
    /* 红桃 8-9和J-K各一张 */
    deck_push(deck, card_new_basic_shan(SUIT_HEART, RANK_8));
    deck_push(deck, card_new_basic_shan(SUIT_HEART, RANK_9));
    deck_push(deck, card_new_basic_shan(SUIT_HEART, RANK_J));
    deck_push(deck, card_new_basic_shan(SUIT_HEART, RANK_K));


    /* ---------- 基本牌：桃 ---------- */
    /* 桃 - 方片 2,3,Q各一张 */
    deck_push(deck, card_new_basic_tao(SUIT_DIAMOND, RANK_2));
    deck_push(deck, card_new_basic_tao(SUIT_DIAMOND, RANK_3));
    deck_push(deck, card_new_basic_tao(SUIT_DIAMOND, RANK_Q));
    /* 桃 - 红桃 3-5,7-9,Q各一张, 6两张 */
    deck_push(deck, card_new_basic_tao(SUIT_HEART, RANK_3));
    deck_push(deck, card_new_basic_tao(SUIT_HEART, RANK_4));
    deck_push(deck, card_new_basic_tao(SUIT_HEART, RANK_5));
    deck_push(deck, card_new_basic_tao(SUIT_HEART, RANK_7));
    deck_push(deck, card_new_basic_tao(SUIT_HEART, RANK_8));
    deck_push(deck, card_new_basic_tao(SUIT_HEART, RANK_9));
    deck_push(deck, card_new_basic_tao(SUIT_HEART, RANK_Q));
    deck_push(deck, card_new_basic_tao(SUIT_HEART, RANK_6));
    deck_push(deck, card_new_basic_tao(SUIT_HEART, RANK_6));


    /* ---------- 基本牌：酒 ---------- */
    /* 酒 - 黑桃 3,9各一张 */
    deck_push(deck, card_new_basic_jiu(SUIT_SPADE, RANK_3));
    deck_push(deck, card_new_basic_jiu(SUIT_SPADE, RANK_9));
    /* 酒 - 梅花 3,9各一张 */
    deck_push(deck, card_new_basic_jiu(SUIT_CLUB, RANK_3));
    deck_push(deck, card_new_basic_jiu(SUIT_CLUB, RANK_9));
    /* 酒 - 方片 9一张 */
    deck_push(deck, card_new_basic_jiu(SUIT_DIAMOND, RANK_9));


    /* ---------- 锦囊牌 ---------- */
    /* 决斗 */
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_A, TRICK_JUEDOU));
    deck_push(deck, card_new_trick(SUIT_CLUB, RANK_A, TRICK_JUEDOU));
    deck_push(deck, card_new_trick(SUIT_DIAMOND, RANK_A, TRICK_JUEDOU));

    /* 万箭齐发 */
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_A, TRICK_WANJIAN));

    /* 南蛮入侵 */
    deck_push(deck, card_new_trick(SUIT_CLUB, RANK_7, TRICK_NANMAN));
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_7, TRICK_NANMAN));
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_K, TRICK_NANMAN));

    /* 桃园结义 */
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_A, TRICK_TAOYUAN));

    /* 五谷丰登 */
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_3, TRICK_WUGU));
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_4, TRICK_WUGU));

    /* 顺手牵羊 */
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_3, TRICK_SHUNSHOU));
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_4, TRICK_SHUNSHOU));
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_J, TRICK_SHUNSHOU));
    deck_push(deck, card_new_trick(SUIT_DIAMOND, RANK_3, TRICK_SHUNSHOU));
    deck_push(deck, card_new_trick(SUIT_DIAMOND, RANK_K, TRICK_SHUNSHOU));

    /* 无中生有 */
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_7, TRICK_WUZHONG));
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_8, TRICK_WUZHONG));
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_9, TRICK_WUZHONG));
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_J, TRICK_WUZHONG));

    /* 过河拆桥 */
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_3, TRICK_GUOHE));
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_4, TRICK_GUOHE));
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_Q, TRICK_GUOHE));
    deck_push(deck, card_new_trick(SUIT_CLUB, RANK_3, TRICK_GUOHE));
    deck_push(deck, card_new_trick(SUIT_CLUB, RANK_4, TRICK_GUOHE));
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_Q, TRICK_GUOHE));

    /* 无懈可击 */
    deck_push(deck, card_new_trick(SUIT_CLUB, RANK_Q, TRICK_WUXIE));
    deck_push(deck, card_new_trick(SUIT_CLUB, RANK_K, TRICK_WUXIE));
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_J, TRICK_WUXIE));
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_K, TRICK_WUXIE));
    deck_push(deck, card_new_trick(SUIT_DIAMOND, RANK_Q, TRICK_WUXIE));
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_A, TRICK_WUXIE));
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_K, TRICK_WUXIE));

    /* 火攻 */
    deck_push(deck, card_new_trick(SUIT_DIAMOND, RANK_Q, TRICK_HUOGONG));
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_2, TRICK_HUOGONG));
    deck_push(deck, card_new_trick(SUIT_HEART, RANK_3, TRICK_HUOGONG));

    /* 铁锁连环 */
    deck_push(deck, card_new_trick(SUIT_CLUB, RANK_10, TRICK_TIESUO));
    deck_push(deck, card_new_trick(SUIT_CLUB, RANK_J, TRICK_TIESUO));
    deck_push(deck, card_new_trick(SUIT_CLUB, RANK_Q, TRICK_TIESUO));
    deck_push(deck, card_new_trick(SUIT_CLUB, RANK_K, TRICK_TIESUO));
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_J, TRICK_TIESUO));
    deck_push(deck, card_new_trick(SUIT_SPADE, RANK_Q, TRICK_TIESUO));


    /* ---------- 延时锦囊 ---------- */
    /* 乐不思蜀 */
    deck_push(deck, card_new_delayed(SUIT_HEART, RANK_6, DELAYED_LEBU));
    deck_push(deck, card_new_delayed(SUIT_SPADE, RANK_6, DELAYED_LEBU));
    deck_push(deck, card_new_delayed(SUIT_CLUB, RANK_6, DELAYED_LEBU));

    /* 兵粮寸断 */
    deck_push(deck, card_new_delayed(SUIT_CLUB, RANK_4, DELAYED_BINGLIANG));
    deck_push(deck, card_new_delayed(SUIT_SPADE, RANK_10, DELAYED_BINGLIANG));

    /* 闪电 */
    deck_push(deck, card_new_delayed(SUIT_HEART, RANK_Q, DELAYED_SHANDIAN));
    deck_push(deck, card_new_delayed(SUIT_SPADE, RANK_A, DELAYED_SHANDIAN));


    /* ---------- 装备牌：武器 ---------- */
    /* 雌雄双股剑 距离2 */
    deck_push(deck, card_new_weapon(SUIT_SPADE, RANK_2, WEAPON_CIXIONG, 2));
    /* 朱雀羽扇 距离4 */
    deck_push(deck, card_new_weapon(SUIT_DIAMOND, RANK_A, WEAPON_ZHUQUE, 4));
    /* 诸葛连弩 距离1 */
    deck_push(deck, card_new_weapon(SUIT_DIAMOND, RANK_A, WEAPON_ZHUGELIANNU, 1));
    deck_push(deck, card_new_weapon(SUIT_CLUB, RANK_A, WEAPON_ZHUGELIANNU, 1));
    /* 贯石斧 距离3 */
    deck_push(deck, card_new_weapon(SUIT_DIAMOND, RANK_5, WEAPON_GUANSHI, 3));
    /* 方天画戟 距离4 */
    deck_push(deck, card_new_weapon(SUIT_DIAMOND, RANK_Q, WEAPON_FANGTIAN, 4));
    /* 青缸剑 距离2 */
    deck_push(deck, card_new_weapon(SUIT_SPADE, RANK_6, WEAPON_QINGGANG, 2));
    /* 寒冰剑 距离2 */
    deck_push(deck, card_new_weapon(SUIT_SPADE, RANK_2, WEAPON_HANBING, 2));
    /* 青龙偃月刀 距离3 */
    deck_push(deck, card_new_weapon(SUIT_SPADE, RANK_5, WEAPON_QINGLONG, 3));
    /* 丈八蛇矛 距离3 */
    deck_push(deck, card_new_weapon(SUIT_SPADE, RANK_Q, WEAPON_ZHANGBA, 3));
    /* 古锭刀 距离2 */
    deck_push(deck, card_new_weapon(SUIT_SPADE, RANK_A, WEAPON_GUDING, 2));
    /* 麒麟弓 距离5 */
    deck_push(deck, card_new_weapon(SUIT_HEART, RANK_5, WEAPON_QILIN, 5));


    /* ---------- 装备牌：防具 ---------- */
    /* 八卦阵 */
    deck_push(deck, card_new_armor(SUIT_CLUB, RANK_2, ARMOR_BAGUA));
    deck_push(deck, card_new_armor(SUIT_SPADE, RANK_2, ARMOR_BAGUA));
    /* 藤甲 */
    deck_push(deck, card_new_armor(SUIT_SPADE, RANK_2, ARMOR_TENGJIA));
    deck_push(deck, card_new_armor(SUIT_CLUB, RANK_2, ARMOR_TENGJIA));
    /* 白银狮子 */
    deck_push(deck, card_new_armor(SUIT_CLUB, RANK_A, ARMOR_BAIYIN));
    /* 仁王盾 */
    deck_push(deck, card_new_armor(SUIT_CLUB, RANK_2, ARMOR_RENWANG));


    /* ---------- 装备牌：马 ---------- */
    /* 进攻马 (-1马) */
    deck_push(deck, card_new_horse(SUIT_DIAMOND, RANK_K, EQUIP_HORSE_ATK));
    deck_push(deck, card_new_horse(SUIT_SPADE, RANK_K, EQUIP_HORSE_ATK));
    deck_push(deck, card_new_horse(SUIT_HEART, RANK_5, EQUIP_HORSE_ATK));
    /* 防御马 (+1马) */
    deck_push(deck, card_new_horse(SUIT_HEART, RANK_K, EQUIP_HORSE_DEF));
    deck_push(deck, card_new_horse(SUIT_DIAMOND, RANK_K, EQUIP_HORSE_DEF));
    deck_push(deck, card_new_horse(SUIT_SPADE, RANK_5, EQUIP_HORSE_DEF));
    deck_push(deck, card_new_horse(SUIT_CLUB, RANK_5, EQUIP_HORSE_DEF));


    /* 设置每张牌的颜色和性质 */
    for(int i = 0; i < deck->count; i++)
    {
        Card* c = deck->cards[i];
        if(c->suit == SUIT_HEART || c->suit == SUIT_DIAMOND)
            c->color = COLOR_RED;
        else
            c->color = COLOR_BLACK;
        c->card_nature = CARD_NATURE_REAL;  /* 牌堆里的牌都是真牌 */
    }

    /* 洗牌 */
    deck_shuffle(deck);
}


void deck_destroy(Deck* deck)
{
    if (deck->cards) {
        for (int i = 0; i < deck->count; i++) {
            free(deck->cards[i]);
        }
        free(deck->cards);
    }
    memset(deck, 0, sizeof(Deck));
}


void deck_shuffle(Deck* deck)
{
    srand((unsigned)time(NULL));
    for (int i = deck->count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Card* tmp = deck->cards[i];
        deck->cards[i] = deck->cards[j];
        deck->cards[j] = tmp;
    }
    deck->top = 0;
}


Card* deck_draw(Deck* deck)
{
    if (deck->top >= deck->count) return NULL;
    return deck->cards[deck->top++];
}


void discard_add(Deck* discard, Card* card)
{
    if (!card) return;
    /* 牌进入弃牌堆时，重置有效标记为 1（被无效的牌恢复为可生效状态） */
    card->is_valid = 1;
    if (discard->count >= discard->capacity) {
        discard->capacity = discard->capacity ? discard->capacity * 2 : 64;
        discard->cards = (Card**)realloc(discard->cards, discard->capacity * sizeof(Card*));
    }
    discard->cards[discard->count++] = card;
}


/* ===== 显示名称 ===== */
const char* card_get_name(const Card* card)
{
    if (!card) return "";
    switch (card->type) {
    case CARD_BASIC:
        switch (card->sub.basic.basic_type) {
        case BASIC_SHA:
            switch (card->sub.basic.sha_element) {
            case SHA_NORMAL: return "杀";
            case SHA_THUNDER: return "雷杀";
            case SHA_FIRE: return "火杀";
            }
            return "杀";
        case BASIC_SHAN: return "闪";
        case BASIC_TAO: return "桃";
        case BASIC_JIU: return "酒";
        }
        return "基本牌";
    case CARD_TRICK:
        switch (card->sub.trick.trick_type) {
        case TRICK_JUEDOU: return "决斗";
        case TRICK_WANJIAN: return "万箭齐发";
        case TRICK_NANMAN: return "南蛮入侵";
        case TRICK_TAOYUAN: return "桃园结义";
        case TRICK_WUGU: return "五谷丰登";
        case TRICK_SHUNSHOU: return "顺手牵羊";
        case TRICK_WUZHONG: return "无中生有";
        case TRICK_GUOHE: return "过河拆桥";
        case TRICK_WUXIE: return "无懈可击";
        case TRICK_HUOGONG: return "火攻";
        case TRICK_TIESUO: return "铁锁连环";
        }
        return "锦囊";
    case CARD_DELAYED:
        switch (card->sub.delayed.delayed_type) {
        case DELAYED_LEBU: return "乐不思蜀";
        case DELAYED_BINGLIANG: return "兵粮寸断";
        case DELAYED_SHANDIAN: return "闪电";
        }
        return "延时锦囊";
    case CARD_EQUIP:
        switch (card->sub.equip.equip_type) {
        case EQUIP_WEAPON:
            switch (card->sub.equip.detail.weapon.weapon_type) {
            case WEAPON_CIXIONG: return "雌雄双股剑";
            case WEAPON_ZHUQUE: return "朱雀羽扇";
            case WEAPON_ZHUGELIANNU: return "诸葛连弩";
            case WEAPON_GUANSHI: return "贯石斧";
            case WEAPON_FANGTIAN: return "方天画戟";
            case WEAPON_QINGGANG: return "青缸剑";
            case WEAPON_HANBING: return "寒冰剑";
            case WEAPON_QINGLONG: return "青龙偃月刀";
            case WEAPON_ZHANGBA: return "丈八蛇矛";
            case WEAPON_GUDING: return "古锭刀";
            case WEAPON_QILIN: return "麒麟弓";
            }
            return "武器";
        case EQUIP_ARMOR:
            switch (card->sub.equip.detail.armor.armor_type) {
            case ARMOR_BAGUA: return "八卦阵";
            case ARMOR_TENGJIA: return "藤甲";
            case ARMOR_BAIYIN: return "白银狮子";
            case ARMOR_RENWANG: return "仁王盾";
            }
            return "防具";
        case EQUIP_HORSE_ATK: return "-1马";
        case EQUIP_HORSE_DEF: return "+1马";
        }
        return "装备";
    }
    return "未知";
}


char card_get_suit_char(const Card* card)
{
    if (!card) return '?';
    switch (card->suit) {
    case SUIT_SPADE:   return 'S';  /* 黑桃 */
    case SUIT_HEART:   return 'H';  /* 红桃 */
    case SUIT_CLUB:    return 'C';  /* 梅花 */
    case SUIT_DIAMOND: return 'D';  /* 方片 */
    }
    return '?';
}


const char* card_get_rank_str(const Card* card)
{
    if (!card) return "?";
    switch (card->rank) {
    case RANK_A: return "A";
    case RANK_2: return "2";
    case RANK_3: return "3";
    case RANK_4: return "4";
    case RANK_5: return "5";
    case RANK_6: return "6";
    case RANK_7: return "7";
    case RANK_8: return "8";
    case RANK_9: return "9";
    case RANK_10: return "10";
    case RANK_J: return "J";
    case RANK_Q: return "Q";
    case RANK_K: return "K";
    }
    return "?";
}


/* 获取牌的花色中文名 */
const char* card_get_suit_name(const Card* card)
{
    if (!card) return "?";
    switch (card->suit) {
    case SUIT_SPADE:   return "黑桃";
    case SUIT_HEART:   return "红桃";
    case SUIT_CLUB:    return "梅花";
    case SUIT_DIAMOND: return "方块";
    case SUIT_NONE:    return "无色";
    }
    return "?";
}


/* 获取牌的颜色中文名 */
const char* card_get_color_name(const Card* card)
{
    if (!card) return "?";
    if(card->suit == SUIT_HEART || card->suit == SUIT_DIAMOND)
        return "红色";
    if(card->suit == SUIT_SPADE || card->suit == SUIT_CLUB)
        return "黑色";
    return "无色";
}


/* 获取牌的完整信息字符串（花色+点数+牌名+颜色），用于日志 */
/* 格式：【黑桃A·杀】(黑色) */
const char* card_get_full_name(const Card* card)
{
    static char buf[128];
    if (!card) return "【未知】";
    snprintf(buf, sizeof(buf), "【%s%s·%s】(%s)",
             card_get_suit_name(card),
             card_get_rank_str(card),
             card_get_name(card),
             card_get_color_name(card));
    return buf;
}


const char* card_get_type_name(const Card* card)
{
    if (!card) return "";
    switch (card->type) {
    case CARD_BASIC:   return "基本牌";
    case CARD_TRICK:   return "锦囊牌";
    case CARD_DELAYED: return "延时锦囊";
    case CARD_EQUIP:   return "装备牌";
    }
    return "未知";
}


/* ===== 无效化相关 ===== */

void card_invalidate(Card* card)
{
    if (!card) return;
    card->is_valid = 0;
}


int card_is_valid(const Card* card)
{
    if (!card) return 0;
    return card->is_valid;
}
