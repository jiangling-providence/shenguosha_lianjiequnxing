#include <stdio.h>
#include <string.h>
#include "render.h"
#include "heroes/hero.h"
#include "heroes/jingliu/jingliu.h"
#include "heroes/paladin/paladin.h"
//无懈响应画面在451
/* ===== 辅助：加载字体，尝试多个路径 ===== */
static TTF_Font* load_font(const char* res_dir, int size)
{
    TTF_Font* font = NULL;
    char path[512];

    /* 尝试路径列表 */
    const char* paths[] = {
        "%s/simhei.ttf",
        "%s/msyh.ttc",
        "/c/Windows/Fonts/simhei.ttf",
        "/c/Windows/Fonts/msyh.ttc",
        "/c/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        snprintf(path, sizeof(path), paths[i], res_dir);
        font = TTF_OpenFont(path, size);
        if (font) {
            printf("Loaded font: %s (size %d)\n", path, size);
            break;
        }
    }

    if (!font) {
        fprintf(stderr, "Warning: Could not load any Chinese font, using default.\n");
        fprintf(stderr, "  Put simhei.ttf or msyh.ttc into res/ folder for Chinese text.\n");
    }
    return font;
}

/* ===== 初始化 ===== */
int render_init(RenderContext* ctx, const char* res_dir)
{
    memset(ctx, 0, sizeof(RenderContext));

    /* 初始化SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return -1;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "IMG_Init failed: %s\n", IMG_GetError());
        return -1;
    }

    /* 创建窗口 */
    ctx->window = SDL_CreateWindow("KillGame - 卡牌对战",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!ctx->window) {
        fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }

    /* 创建渲染器 */
    ctx->renderer = SDL_CreateRenderer(ctx->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ctx->renderer) {
        fprintf(stderr, "CreateRenderer failed: %s\n", SDL_GetError());
        return -1;
    }

    /* 加载字体 */
    ctx->font_small  = load_font(res_dir, 14);
    ctx->font_normal = load_font(res_dir, 20);
    ctx->font_large  = load_font(res_dir, 32);

    /* 加载8个角色贴图（索引对应HeroId） */
    const char* hero_names[8] = {
        "feixiao", "zhaoyun", "gilgamesh", "linyuxia",
        "paladin", "yudie", "liuying", "jingliu"
    };
    for(int i = 0; i < 8; i++)
    {
        char hero_path[512];
        snprintf(hero_path, sizeof(hero_path), "%s/%s.png", res_dir, hero_names[i]);
        SDL_Surface* hero_surf = IMG_Load(hero_path);
        if (hero_surf) {
            ctx->hero_texture[i] = SDL_CreateTextureFromSurface(ctx->renderer, hero_surf);
            SDL_FreeSurface(hero_surf);
        }
        if (!ctx->hero_texture[i]) {
            fprintf(stderr, "Warning: Failed to load %s.png: %s\n",
                    hero_names[i], IMG_GetError());
        }
    }

    /* 颜色初始化 */
    ctx->color_white  = (SDL_Color){255, 255, 255, 255};
    ctx->color_black  = (SDL_Color){0, 0, 0, 255};
    ctx->color_red    = (SDL_Color){220, 50, 50, 255};
    ctx->color_green  = (SDL_Color){50, 180, 50, 255};
    ctx->color_blue   = (SDL_Color){50, 100, 200, 255};
    ctx->color_yellow = (SDL_Color){255, 220, 50, 255};
    ctx->color_bg     = (SDL_Color){28, 32, 38, 255};

    return 0;
}

void render_destroy(RenderContext* ctx)
{
    if (!ctx) return;
    for(int i = 0; i < 8; i++) {
        if (ctx->hero_texture[i]) SDL_DestroyTexture(ctx->hero_texture[i]);
    }
    if (ctx->font_small) TTF_CloseFont(ctx->font_small);
    if (ctx->font_normal) TTF_CloseFont(ctx->font_normal);
    if (ctx->font_large) TTF_CloseFont(ctx->font_large);
    if (ctx->renderer) SDL_DestroyRenderer(ctx->renderer);
    if (ctx->window) SDL_DestroyWindow(ctx->window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

/* ===== 文本渲染 ===== */
void render_text(RenderContext* ctx, const char* text, int x, int y,
                 TTF_Font* font, SDL_Color color)
{
    if (!ctx || !text || !font) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ctx->renderer, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }
    SDL_Rect rect = {x, y, surf->w, surf->h};
    SDL_RenderCopy(ctx->renderer, tex, NULL, &rect);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void render_text_center(RenderContext* ctx, const char* text, int cx, int y,
                        TTF_Font* font, SDL_Color color)
{
    if (!ctx || !text || !font) return;
    int w = render_text_width(ctx, text, font);
    render_text(ctx, text, cx - w / 2, y, font, color);
}

int render_text_width(RenderContext* ctx, const char* text, TTF_Font* font)
{
    if (!ctx || !text || !font) return 0;
    int w = 0;
    TTF_SizeUTF8(font, text, &w, NULL);
    return w;
}


/* ================================================================
 * 通用悬停发光函数：当鼠标移到可点击区域上时，微微发亮
 *   x, y, w, h: 区域坐标和大小
 *   mouse_x, mouse_y: 鼠标位置
 *   效果：半透明白色覆盖层 + 淡黄色边框
 * ================================================================ */
void render_hover_glow(RenderContext* ctx, int x, int y, int w, int h,
                       int mouse_x, int mouse_y)
{
    if(!ctx) return;
    SDL_Renderer* ren = ctx->renderer;
    if(!ren) return;

    /* 检查鼠标是否在区域内 */
    if(mouse_x < x || mouse_x > x + w || mouse_y < y || mouse_y > y + h)
        return;

    /* 半透明白色覆盖层（微微发亮） */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 40);
    SDL_Rect glow = {x, y, w, h};
    SDL_RenderFillRect(ren, &glow);

    /* 淡黄色边框 */
    SDL_SetRenderDrawColor(ren, 255, 255, 180, 200);
    SDL_RenderDrawRect(ren, &glow);
}

/* ================================================================
 * 通用双按钮渲染：确定（右）+ 取消（左）
 * confirm_enabled=1：确定按钮绿色亮起；=0：确定按钮灰色不可点
 * ================================================================ */
void render_dual_buttons(RenderContext* ctx, int confirm_enabled, int mouse_x, int mouse_y)
{
    if(!ctx) return;
    SDL_Renderer* ren = ctx->renderer;
    if(!ren) return;

    int btn_w = 140;
    int btn_h = 45;
    int btn_y = WINDOW_HEIGHT / 2 + 100;
    int confirm_x = WINDOW_WIDTH / 2 + 10;
    int cancel_x = WINDOW_WIDTH / 2 - btn_w - 10;

    /* 确定按钮 */
    if(confirm_enabled)
        SDL_SetRenderDrawColor(ren, 0, 150, 0, 255);  /* 绿色 */
    else
        SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);  /* 灰色 */
    SDL_Rect confirm_btn = {confirm_x, btn_y, btn_w, btn_h};
    SDL_RenderFillRect(ren, &confirm_btn);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderDrawRect(ren, &confirm_btn);
    render_text_center(ctx, "确定", confirm_x + btn_w / 2, btn_y + 10,
                       ctx->font_normal, ctx->color_white);
    /* 悬停发光 */
    render_hover_glow(ctx, confirm_x, btn_y, btn_w, btn_h, mouse_x, mouse_y);

    /* 取消按钮 */
    SDL_SetRenderDrawColor(ren, 120, 50, 50, 255);  /* 暗红色 */
    SDL_Rect cancel_btn = {cancel_x, btn_y, btn_w, btn_h};
    SDL_RenderFillRect(ren, &cancel_btn);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderDrawRect(ren, &cancel_btn);
    render_text_center(ctx, "取消", cancel_x + btn_w / 2, btn_y + 10,
                       ctx->font_normal, ctx->color_white);
    /* 悬停发光 */
    render_hover_glow(ctx, cancel_x, btn_y, btn_w, btn_h, mouse_x, mouse_y);
}

/* ===== 渲染一张卡牌 ===== */
void render_card(RenderContext* ctx, Card* card, int x, int y, int selected)
{
    if (!ctx || !card) return;

    SDL_Renderer* ren = ctx->renderer;

    /* 卡牌背景 */
    SDL_Rect rect = {x, y, CARD_WIDTH, CARD_HEIGHT};
    if (selected) {
        /* 选中：黄色边框 + 上移 */
        rect.y -= 15;
        SDL_SetRenderDrawColor(ren, 255, 220, 50, 255);
        SDL_RenderFillRect(ren, &rect);
        SDL_Rect inner = {rect.x + 3, rect.y + 3, rect.w - 6, rect.h - 6};
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderFillRect(ren, &inner);
    } else {
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderFillRect(ren, &rect);
        /* 边框 */
        SDL_SetRenderDrawColor(ren, 100, 100, 100, 255);
        SDL_RenderDrawRect(ren, &rect);
    }

    /* 花色颜色 */
    SDL_Color suit_color;
    if (card->suit == SUIT_HEART || card->suit == SUIT_DIAMOND) {
        suit_color = ctx->color_red;
    } else {
        suit_color = ctx->color_black;
    }

    /* 左上角：点数+花色 */
    char rank_suit[16];
    const char* suit_sym = "";
    switch (card->suit) {
    case SUIT_SPADE:   suit_sym = "S"; break;
    case SUIT_HEART:   suit_sym = "H"; break;
    case SUIT_CLUB:    suit_sym = "C"; break;
    case SUIT_DIAMOND: suit_sym = "D"; break;
    }
    snprintf(rank_suit, sizeof(rank_suit), "%s%s",
             card_get_rank_str(card), suit_sym);
    render_text(ctx, rank_suit, x + 4, y + (selected ? -15 : 0) + 4,
                ctx->font_small, suit_color);

    /* 中间：牌名 */
    const char* name = card_get_name(card);
    int name_y = y + (selected ? -15 : 0) + CARD_HEIGHT / 2 - 10;
    render_text_center(ctx, name, x + CARD_WIDTH / 2, name_y,
                       ctx->font_small, ctx->color_black);

    /* 右下角：点数+花色（倒过来更像真牌，这里简化直接写） */
    render_text(ctx, rank_suit, x + CARD_WIDTH - 28,
                y + (selected ? -15 : 0) + CARD_HEIGHT - 18,
                ctx->font_small, suit_color);
}

/* ===== 渲染武将 ===== */
static void render_hero(RenderContext* ctx, SDL_Texture* tex, int x, int y,
                        const char* name, int hp, int max_hp, int shield)
{
    SDL_Rect rect = {x, y, HERO_WIDTH, HERO_HEIGHT};

    if (tex) {
        SDL_RenderCopy(ctx->renderer, tex, NULL, &rect);
    } else {
        /* 没有贴图时画一个占位矩形 */
        SDL_SetRenderDrawColor(ctx->renderer, 80, 80, 120, 255);
        SDL_RenderFillRect(ctx->renderer, &rect);
    }

    /* 边框 */
    SDL_SetRenderDrawColor(ctx->renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(ctx->renderer, &rect);

    /* 名字 */
    render_text_center(ctx, name, x + HERO_WIDTH / 2, y + HERO_HEIGHT + 5,
                       ctx->font_normal, ctx->color_white);

    /* 血量 + 盾量 */
    char hp_str[64];
    if(shield > 0)
        snprintf(hp_str, sizeof(hp_str), "HP: %d/%d  盾:%d", hp, max_hp, shield);
    else
        snprintf(hp_str, sizeof(hp_str), "HP: %d/%d", hp, max_hp);
    render_text_center(ctx, hp_str, x + HERO_WIDTH / 2, y + HERO_HEIGHT + 30,
                       ctx->font_small, ctx->color_red);
}

/* ===== 渲染装备区 ===== */
static void render_equip_area(RenderContext* ctx, GameState* game, Player* p, int x, int y)
{
    /* 武器 */
    if (p->equip.weapon) {
        render_card(ctx, p->equip.weapon, x, y, 0);
        render_text(ctx, "武器", x, y - 16, ctx->font_small, ctx->color_white);
        /* 悬停发光 */
        render_hover_glow(ctx, x, y, CARD_WIDTH, CARD_HEIGHT, game->mouse_x, game->mouse_y);
    }
    /* 防具 */
    if (p->equip.armor) {
        int ax = x + CARD_WIDTH + 10;
        render_card(ctx, p->equip.armor, ax, y, 0);
        render_text(ctx, "防具", ax, y - 16, ctx->font_small, ctx->color_white);
        /* 悬停发光 */
        render_hover_glow(ctx, ax, y, CARD_WIDTH, CARD_HEIGHT, game->mouse_x, game->mouse_y);
    }
    /* 进攻马 */
    if (p->equip.horse_atk) {
        int hx = x + (CARD_WIDTH + 10) * 2;
        render_card(ctx, p->equip.horse_atk, hx, y, 0);
        render_text(ctx, "-1马", hx, y - 16, ctx->font_small, ctx->color_white);
        /* 悬停发光 */
        render_hover_glow(ctx, hx, y, CARD_WIDTH, CARD_HEIGHT, game->mouse_x, game->mouse_y);
    }
    /* 防御马 */
    if (p->equip.horse_def) {
        int hx = x + (CARD_WIDTH + 10) * 3;
        render_card(ctx, p->equip.horse_def, hx, y, 0);
        render_text(ctx, "+1马", hx, y - 16, ctx->font_small, ctx->color_white);
        /* 悬停发光 */
        render_hover_glow(ctx, hx, y, CARD_WIDTH, CARD_HEIGHT, game->mouse_x, game->mouse_y);
    }
}

/* ===== 主渲染函数 ===== */
void render_game(RenderContext* ctx, GameState* game)
{
    if (!ctx || !game) return;
    SDL_Renderer* ren = ctx->renderer;

    /* 清屏 */
    SDL_SetRenderDrawColor(ren, ctx->color_bg.r, ctx->color_bg.g,
                           ctx->color_bg.b, ctx->color_bg.a);
    SDL_RenderClear(ren);

    /* ---- 角色选择界面 ---- */
    if(game->phase == PHASE_CHARACTER_SELECT)
    {
        const char* names[8] = {
            "feixiao", "zhaoyun", "gilgamesh", "linyuxia",
            "paladin", "yudie", "liuying", "jingliu"
        };
        int cols = 4, rows = 2;
        int btn_w = 180, btn_h = 90;
        int gap_x = 25, gap_y = 25;
        int total_w = cols * btn_w + (cols-1) * gap_x;
        int start_x = (WINDOW_WIDTH - total_w) / 2;
        int start_y = (WINDOW_HEIGHT - (rows*btn_h + (rows-1)*gap_y)) / 2 + 20;

        render_text_center(ctx, "选择你的角色", WINDOW_WIDTH/2, 80,
                           ctx->font_large, ctx->color_yellow);
        render_text_center(ctx, "（点击角色选中发亮，再次点击确认选择）",
                           WINDOW_WIDTH/2, 125, ctx->font_small, ctx->color_white);

        for(int i = 0; i < 8; i++)
        {
            int row = i / cols;
            int col = i % cols;
            int x = start_x + col * (btn_w + gap_x);
            int y = start_y + row * (btn_h + gap_y);

            SDL_Rect rect = {x, y, btn_w, btn_h};
            if(game->selected_hero_idx == i)
            {
                /* 已选中：金色边框 + 亮背景 */
                SDL_SetRenderDrawColor(ren, 90, 90, 150, 255);
                SDL_RenderFillRect(ren, &rect);
                SDL_SetRenderDrawColor(ren, 255, 220, 50, 255);
                SDL_RenderDrawRect(ren, &rect);
                /* 再画一层边框增强效果 */
                SDL_Rect rect2 = {x+2, y+2, btn_w-4, btn_h-4};
                SDL_SetRenderDrawColor(ren, 255, 240, 100, 255);
                SDL_RenderDrawRect(ren, &rect2);
            }
            else if(game->select_hover == i)
            {
                /* 悬停：蓝色边框 */
                SDL_SetRenderDrawColor(ren, 70, 70, 130, 255);
                SDL_RenderFillRect(ren, &rect);
                SDL_SetRenderDrawColor(ren, 100, 150, 255, 255);
                SDL_RenderDrawRect(ren, &rect);
            }
            else
            {
                SDL_SetRenderDrawColor(ren, 45, 45, 65, 255);
                SDL_RenderFillRect(ren, &rect);
                SDL_SetRenderDrawColor(ren, 120, 120, 140, 255);
                SDL_RenderDrawRect(ren, &rect);
            }

            render_text_center(ctx, names[i], x + btn_w / 2, y + btn_h / 2 - 10,
                               ctx->font_normal, ctx->color_white);

            /* 显示角色小贴图（如果加载成功） */
            if(ctx->hero_texture[i])
            {
                SDL_Rect icon = {x + 10, y + 10, 50, 70};
                SDL_RenderCopy(ren, ctx->hero_texture[i], NULL, &icon);
            }
        }

        /* 角色选择界面：跳过后面的游戏渲染，但仍然执行 SDL_RenderPresent */
        SDL_RenderPresent(ren);
        return;
    }

    Player* enemy  = &game->players[1];  /* zhaoyun 敌人，上方 */
    Player* me     = &game->players[0];  /* feixiao 玩家，下方 */

    /* ---- 左上角：弃牌堆数量 ---- */
    char discard_str[32];
    snprintf(discard_str, sizeof(discard_str), "弃牌堆: %d", game->discard.count);
    render_text(ctx, discard_str, 20, 20, ctx->font_normal, ctx->color_white);

    /* ========== 左上角：阶段和当前玩家（轮次已移到右上角） ========== */
    const char* phase_names[] = {
        "准备阶段", "判定阶段", "摸牌阶段",
        "出牌阶段", "弃牌阶段", "结束阶段", "游戏结束"
    };
    char phase_str[128];
    snprintf(phase_str, sizeof(phase_str), "%s - %s的回合",
             phase_names[game->phase],
             game->players[game->current_player].name);
    render_text(ctx, phase_str, 20, 48, ctx->font_small, ctx->color_white);

    if(game->phase != PHASE_GAME_OVER)
    {
        if(game->resp_state != 0)
        {
            /* 所有响应已改用鼠标点击交互 */
            render_text(ctx,"等待响应（点击对应牌选中/确认打出/取消放弃）",20,68,ctx->font_small,ctx->color_yellow);
        }
        else
        {
            render_text(ctx,"按空格推进阶段 | 按R重新开始",20,68,ctx->font_small,ctx->color_white);
        }
    }

    /* ---- 右上角：牌堆数量 + 轮次 ---- */
    int deck_remain = game->deck.count - game->deck.top;
    if (deck_remain < 0) deck_remain = 0;
    char deck_str[32];
    snprintf(deck_str, sizeof(deck_str), "牌堆: %d", deck_remain);
    int deck_w = render_text_width(ctx, deck_str, ctx->font_normal);
    render_text(ctx, deck_str, WINDOW_WIDTH - deck_w - 20, 20,
                ctx->font_normal, ctx->color_white);

    /* 轮次显示在右上角，牌堆下面 */
    char turn_str[32];
    snprintf(turn_str, sizeof(turn_str), "第%d轮", game->turn_count);
    int turn_w = render_text_width(ctx, turn_str, ctx->font_normal);
    render_text(ctx, turn_str, WINDOW_WIDTH - turn_w - 20, 48,
                ctx->font_normal, ctx->color_yellow);

    /* ---- 正上方：敌方武将 + 手牌数 ---- */
    int zhaoyun_x = (WINDOW_WIDTH - HERO_WIDTH) / 2;
    int zhaoyun_y = 60;
    SDL_Texture* enemy_tex = ctx->hero_texture[enemy->hero_id];
    render_hero(ctx, enemy_tex, zhaoyun_x, zhaoyun_y,
                enemy->name, enemy->hp, enemy->max_hp, enemy->shield);

    /* 选目标状态下，合法目标高亮（绿色双边框） */
    if(game->resp_state == RESPONSE_NEED_TARGET && game->pending_card &&
       game_can_target(game, game->pending_card, 0, 1))
    {
        SDL_Rect hl1 = {zhaoyun_x - 4, zhaoyun_y - 4, HERO_WIDTH + 8, HERO_HEIGHT + 8};
        SDL_Rect hl2 = {zhaoyun_x - 2, zhaoyun_y - 2, HERO_WIDTH + 4, HERO_HEIGHT + 4};
        SDL_SetRenderDrawColor(ren, 50, 255, 50, 255);
        SDL_RenderDrawRect(ren, &hl1);
        SDL_RenderDrawRect(ren, &hl2);

        /* 悬停发光：鼠标移到合法目标头像上时微微发亮 */
        render_hover_glow(ctx, zhaoyun_x, zhaoyun_y, HERO_WIDTH, HERO_HEIGHT,
                          game->mouse_x, game->mouse_y);
    }

    /* 敌人手牌数（武将下方） */
    char hand_count_str[32];
    snprintf(hand_count_str, sizeof(hand_count_str), "手牌: %d", enemy->hand_count);
    int enemy_handtext_y = zhaoyun_y + HERO_HEIGHT + 55;
    render_text_center(ctx, hand_count_str,
                       zhaoyun_x + HERO_WIDTH / 2,
                       enemy_handtext_y,
                       ctx->font_small, ctx->color_white);

    //==================== 新增：敌方打出牌 + 响应弹窗渲染 ====================
    int resp_base_y = enemy_handtext_y + 30;  // 在“手牌:4”文字下面
    if(game->resp_trigger_card != NULL)
    {
        char show_buf[128];
        snprintf(show_buf,sizeof(show_buf),"敌方打出：%s",card_get_name(game->resp_trigger_card));
        render_text_center(ctx, show_buf, zhaoyun_x + HERO_WIDTH/2, resp_base_y, ctx->font_normal, ctx->color_white);
    }

    /* 敌人装备区（武将左侧） */
    render_equip_area(ctx, game, enemy, zhaoyun_x - 4 * (CARD_WIDTH + 10) - 20,
                      zhaoyun_y + 30);

    /* ---- 右下角：玩家武将 ---- */
    int feixiao_x = WINDOW_WIDTH - HERO_WIDTH - 40;
    int feixiao_y = WINDOW_HEIGHT - HERO_HEIGHT - 40;
    SDL_Texture* me_tex = ctx->hero_texture[me->hero_id];
    render_hero(ctx, me_tex, feixiao_x, feixiao_y,
                me->name, me->hp, me->max_hp, me->shield);

    /* 镜流专属：薨标记和形态显示 */
    if(me->hero_id == HERO_JINGLIU)
    {
        char jingliu_str[128];
        const char* form_str = "";
        if(me->jingliu.transformation == JINGLIU_FORM_DENGXIAN) form_str = "【登仙】";
        else if(me->jingliu.transformation == JINGLIU_FORM_RUMO) form_str = "【入魔】";

        snprintf(jingliu_str, sizeof(jingliu_str), "%s薨:%d", form_str, me->jingliu.hong_marks);
        render_text_center(ctx, jingliu_str,
                           feixiao_x + HERO_WIDTH / 2,
                           feixiao_y + HERO_HEIGHT + 50,
                           ctx->font_small, (SDL_Color){200, 50, 200, 255});

        /* 无罅飞光效果显示 */
        if(me->jingliu.wuxia_suit_count > 0)
        {
            char wuxia_str[64];
            snprintf(wuxia_str, sizeof(wuxia_str), "无罅飞光(花色%d)", me->jingliu.wuxia_suit_count);
            render_text_center(ctx, wuxia_str,
                               feixiao_x + HERO_WIDTH / 2,
                               feixiao_y + HERO_HEIGHT + 68,
                               ctx->font_small, (SDL_Color){255, 200, 0, 255});
        }
    }

    /* 选目标状态下，合法目标高亮（绿色双边框） */
    if(game->resp_state == RESPONSE_NEED_TARGET && game->pending_card &&
       game_can_target(game, game->pending_card, 0, 0))
    {
        SDL_Rect hl1 = {feixiao_x - 4, feixiao_y - 4, HERO_WIDTH + 8, HERO_HEIGHT + 8};
        SDL_Rect hl2 = {feixiao_x - 2, feixiao_y - 2, HERO_WIDTH + 4, HERO_HEIGHT + 4};
        SDL_SetRenderDrawColor(ren, 50, 255, 50, 255);
        SDL_RenderDrawRect(ren, &hl1);
        SDL_RenderDrawRect(ren, &hl2);
    }

    /* ---- 玩家手牌：从日志右边开始排列 ---- */
    int hand_start_x = 320;  /* 避开左下角日志区域 */
    int hand_y = feixiao_y + HERO_HEIGHT - CARD_HEIGHT + 10;

    for (int i = 0; i < me->hand_count; i++) {
        int cx = hand_start_x + i * (CARD_WIDTH + 5);
        /* 选目标状态下，待使用的牌高亮 */
        int selected = (game->resp_state == RESPONSE_NEED_TARGET &&
                        i == game->pending_hand_index) ? 1 : 0;
        render_card(ctx, me->hand[i], cx, hand_y, selected);

        /* 悬停发光：鼠标移到手牌上时微微发亮 */
        render_hover_glow(ctx, cx, hand_y, CARD_WIDTH, CARD_HEIGHT,
                          game->mouse_x, game->mouse_y);

        /* 龙胆模式：杀显示成闪，闪显示成杀（覆盖牌名） */
        if(me->hero_id == HERO_ZHAOYUN && me->longdan_active &&
           me->hand[i] && me->hand[i]->type == CARD_BASIC)
        {
            BasicType bt = me->hand[i]->sub.basic.basic_type;
            const char* cover_name = NULL;
            if(bt == BASIC_SHA) cover_name = "闪";
            else if(bt == BASIC_SHAN) cover_name = "杀";

            if(cover_name)
            {
                int name_y = hand_y + (selected ? -15 : 0) + CARD_HEIGHT / 2 - 10;
                /* 用白色矩形覆盖原牌名 */
                SDL_Rect cover_rect = {cx + 5, name_y - 2, CARD_WIDTH - 10, 18};
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_RenderFillRect(ren, &cover_rect);
                /* 显示转换后的牌名（红色表示龙胆转换） */
                render_text_center(ctx, cover_name, cx + CARD_WIDTH / 2, name_y,
                                   ctx->font_small, (SDL_Color){200, 0, 0, 255});
            }
        }
    }

    /* ---- 武将技能列表：固定放在武将插图左边，按主动/被动分组 ---- */
    if(me->hero && me->hero->skill_count > 0)
    {
        int skill_w = 100;
        int skill_h = 18;
        int skill_x = feixiao_x - skill_w - 20;  /* 武将插图左边 */
        int skill_y = feixiao_y - 80;  /* 武将插图上方 */

        /* 统计主动技能和被动技能 */
        int active_skills[10], active_count = 0;
        int passive_skills[10], passive_count = 0;
        for(int s = 0; s < me->hero->skill_count; s++)
        {
            if(me->hero->skills[s].type == SKILL_ACTIVE)
                active_skills[active_count++] = s;
            else
                passive_skills[passive_count++] = s;
        }

        int cur_y = skill_y;

        /* 主动技能组 */
        if(active_count > 0)
        {
            render_text(ctx, "【主动技】", skill_x, cur_y, ctx->font_small, ctx->color_green);
            cur_y += 18;
            for(int i = 0; i < active_count; i++)
            {
                int s = active_skills[i];
                int can_use = hero_skill_can_use(game, 0, s);
                SDL_Rect skill_rect = {skill_x, cur_y, skill_w, skill_h};

                if(can_use)
                {
                    /* 可以使用：亮起（亮绿色背景，亮绿色边框） */
                    SDL_SetRenderDrawColor(ren, 40, 100, 40, 230);
                    SDL_RenderFillRect(ren, &skill_rect);
                    SDL_SetRenderDrawColor(ren, 100, 255, 100, 255);
                    SDL_RenderDrawRect(ren, &skill_rect);
                    render_text(ctx, me->hero->skills[s].name, skill_x + 5, cur_y + 2,
                               ctx->font_small, ctx->color_white);

                    /* 悬停发光：鼠标移到可使用技能上时微微发亮 */
                    render_hover_glow(ctx, skill_x, cur_y, skill_w, skill_h,
                                      game->mouse_x, game->mouse_y);
                }
                else
                {
                    /* 不能使用或正在结算：变暗（灰色背景，灰色边框） */
                    SDL_SetRenderDrawColor(ren, 50, 50, 50, 200);
                    SDL_RenderFillRect(ren, &skill_rect);
                    SDL_SetRenderDrawColor(ren, 100, 100, 100, 255);
                    SDL_RenderDrawRect(ren, &skill_rect);
                    render_text(ctx, me->hero->skills[s].name, skill_x + 5, cur_y + 2,
                               ctx->font_small, ctx->color_white);
                }
                cur_y += skill_h + 3;
            }
            cur_y += 8;  /* 组间间距 */
        }

        /* 被动技能组（锁定技+被动技） */
        if(passive_count > 0)
        {
            render_text(ctx, "【被动技】", skill_x, cur_y, ctx->font_small, ctx->color_yellow);
            cur_y += 18;
            for(int i = 0; i < passive_count; i++)
            {
                int s = passive_skills[i];
                SDL_Rect skill_rect = {skill_x, cur_y, skill_w, skill_h};
                SDL_SetRenderDrawColor(ren, 60, 50, 20, 220);  /* 深黄背景 */
                SDL_RenderFillRect(ren, &skill_rect);
                SDL_SetRenderDrawColor(ren, 180, 160, 60, 255);  /* 黄色边框 */
                SDL_RenderDrawRect(ren, &skill_rect);
                render_text(ctx, me->hero->skills[s].name, skill_x + 5, cur_y + 2,
                           ctx->font_small, ctx->color_white);
                cur_y += skill_h + 3;
            }
        }
    }

    /* 选目标状态下，待使用的牌额外发光（绿色双层边框，更明显） */
    if(game->resp_state == RESPONSE_NEED_TARGET && game->pending_hand_index >= 0 &&
       game->pending_hand_index < me->hand_count)
    {
        int cx = hand_start_x + game->pending_hand_index * (CARD_WIDTH + 5);
        int cy = hand_y - 15;  /* render_card 里 selected 时上移15像素 */
        SDL_Rect glow1 = {cx - 5, cy - 5, CARD_WIDTH + 10, CARD_HEIGHT + 10};
        SDL_Rect glow2 = {cx - 3, cy - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
        SDL_RenderDrawRect(ren, &glow1);
        SDL_RenderDrawRect(ren, &glow2);
    }

    /* 火攻选牌阶段：高亮同花色手牌（绿色边框，提示可以弃置） */
    if(game->resp_state == RESPONSE_NEED_HUOGONG_PICK)
    {
        int has_suit_card = 0;
        for (int i = 0; i < me->hand_count; i++) {
            if(me->hand[i] && me->hand[i]->suit == game->huogong_need_suit)
            {
                has_suit_card = 1;
                int cx = hand_start_x + i * (CARD_WIDTH + 5);
                if(i == game->huogong_picked_hand)
                {
                    /* 选中的牌：金色双层高亮 */
                    SDL_Rect glow1 = {cx - 5, hand_y - 5, CARD_WIDTH + 10, CARD_HEIGHT + 10};
                    SDL_Rect glow2 = {cx - 3, hand_y - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
                    SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
                    SDL_RenderDrawRect(ren, &glow1);
                    SDL_RenderDrawRect(ren, &glow2);
                }
                else
                {
                    /* 可选的牌：绿色边框 */
                    SDL_Rect glow = {cx - 3, hand_y - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
                    SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
                    SDL_RenderDrawRect(ren, &glow);
                }
            }
        }

        /* 没有同花色手牌时的提示 */
        if(!has_suit_card)
        {
            const char* suit_names[] = {"黑桃", "红桃", "梅花", "方块"};
            char tip[128];
            snprintf(tip, sizeof(tip), "【火攻】没有%s花色手牌，无法造成伤害，请点击取消",
                     game->huogong_need_suit >= 0 ? suit_names[game->huogong_need_suit] : "");
            render_text_center(ctx, tip, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2,
                               ctx->font_normal, (SDL_Color){255, 100, 100, 255});
        }
    }

    /* 玩家装备区（手牌上方） */
    if (me->equip.weapon || me->equip.armor ||
        me->equip.horse_atk || me->equip.horse_def) {
        render_equip_area(ctx, game, me, hand_start_x, hand_y - CARD_HEIGHT - 25);
    }

    /* 贯石斧发亮效果：杀被闪后，贯石斧发动阶段 */
    if(game->resp_state == RESPONSE_NEED_GUANSHI &&
       player_weapon_type(me) == WEAPON_GUANSHI)
    {
        int gx = hand_start_x;
        int gy = hand_y - CARD_HEIGHT - 25;
        /* 多层金色边框模拟发光 */
        SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
        SDL_Rect glow1 = {gx - 4, gy - 4, CARD_WIDTH + 8, CARD_HEIGHT + 8};
        SDL_RenderDrawRect(ren, &glow1);
        SDL_Rect glow2 = {gx - 2, gy - 2, CARD_WIDTH + 4, CARD_HEIGHT + 4};
        SDL_RenderDrawRect(ren, &glow2);
        /* 提示文字 */
        render_text(ctx, "点击贯石斧发动（弃2牌强制命中）", gx, gy - 35,
                    ctx->font_small, ctx->color_yellow);
    }

    /* 寒冰剑发亮效果：杀造成伤害时，寒冰剑发动阶段 */
    if(game->resp_state == RESPONSE_NEED_HANBING &&
       player_weapon_type(me) == WEAPON_HANBING)
    {
        int hx = hand_start_x;
        int hy = hand_y - CARD_HEIGHT - 25;
        /* 多层青色边框模拟寒冰发光 */
        SDL_SetRenderDrawColor(ren, 0, 200, 255, 255);
        SDL_Rect glow1 = {hx - 4, hy - 4, CARD_WIDTH + 8, CARD_HEIGHT + 8};
        SDL_RenderDrawRect(ren, &glow1);
        SDL_Rect glow2 = {hx - 2, hy - 2, CARD_WIDTH + 4, CARD_HEIGHT + 4};
        SDL_RenderDrawRect(ren, &glow2);
        /* 提示文字 */
        if(!game->hanbing_picking)
            render_text(ctx, "点击寒冰剑发动（弃对方2牌免伤）", hx, hy - 35,
                        ctx->font_small, (SDL_Color){0, 200, 255, 255});
        else
            render_text(ctx, "请选择对方的2张牌（手牌或装备）", hx, hy - 35,
                        ctx->font_small, (SDL_Color){0, 200, 255, 255});
    }

    /* 寒冰剑选牌阶段：显示对方手牌（牌面朝上） */
    if(game->resp_state == RESPONSE_NEED_HANBING && game->hanbing_picking)
    {
        Player* enemy = &game->players[1];
        int zhaoyun_x = (WINDOW_WIDTH - HERO_WIDTH) / 2;
        int enemy_hand_y = 60 + HERO_HEIGHT + 85;
        int total_width = enemy->hand_count * (CARD_WIDTH + 5);
        int enemy_hand_start_x = zhaoyun_x + HERO_WIDTH / 2 - total_width / 2;

        render_text_center(ctx, "对方手牌（点击选择）",
                           zhaoyun_x + HERO_WIDTH / 2, enemy_hand_y - 20,
                           ctx->font_small, (SDL_Color){0, 200, 255, 255});

        for(int i = 0; i < enemy->hand_count; i++)
        {
            render_card(ctx, enemy->hand[i],
                        enemy_hand_start_x + i * (CARD_WIDTH + 5), enemy_hand_y, 0);
        }
    }

    /* 寒冰剑选牌阶段：选中的牌发亮 */
    if(game->resp_state == RESPONSE_NEED_HANBING && game->hanbing_picking)
    {
        Player* enemy = &game->players[1];
        int zhaoyun_x = (WINDOW_WIDTH - HERO_WIDTH) / 2;
        int enemy_hand_y = 60 + HERO_HEIGHT + 85;
        int total_width = enemy->hand_count * (CARD_WIDTH + 5);
        int enemy_hand_start_x = zhaoyun_x + HERO_WIDTH / 2 - total_width / 2;
        int equip_x = zhaoyun_x - 4 * (CARD_WIDTH + 10) - 20;
        int equip_y = 60 + 30;

        for(int i = 0; i < game->hanbing_picked_count; i++)
        {
            int ctype = game->hanbing_picked_type[i];
            int cidx = game->hanbing_picked_index[i];
            int cx = 0, cy = 0;

            if(ctype == 0)  /* 手牌 */
            {
                cx = enemy_hand_start_x + cidx * (CARD_WIDTH + 5);
                cy = enemy_hand_y;
            }
            else if(ctype >= 1 && ctype <= 4)  /* 装备 */
            {
                cx = equip_x + (ctype - 1) * (CARD_WIDTH + 10);
                cy = equip_y;
            }

            SDL_SetRenderDrawColor(ren, 0, 200, 255, 255);
            SDL_Rect sel = {cx - 3, cy - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
            SDL_RenderDrawRect(ren, &sel);
        }
    }

    /* 通用响应：响应牌发亮 + 取消/确认按钮（无懈可击/杀/闪） */
    if((game->resp_state == RESPONSE_NEED_WUXIE || game->resp_state == RESPONSE_NEED_BASIC ||
        game->resp_state == RESPONSE_NEED_MULTI_WUXIE) &&
       (game->resp_target_player == 0 ||
        (game->resp_state == RESPONSE_NEED_BASIC && game->duel_turn == 0) ||
        (game->resp_state == RESPONSE_NEED_MULTI_WUXIE && game->multi_wuxie_stack_depth > 0 &&
         game->multi_wuxie_stack[game->multi_wuxie_stack_depth - 1].asker_idx == 0)))
    {
        Player* me = &game->players[0];

        /* 获取响应牌名 */
        const char* resp_card_name = "";
        if(game->resp_state == RESPONSE_NEED_WUXIE || game->resp_state == RESPONSE_NEED_MULTI_WUXIE)
            resp_card_name = "无懈可击";
        else if(game->resp_required_basic == BASIC_SHA)
            resp_card_name = "杀";
        else if(game->resp_required_basic == BASIC_SHAN)
            resp_card_name = "闪";

        /* 响应牌：不自动高亮所有可响应牌，只有选中的牌才高亮（绿色边框）
         * 鼠标悬停时用通用悬停发光提示可点击 */
        if(game->response_pick_selected && game->response_pick_index >= 0 &&
           game->response_pick_index < me->hand_count)
        {
            int i = game->response_pick_index;
            int cx = hand_start_x + i * (CARD_WIDTH + 5);
            int cy = hand_y;
            SDL_SetRenderDrawColor(ren, 50, 200, 50, 255);
            SDL_Rect glow1 = {cx - 4, cy - 4, CARD_WIDTH + 8, CARD_HEIGHT + 8};
            SDL_RenderDrawRect(ren, &glow1);
            SDL_Rect glow2 = {cx - 2, cy - 2, CARD_WIDTH + 4, CARD_HEIGHT + 4};
            SDL_RenderDrawRect(ren, &glow2);
        }

        /* 提示文字 */
        char tip_buf[128];
        if(game->resp_state == RESPONSE_NEED_MULTI_WUXIE)
        {
            int trick_target = game->multi_wuxie_targets[game->multi_wuxie_current_target];
            if(game->multi_wuxie_stack_depth > 1)
                snprintf(tip_buf, sizeof(tip_buf), "反无懈：点击【无懈可击】选中打出，点击取消放弃");
            else
                snprintf(tip_buf, sizeof(tip_buf), "对 %s 使用无懈可击？点击【无懈可击】选中，点击确定打出，点击取消放弃",
                         game->players[trick_target].name);
        }
        else
        {
            snprintf(tip_buf, sizeof(tip_buf), "点击【%s】选中，点击确定打出，点击取消放弃", resp_card_name);
        }
        render_text_center(ctx, tip_buf,
                           WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                           ctx->font_small, ctx->color_yellow);

        /* 倒计时渲染 */
        if(game->countdown.active)
        {
            char cd_buf[32];
            snprintf(cd_buf, sizeof(cd_buf), "剩余时间：%.1f秒", game->countdown.remaining);
            render_text_center(ctx, cd_buf,
                               WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 85,
                               ctx->font_normal, ctx->color_red);
        }

        /* 确认/取消按钮（在倒计时下方） */
        if(game->resp_state == RESPONSE_NEED_MULTI_WUXIE)
        {
            int btn_w = 140;
            int btn_h = 45;
            int btn_y = WINDOW_HEIGHT / 2 + 100;
            int confirm_x = WINDOW_WIDTH / 2 + 10;
            int cancel_x = WINDOW_WIDTH / 2 - btn_w - 10;

            /* 取消按钮（始终显示） */
            SDL_SetRenderDrawColor(ren, 120, 50, 50, 255);
            SDL_Rect cancel_btn = {cancel_x, btn_y, btn_w, btn_h};
            SDL_RenderFillRect(ren, &cancel_btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &cancel_btn);
            render_text_center(ctx, "取消", cancel_x + btn_w / 2, btn_y + 10,
                               ctx->font_normal, ctx->color_white);
            render_hover_glow(ctx, cancel_x, btn_y, btn_w, btn_h, game->mouse_x, game->mouse_y);

            /* 确定按钮（只有选中无懈可击后才亮起） */
            if(game->response_pick_selected == 1)
            {
                SDL_SetRenderDrawColor(ren, 0, 150, 0, 255);
            }
            else
            {
                SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
            }
            SDL_Rect confirm_btn = {confirm_x, btn_y, btn_w, btn_h};
            SDL_RenderFillRect(ren, &confirm_btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &confirm_btn);
            render_text_center(ctx, "确定", confirm_x + btn_w / 2, btn_y + 10,
                               ctx->font_normal, ctx->color_white);
            if(game->response_pick_selected == 1)
                render_hover_glow(ctx, confirm_x, btn_y, btn_w, btn_h, game->mouse_x, game->mouse_y);
        }
        else
        {
            /* 单目标响应（杀/闪/无懈可击）：确认/取消按钮
             * 确认按钮只有选中牌后才亮起 */
            render_dual_buttons(ctx, game->response_pick_selected == 1,
                                game->mouse_x, game->mouse_y);
        }
    }

    /* ---- 右侧中央：最近打出的牌 ---- */
    if(game->central_show_card)
    {
        int cx = WINDOW_WIDTH - CARD_WIDTH - 30;
        int cy = WINDOW_HEIGHT / 2 - CARD_HEIGHT / 2;
        render_card(ctx, game->central_show_card, cx, cy, 0);
        render_text(ctx, "最近打出", cx, cy - 18, ctx->font_small, ctx->color_yellow);
    }

    /* ---- 屏幕中心：通用展示牌 ---- */
    if(game->show_card_center)
    {
        int cx = WINDOW_WIDTH / 2 - CARD_WIDTH / 2;
        int cy = WINDOW_HEIGHT / 2 - CARD_HEIGHT / 2 - 30;

        /* 半透明黑色背景 */
        SDL_Rect bg = {cx - 30, cy - 50, CARD_WIDTH + 60, CARD_HEIGHT + 90};
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
        SDL_RenderFillRect(ren, &bg);
        SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
        SDL_RenderDrawRect(ren, &bg);

        /* 展示者名字 */
        char who_text[64];
        snprintf(who_text, sizeof(who_text), "%s 展示", game->show_card_who);
        render_text_center(ctx, who_text, cx + CARD_WIDTH / 2, cy - 30,
                           ctx->font_normal, ctx->color_yellow);

        /* 展示的牌 */
        render_card(ctx, game->show_card_center, cx, cy, 0);

        /* 剩余时间条 */
        int timer_w = (int)((float)game->show_card_timer / 180.0f * (CARD_WIDTH + 40));
        SDL_Rect timer_bg = {cx - 20, cy + CARD_HEIGHT + 15, CARD_WIDTH + 40, 8};
        SDL_Rect timer_fg = {cx - 20, cy + CARD_HEIGHT + 15, timer_w, 8};
        SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
        SDL_RenderFillRect(ren, &timer_bg);
        SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
        SDL_RenderFillRect(ren, &timer_fg);
    }

    /* ---- 屏幕中心：通用文字提示 ---- */
    if(game->center_message[0] != '\0')
    {
        int msg_w = 400;
        int msg_h = 80;
        int msg_x = WINDOW_WIDTH / 2 - msg_w / 2;
        int msg_y = WINDOW_HEIGHT / 2 - msg_h / 2;

        /* 半透明黑色背景 */
        SDL_Rect bg = {msg_x - 20, msg_y - 20, msg_w + 40, msg_h + 40};
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
        SDL_RenderFillRect(ren, &bg);
        SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
        SDL_RenderDrawRect(ren, &bg);

        /* 提示文字（居中，大字体） */
        render_text_center(ctx, game->center_message, WINDOW_WIDTH / 2, msg_y + 15,
                           ctx->font_normal, ctx->color_yellow);

        /* 剩余时间条 */
        int timer_w = (int)((float)game->center_message_timer / 180.0f * msg_w);
        SDL_Rect timer_bg = {msg_x, msg_y + msg_h - 10, msg_w, 6};
        SDL_Rect timer_fg = {msg_x, msg_y + msg_h - 10, timer_w, 6};
        SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
        SDL_RenderFillRect(ren, &timer_bg);
        SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
        SDL_RenderFillRect(ren, &timer_fg);
    }

    /* ---- 屏幕中心：批量展示牌（飞舞/化蝶亮出的牌） ---- */
    if(game->show_cards_count > 0)
    {
        int count = game->show_cards_count;
        int card_w = CARD_WIDTH;
        int card_h = CARD_HEIGHT;
        int gap = 10;
        int total_w = count * card_w + (count - 1) * gap;
        int start_x = WINDOW_WIDTH / 2 - total_w / 2;
        int start_y = WINDOW_HEIGHT / 2 - card_h / 2 - 20;

        /* 半透明黑色背景 */
        int bg_pad = 30;
        SDL_Rect bg = {start_x - bg_pad, start_y - bg_pad, total_w + bg_pad * 2, card_h + bg_pad * 2 + 50};
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
        SDL_RenderFillRect(ren, &bg);
        SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
        SDL_RenderDrawRect(ren, &bg);

        /* 展示者名字 */
        char who_text[64];
        snprintf(who_text, sizeof(who_text), "%s 亮出%d张牌", game->show_cards_who, count);
        render_text_center(ctx, who_text, WINDOW_WIDTH / 2, start_y - 25,
                           ctx->font_normal, ctx->color_yellow);

        /* 展示的牌 */
        for(int i = 0; i < count; i++)
        {
            if(game->show_cards_center[i])
            {
                int cx = start_x + i * (card_w + gap);
                render_card(ctx, game->show_cards_center[i], cx, start_y, 0);
            }
        }

        /* 剩余时间条 */
        int total_frames = game->show_cards_total > 0 ? game->show_cards_total : count * 60;
        int timer_w2 = (int)((float)game->show_cards_timer / (float)total_frames * total_w);
        SDL_Rect timer_bg2 = {start_x, start_y + card_h + 15, total_w, 8};
        SDL_Rect timer_fg2 = {start_x, start_y + card_h + 15, timer_w2, 8};
        SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
        SDL_RenderFillRect(ren, &timer_bg2);
        SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
        SDL_RenderFillRect(ren, &timer_fg2);
    }

    /* ---- 过河拆桥/顺手牵羊：选择对方的一张牌 ---- */
    if(game->resp_state == RESPONSE_NEED_PICK_ENEMY_CARD)
    {
        Player* enemy = &game->players[1];
        int zhaoyun_x = (WINDOW_WIDTH - HERO_WIDTH) / 2;
        int zhaoyun_y = 60;

        /* 提示文字 */
        const char* action_name = (game->pick_enemy_action == 0) ? "过河拆桥" : "顺手牵羊";
        render_text_center(ctx, "【选择要弃置的牌】再次点击确认，点击取消按钮取消",
                           WINDOW_WIDTH / 2, 120,
                           ctx->font_normal, ctx->color_yellow);

        /* 对方手牌（暗置牌背）- 渲染在武将下方 */
        int enemy_hand_y = zhaoyun_y + HERO_HEIGHT + 85;
        int total_width = enemy->hand_count * (CARD_WIDTH + 5);
        int enemy_hand_start_x = zhaoyun_x + HERO_WIDTH / 2 - total_width / 2;

        render_text_center(ctx, "对方手牌（暗置）",
                           zhaoyun_x + HERO_WIDTH / 2, enemy_hand_y - 20,
                           ctx->font_small, ctx->color_white);

        for(int i = 0; i < enemy->hand_count; i++)
        {
            int cx = enemy_hand_start_x + i * (CARD_WIDTH + 5);
            /* 渲染牌背（暗置） */
            SDL_SetRenderDrawColor(ren, 40, 40, 80, 255);
            SDL_Rect back = {cx, enemy_hand_y, CARD_WIDTH, CARD_HEIGHT};
            SDL_RenderFillRect(ren, &back);
            SDL_SetRenderDrawColor(ren, 100, 100, 150, 255);
            SDL_RenderDrawRect(ren, &back);
            /* 牌背图案 */
            render_text_center(ctx, "?", cx + CARD_WIDTH / 2, enemy_hand_y + CARD_HEIGHT / 2 - 10,
                               ctx->font_large, (SDL_Color){150, 150, 200, 255});

            /* 选中的手牌发亮 */
            if(game->pick_enemy_card_type == 0 && game->pick_enemy_card_index == i)
            {
                SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
                SDL_Rect sel = {cx - 3, enemy_hand_y - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
                SDL_RenderDrawRect(ren, &sel);
            }
        }

        /* 对方延时锦囊区（明置牌面）- 渲染在武将右侧 */
        int judge_x = zhaoyun_x + HERO_WIDTH + 20;
        int judge_y = zhaoyun_y + 30;

        if(enemy->judge.count > 0)
        {
            render_text(ctx, "判定区", judge_x, judge_y - 18, ctx->font_small, ctx->color_white);
            for(int i = 0; i < enemy->judge.count; i++)
            {
                if(!enemy->judge.cards[i]) continue;
                int cx = judge_x + i * (CARD_WIDTH + 10);
                render_card(ctx, enemy->judge.cards[i], cx, judge_y, 0);

                /* 选中的延时锦囊发亮 */
                if(game->pick_enemy_card_type == 5 && game->pick_enemy_card_index == i)
                {
                    SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
                    SDL_Rect sel = {cx - 3, judge_y - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
                    SDL_RenderDrawRect(ren, &sel);
                }
            }
        }

        /* 对方装备区选中的牌发亮（装备区已有渲染，这里只加选中高亮） */
        int equip_x = zhaoyun_x - 4 * (CARD_WIDTH + 10) - 20;
        int equip_y = zhaoyun_y + 30;
        Card* equips[4] = {enemy->equip.weapon, enemy->equip.armor,
                            enemy->equip.horse_atk, enemy->equip.horse_def};
        int equip_types[4] = {1, 2, 3, 4};
        for(int i = 0; i < 4; i++)
        {
            if(!equips[i]) continue;
            if(game->pick_enemy_card_type == equip_types[i])
            {
                int cx = equip_x + i * (CARD_WIDTH + 10);
                SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
                SDL_Rect sel = {cx - 3, equip_y - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
                SDL_RenderDrawRect(ren, &sel);
            }
        }
    }

    /* ---- 五谷丰登：亮出的牌 ---- */
    if(game->group_active && game->group_trick_type == TRICK_WUGU &&
       game->group_wugu_count > 0)
    {
        int count = game->group_wugu_count;
        int start_x = WINDOW_WIDTH/2 - (count*(CARD_WIDTH+10))/2;
        int wugu_y = 140;
        if(game->resp_state == RESPONSE_NEED_WUGU_PICK)
        {
            render_text_center(ctx, "五谷丰登 - 点击选择一张牌",
                               WINDOW_WIDTH/2, wugu_y - 22,
                               ctx->font_normal, ctx->color_yellow);
        }
        else
        {
            render_text_center(ctx, "五谷丰登 - 亮出的牌",
                               WINDOW_WIDTH/2, wugu_y - 22,
                               ctx->font_small, ctx->color_white);
        }
        for(int i=0;i<count;i++)
        {
            render_card(ctx, game->group_wugu_pile[i],
                        start_x + i*(CARD_WIDTH+10), wugu_y, 0);
        }
    }

    /* ---- 朱雀羽扇：打出杀时，选择是否将杀变成火杀 ---- */
    if(game->resp_state == RESPONSE_NEED_ZHUQUE && game->zhuque_active)
    {
        Player* me = &game->players[0];

        /* 朱雀羽扇发亮（红橙色边框，火属性） */
        if(player_weapon_type(me) == WEAPON_ZHUQUE)
        {
            int wx = hand_start_x;
            int wy = hand_y - CARD_HEIGHT - 25;
            /* 多层红橙色边框模拟火焰发光 */
            SDL_SetRenderDrawColor(ren, 255, 100, 0, 255);
            SDL_Rect glow1 = {wx - 4, wy - 4, CARD_WIDTH + 8, CARD_HEIGHT + 8};
            SDL_RenderDrawRect(ren, &glow1);
            SDL_Rect glow2 = {wx - 2, wy - 2, CARD_WIDTH + 4, CARD_HEIGHT + 4};
            SDL_RenderDrawRect(ren, &glow2);
            /* 选中时额外高亮 */
            if(game->zhuque_selected)
            {
                SDL_SetRenderDrawColor(ren, 255, 200, 0, 255);
                SDL_Rect sel = {wx - 5, wy - 5, CARD_WIDTH + 10, CARD_HEIGHT + 10};
                SDL_RenderDrawRect(ren, &sel);
            }
        }

        /* 提示文字 */
        if(game->zhuque_selected == 0)
            render_text_center(ctx, "【朱雀羽扇】点击朱雀羽扇选中，将杀变成火杀；点击取消不发动",
                               WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                               ctx->font_small, (SDL_Color){255, 150, 0, 255});
        else
            render_text_center(ctx, "【朱雀羽扇】已选中，点击确定将杀变成火杀，或点击取消取消选中",
                               WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                               ctx->font_small, (SDL_Color){255, 200, 0, 255});
    }

    /* ---- 丈八蛇矛：出牌阶段常亮 + 选牌模式 ---- */
    {
        Player* me = &game->players[0];
        int zhangba_equipped = (player_weapon_type(me) == WEAPON_ZHANGBA);

        if(zhangba_equipped)
        {
            int wx = hand_start_x;
            int wy = hand_y - CARD_HEIGHT - 25;

            if(game->resp_state == RESPONSE_NEED_ZHANGBA && game->zhangba_active)
            {
                /* 选牌模式：丈八变暗（不发光） */

                /* 选中的手牌发亮 */
                for(int i = 0; i < game->zhangba_selected_count; i++)
                {
                    int sel_idx = game->zhangba_selected[i];
                    if(sel_idx >= 0 && sel_idx < me->hand_count)
                    {
                        int cx = hand_start_x + sel_idx * (CARD_WIDTH + 5);
                        SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);  /* 金色 */
                        SDL_Rect glow = {cx - 3, hand_y - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
                        SDL_RenderDrawRect(ren, &glow);
                        SDL_Rect glow2 = {cx - 5, hand_y - 5, CARD_WIDTH + 10, CARD_HEIGHT + 10};
                        SDL_RenderDrawRect(ren, &glow2);
                    }
                }

                /* 提示文字 */
                if(game->zhangba_selected_count == 2)
                    render_text_center(ctx, "【丈八蛇矛】已选择2张牌，点击确定当杀打出，或点击手牌取消选择",
                                       WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                                       ctx->font_small, (SDL_Color){255, 215, 0, 255});
                else
                    render_text_center(ctx, "【丈八蛇矛】请选择两张手牌（点击选中/取消），点击取消退出",
                                       WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                                       ctx->font_small, (SDL_Color){255, 215, 0, 255});
            }
            else if(game->phase == PHASE_PLAY && game->current_player == 0 &&
                    game->resp_state == RESPONSE_NONE && !game->game_over)
            {
                /* 出牌阶段：丈八常亮（金色边框） */
                SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);  /* 金色 */
                SDL_Rect glow1 = {wx - 3, wy - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
                SDL_RenderDrawRect(ren, &glow1);
                SDL_Rect glow2 = {wx - 5, wy - 5, CARD_WIDTH + 10, CARD_HEIGHT + 10};
                SDL_RenderDrawRect(ren, &glow2);
            }
        }
    }

    /* ---- 雨蝶飞舞：选择要置入装备区的手牌 ---- */
    if(game->resp_state == RESPONSE_NEED_FEIWUU_PICK)
    {
        Player* me = &game->players[0];
        int hand_start_x = 320;  /* 和普通手牌渲染保持一致 */
        int hand_y = WINDOW_HEIGHT - HERO_HEIGHT - 40 + HERO_HEIGHT - CARD_HEIGHT + 10;

        /* 选中的手牌发亮（金色双层边框） */
        for(int i = 0; i < game->feiwuu_selected_count; i++)
        {
            int sel_idx = game->feiwuu_selected[i];
            if(sel_idx >= 0 && sel_idx < me->hand_count)
            {
                int cx = hand_start_x + sel_idx * (CARD_WIDTH + 5);
                SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);  /* 金色 */
                SDL_Rect glow1 = {cx - 3, hand_y - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
                SDL_RenderDrawRect(ren, &glow1);
                SDL_Rect glow2 = {cx - 5, hand_y - 5, CARD_WIDTH + 10, CARD_HEIGHT + 10};
                SDL_RenderDrawRect(ren, &glow2);
            }
        }

        /* 提示文字（区分化蝶和飞舞） */
        Player* yudie_p = &game->players[0];
        int is_huadie = yudie_p->yudie.huadie_active;
        int max_cards = is_huadie ? 1 : 4;
        const char* skill_name = is_huadie ? "化蝶" : "飞舞";
        char tip[128];
        if(is_huadie)
            snprintf(tip, sizeof(tip), "【%s】选择0或1张牌置入装备区（已选%d张），0张则直接亮牌获得，点击确定，点击取消退出",
                     skill_name, game->feiwuu_selected_count);
        else
            snprintf(tip, sizeof(tip), "【%s】已选择%d张手牌（0-%d张），点击手牌选中/取消，点击确定置入装备区，点击取消退出",
                     skill_name, game->feiwuu_selected_count, max_cards);
        render_text_center(ctx, tip, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                           ctx->font_small, (SDL_Color){255, 215, 0, 255});
    }

    /* ---- 通用主动弃牌：选择要弃置的手牌 ---- */
    if(game->resp_state == RESPONSE_NEED_GENERIC_DISCARD)
    {
        Player* me = &game->players[game->generic_discard_player];
        int hand_start_x = 320;  /* 和普通手牌渲染保持一致 */
        int hand_y = WINDOW_HEIGHT - HERO_HEIGHT - 40 + HERO_HEIGHT - CARD_HEIGHT + 10;

        /* 选中的手牌发亮（金色双层边框） */
        for(int i = 0; i < game->generic_discard_selected_count; i++)
        {
            int sel_idx = game->generic_discard_selected[i];
            if(sel_idx >= 0 && sel_idx < me->hand_count)
            {
                int cx = hand_start_x + sel_idx * (CARD_WIDTH + 5);
                SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);  /* 金色 */
                SDL_Rect glow1 = {cx - 3, hand_y - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
                SDL_RenderDrawRect(ren, &glow1);
                SDL_Rect glow2 = {cx - 5, hand_y - 5, CARD_WIDTH + 10, CARD_HEIGHT + 10};
                SDL_RenderDrawRect(ren, &glow2);
            }
        }

        /* 双按钮：确定（右，选满时亮起）+ 取消（左） */
        int can_confirm = (game->generic_discard_selected_count == game->generic_discard_need);

        /* 提示文字 */
        char tip[128];
        snprintf(tip, sizeof(tip), "【弃牌】已选择%d/%d张手牌，点击手牌选中/取消，选满后点击确定弃牌，点击取消退出",
                 game->generic_discard_selected_count, game->generic_discard_need);
        render_text_center(ctx, tip, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                           ctx->font_small, (SDL_Color){255, 215, 0, 255});
    }

    /* ---- 确认出牌：选完目标后，点击确定才打出 ---- */
    if(game->resp_state == RESPONSE_NEED_CONFIRM_PLAY)
    {
        Player* me = &game->players[0];
        int hand_start_x = 320;  /* 和普通手牌渲染保持一致 */
        int hand_y = WINDOW_HEIGHT - HERO_HEIGHT - 40 + HERO_HEIGHT - CARD_HEIGHT + 10;

        /* 待打出的手牌高亮（金色双层边框） */
        if(game->confirm_play_hand_index >= 0 && game->confirm_play_hand_index < me->hand_count)
        {
            int cx = hand_start_x + game->confirm_play_hand_index * (CARD_WIDTH + 5);
            SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);  /* 金色 */
            SDL_Rect glow1 = {cx - 3, hand_y - 3, CARD_WIDTH + 6, CARD_HEIGHT + 6};
            SDL_RenderDrawRect(ren, &glow1);
            SDL_Rect glow2 = {cx - 5, hand_y - 5, CARD_WIDTH + 10, CARD_HEIGHT + 10};
            SDL_RenderDrawRect(ren, &glow2);
        }

        /* 目标高亮（如果有目标） */
        if(game->confirm_play_target_index >= 0)
        {
            int target_idx = game->confirm_play_target_index;
            int tx, ty;
            if(target_idx == 0)
            {
                tx = WINDOW_WIDTH - HERO_WIDTH - 40;
                ty = WINDOW_HEIGHT - HERO_HEIGHT - 40;
            }
            else
            {
                tx = (WINDOW_WIDTH - HERO_WIDTH) / 2;
                ty = 60;
            }
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 255);  /* 红色 */
            SDL_Rect target_glow = {tx - 5, ty - 5, HERO_WIDTH + 10, HERO_HEIGHT + 10};
            SDL_RenderDrawRect(ren, &target_glow);
        }

        /* 提示文字 */
        const char* card_name = "";
        if(game->confirm_play_hand_index >= 0 && game->confirm_play_hand_index < me->hand_count)
            card_name = card_get_name(me->hand[game->confirm_play_hand_index]);
        if(game->confirm_play_target_index >= 0)
        {
            char tip[128];
            snprintf(tip, sizeof(tip), "点击确定对【%s】使用【%s】，点击取消取消",
                     game->players[game->confirm_play_target_index].name, card_name);
            render_text_center(ctx, tip, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                               ctx->font_small, (SDL_Color){255, 215, 0, 255});
        }
        else
        {
            char tip[128];
            snprintf(tip, sizeof(tip), "点击确定使用【%s】，点击取消取消", card_name);
            render_text_center(ctx, tip, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                               ctx->font_small, (SDL_Color){255, 215, 0, 255});
        }
    }

    /* ---- 铁索连环选目标：选择1-2名角色，改变横置状态 ---- */
    if(game->resp_state == RESPONSE_NEED_TIESUO_TARGET)
    {
        /* 选中的目标头像高亮（绿色双层边框） */
        for(int i = 0; i < game->tiesuo_target_count; i++)
        {
            int idx = game->tiesuo_targets[i];
            if(idx < 0 || idx >= game->player_count) continue;
            int tx, ty;
            if(idx == 0)
            {
                tx = WINDOW_WIDTH - HERO_WIDTH - 40;
                ty = WINDOW_HEIGHT - HERO_HEIGHT - 40;
            }
            else
            {
                tx = (WINDOW_WIDTH - HERO_WIDTH) / 2;
                ty = 60;
            }
            SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);  /* 绿色 */
            SDL_Rect glow1 = {tx - 5, ty - 5, HERO_WIDTH + 10, HERO_HEIGHT + 10};
            SDL_Rect glow2 = {tx - 3, ty - 3, HERO_WIDTH + 6, HERO_HEIGHT + 6};
            SDL_RenderDrawRect(ren, &glow1);
            SDL_RenderDrawRect(ren, &glow2);
        }

        /* 提示文字（屏幕中心下方） */
        int btn_y = WINDOW_HEIGHT / 2 + 100;
        char tip[128];
        snprintf(tip, sizeof(tip), "【铁索连环】已选择%d/2名角色（点击头像选中/取消，点确定生效，点取消取消）",
                 game->tiesuo_target_count);
        render_text_center(ctx, tip, WINDOW_WIDTH / 2, btn_y,
                           ctx->font_small, (SDL_Color){255, 215, 0, 255});
    }

    /* ---- 通用多目标选择：选1~N个目标 ---- */
    if(game->resp_state == RESPONSE_NEED_MULTI_TARGET)
    {
        /* 选中的目标头像高亮（绿色双层边框） */
        for(int i = 0; i < game->multi_target_count; i++)
        {
            int idx = game->multi_targets[i];
            if(idx < 0 || idx >= game->player_count) continue;
            int tx, ty;
            if(idx == 0)
            {
                tx = WINDOW_WIDTH - HERO_WIDTH - 40;
                ty = WINDOW_HEIGHT - HERO_HEIGHT - 40;
            }
            else
            {
                tx = (WINDOW_WIDTH - HERO_WIDTH) / 2;
                ty = 60;
            }
            SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
            SDL_Rect glow1 = {tx - 5, ty - 5, HERO_WIDTH + 10, HERO_HEIGHT + 10};
            SDL_Rect glow2 = {tx - 3, ty - 3, HERO_WIDTH + 6, HERO_HEIGHT + 6};
            SDL_RenderDrawRect(ren, &glow1);
            SDL_RenderDrawRect(ren, &glow2);
        }

        /* 提示文字（上移，避免和按钮重叠） */
        int tip_y = WINDOW_HEIGHT / 2 + 60;
        char tip[128];
        snprintf(tip, sizeof(tip), "【多目标】已选择%d/%d名角色（至少选%d个，点击头像选中/取消）",
                 game->multi_target_count, game->multi_target_max, game->multi_target_min);
        render_text_center(ctx, tip, WINDOW_WIDTH / 2, tip_y,
                           ctx->font_small, (SDL_Color){255, 215, 0, 255});

        /* 确定/取消按钮：选满最少目标数后确定按钮亮起 */
        int confirm_enabled = (game->multi_target_count >= game->multi_target_min);
        render_dual_buttons(ctx, confirm_enabled, game->mouse_x, game->mouse_y);
    }

    /* ---- 登仙牌型转换选择界面 ---- */
    if(game->resp_state == RESPONSE_NEED_DENGXIAN_CONVERT)
    {
        int cx = WINDOW_WIDTH / 2;
        int cy = WINDOW_HEIGHT / 2;
        int btn_w = 180;
        int btn_h = 50;
        int gap = 20;

        /* 半透明背景 */
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
        SDL_Rect bg = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
        SDL_RenderFillRect(ren, &bg);

        /* 标题 */
        render_text_center(ctx, "【登仙】请选择牌的使用方式",
                           cx, cy - 60, ctx->font_normal, ctx->color_yellow);

        /* 按钮1：当原牌 */
        SDL_Rect btn1 = {cx - btn_w - gap/2, cy, btn_w, btn_h};
        SDL_SetRenderDrawColor(ren, 60, 60, 80, 255);
        SDL_RenderFillRect(ren, &btn1);
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDrawRect(ren, &btn1);
        render_text_center(ctx, "当原牌使用", cx - btn_w/2 - gap/2, cy + 15,
                           ctx->font_small, ctx->color_white);

        /* 按钮2：当桃 */
        SDL_Rect btn2 = {cx - btn_w/2, cy, btn_w, btn_h};
        SDL_SetRenderDrawColor(ren, 80, 40, 40, 255);
        SDL_RenderFillRect(ren, &btn2);
        SDL_SetRenderDrawColor(ren, 255, 100, 100, 255);
        SDL_RenderDrawRect(ren, &btn2);
        render_text_center(ctx, "当桃使用(回血)", cx, cy + 15,
                           ctx->font_small, ctx->color_white);

        /* 按钮3：当桃园结义 */
        SDL_Rect btn3 = {cx + gap/2, cy, btn_w, btn_h};
        SDL_SetRenderDrawColor(ren, 40, 80, 40, 255);
        SDL_RenderFillRect(ren, &btn3);
        SDL_SetRenderDrawColor(ren, 100, 255, 100, 255);
        SDL_RenderDrawRect(ren, &btn3);
        render_text_center(ctx, "当桃园结义", cx + btn_w/2 + gap/2, cy + 15,
                           ctx->font_small, ctx->color_white);

        /* 取消按钮 */
        SDL_Rect btn_cancel = {cx - 60, cy + 80, 120, 40};
        SDL_SetRenderDrawColor(ren, 50, 50, 50, 255);
        SDL_RenderFillRect(ren, &btn_cancel);
        SDL_SetRenderDrawColor(ren, 150, 150, 150, 255);
        SDL_RenderDrawRect(ren, &btn_cancel);
        render_text_center(ctx, "取消", cx, cy + 100,
                           ctx->font_small, ctx->color_white);
    }

    /* ---- 玉盏目标调整选择界面 ---- */
    if(game->resp_state == RESPONSE_NEED_YUZHAN_TARGET)
    {
        int cx = WINDOW_WIDTH / 2;
        int cy = WINDOW_HEIGHT / 2;
        int btn_w = 160;
        int btn_h = 50;
        int gap = 15;

        /* 半透明背景 */
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
        SDL_Rect bg = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
        SDL_RenderFillRect(ren, &bg);

        /* 标题 */
        render_text_center(ctx, "【玉盏】请选择目标调整方式",
                           cx, cy - 60, ctx->font_normal, ctx->color_yellow);

        /* 按钮1：增加目标 */
        SDL_Rect btn1 = {cx - btn_w*1.5 - gap, cy, btn_w, btn_h};
        SDL_SetRenderDrawColor(ren, 40, 80, 40, 255);
        SDL_RenderFillRect(ren, &btn1);
        SDL_SetRenderDrawColor(ren, 100, 255, 100, 255);
        SDL_RenderDrawRect(ren, &btn1);
        render_text_center(ctx, "增加目标", cx - btn_w - gap, cy + 15,
                           ctx->font_small, ctx->color_white);

        /* 按钮2：减少目标 */
        SDL_Rect btn2 = {cx - btn_w/2, cy, btn_w, btn_h};
        SDL_SetRenderDrawColor(ren, 80, 40, 40, 255);
        SDL_RenderFillRect(ren, &btn2);
        SDL_SetRenderDrawColor(ren, 255, 100, 100, 255);
        SDL_RenderDrawRect(ren, &btn2);
        render_text_center(ctx, "减少目标", cx, cy + 15,
                           ctx->font_small, ctx->color_white);

        /* 按钮3：不调整 */
        SDL_Rect btn3 = {cx + btn_w/2 + gap, cy, btn_w, btn_h};
        SDL_SetRenderDrawColor(ren, 60, 60, 80, 255);
        SDL_RenderFillRect(ren, &btn3);
        SDL_SetRenderDrawColor(ren, 200, 200, 255, 255);
        SDL_RenderDrawRect(ren, &btn3);
        render_text_center(ctx, "不调整", cx + btn_w + gap, cy + 15,
                           ctx->font_small, ctx->color_white);

        /* 取消按钮 */
        SDL_Rect btn_cancel = {cx - 60, cy + 80, 120, 40};
        SDL_SetRenderDrawColor(ren, 50, 50, 50, 255);
        SDL_RenderFillRect(ren, &btn_cancel);
        SDL_SetRenderDrawColor(ren, 150, 150, 150, 255);
        SDL_RenderDrawRect(ren, &btn_cancel);
        render_text_center(ctx, "取消", cx, cy + 100,
                           ctx->font_small, ctx->color_white);
    }

    /* ---- 流萤迸发：选择火属性/雷属性 ---- */
    if(game->resp_state == RESPONSE_NEED_LIUYING_BENGFA)
    {
        int cx = WINDOW_WIDTH / 2;
        int cy = WINDOW_HEIGHT / 2;
        int btn_w = 160;
        int btn_h = 60;
        int gap = 30;

        /* 半透明背景 */
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
        SDL_Rect bg = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
        SDL_RenderFillRect(ren, &bg);

        /* 标题 */
        render_text_center(ctx, "【迸发】请选择下次伤害的属性",
                           cx, cy - 80, ctx->font_normal, ctx->color_yellow);

        /* 按钮1：火属性（红色） */
        SDL_Rect btn1 = {cx - btn_w - gap/2, cy, btn_w, btn_h};
        SDL_SetRenderDrawColor(ren, 120, 30, 30, 255);
        SDL_RenderFillRect(ren, &btn1);
        SDL_SetRenderDrawColor(ren, 255, 80, 80, 255);
        SDL_RenderDrawRect(ren, &btn1);
        render_text_center(ctx, "火属性", cx - btn_w/2 - gap/2, cy + 20,
                           ctx->font_normal, ctx->color_white);

        /* 按钮2：雷属性（蓝色） */
        SDL_Rect btn2 = {cx + gap/2, cy, btn_w, btn_h};
        SDL_SetRenderDrawColor(ren, 30, 60, 120, 255);
        SDL_RenderFillRect(ren, &btn2);
        SDL_SetRenderDrawColor(ren, 80, 150, 255, 255);
        SDL_RenderDrawRect(ren, &btn2);
        render_text_center(ctx, "雷属性", cx + btn_w/2 + gap/2, cy + 20,
                           ctx->font_normal, ctx->color_white);

        /* 取消按钮 */
        SDL_Rect btn_cancel = {cx - 60, cy + 90, 120, 40};
        SDL_SetRenderDrawColor(ren, 50, 50, 50, 255);
        SDL_RenderFillRect(ren, &btn_cancel);
        SDL_SetRenderDrawColor(ren, 150, 150, 150, 255);
        SDL_RenderDrawRect(ren, &btn_cancel);
        render_text_center(ctx, "取消", cx, cy + 110,
                           ctx->font_small, ctx->color_white);
    }

    /* ---- 铁索连环无懈可击询问：提示当前询问哪个目标 ---- */
    if(game->resp_state == RESPONSE_NEED_TIESUO_WUXIE &&
       game->tiesuo_wuxie_index < game->tiesuo_target_count)
    {
        int target_idx = game->tiesuo_targets[game->tiesuo_wuxie_index];
        if(target_idx >= 0 && target_idx < game->player_count)
        {
            char tip[128];
            snprintf(tip, sizeof(tip), "【铁索连环】正在询问%s是否使用无懈可击（第%d/%d个目标）",
                     game->players[target_idx].name,
                     game->tiesuo_wuxie_index + 1, game->tiesuo_target_count);
            render_text_center(ctx, tip, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2,
                               ctx->font_normal, ctx->color_yellow);
        }
    }

    /* ---- 群体锦囊指定目标：从自己开始顺时针依次指定所有角色 ---- */
    if(game->resp_state == RESPONSE_NEED_GROUP_TARGET)
    {
        /* 当前需要指定的角色头像高亮 */
        if(game->group_target_current < game->group_target_count)
        {
            int idx = game->group_target_order[game->group_target_current];
            if(idx >= 0 && idx < game->player_count)
            {
                int tx, ty;
                if(idx == 0)
                {
                    tx = WINDOW_WIDTH - HERO_WIDTH - 40;
                    ty = WINDOW_HEIGHT - HERO_HEIGHT - 40;
                }
                else
                {
                    tx = (WINDOW_WIDTH - HERO_WIDTH) / 2;
                    ty = 60;
                }
                /* 金色双层边框高亮 */
                SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
                SDL_Rect glow1 = {tx - 5, ty - 5, HERO_WIDTH + 10, HERO_HEIGHT + 10};
                SDL_Rect glow2 = {tx - 3, ty - 3, HERO_WIDTH + 6, HERO_HEIGHT + 6};
                SDL_RenderDrawRect(ren, &glow1);
                SDL_RenderDrawRect(ren, &glow2);
            }
        }

        /* 提示文字 */
        char tip[128];
        if(game->group_target_current < game->group_target_count)
        {
            int idx = game->group_target_order[game->group_target_current];
            snprintf(tip, sizeof(tip), "【群体锦囊】请指定【%s】为目标（第%d/%d人），点击确定继续",
                     game->players[idx].name, game->group_target_current + 1, game->group_target_count);
        }
        else
        {
            snprintf(tip, sizeof(tip), "【群体锦囊】所有目标已指定完毕");
        }
        render_text_center(ctx, tip, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                           ctx->font_small, (SDL_Color){255, 215, 0, 255});
    }

    /* ---- 化形①：选择要执行的花色（2*2） ---- */
    if(game->resp_state == RESPONSE_NEED_HUAXING_SUIT)
    {
        Player* me = &game->players[0];
        int btn_w = 140, btn_h = 70, gap = 15;
        int total_w = btn_w * 2 + gap;
        int start_x = WINDOW_WIDTH / 2 - total_w / 2;
        int start_y = WINDOW_HEIGHT / 2 - btn_h - gap / 2;
        const char* suit_names[] = {"黑桃♠", "红桃♥", "梅花♣", "方块♦"};
        int suits[] = {SUIT_SPADE, SUIT_HEART, SUIT_CLUB, SUIT_DIAMOND};
        SDL_Color suit_colors[] = {{50,50,50,255}, {200,50,50,255}, {50,100,50,255}, {200,100,50,255}};

        for(int i = 0; i < 4; i++)
        {
            int col = i % 2, row = i / 2;
            int bx = start_x + col * (btn_w + gap);
            int by = start_y + row * (btn_h + gap);
            int used = (me->yudie.huaxing_used_suits & (1 << suits[i])) ? 1 : 0;

            if(used)
                SDL_SetRenderDrawColor(ren, 80, 80, 80, 200);
            else
                SDL_SetRenderDrawColor(ren, suit_colors[i].r, suit_colors[i].g, suit_colors[i].b, 200);
            SDL_Rect btn = {bx, by, btn_w, btn_h};
            SDL_RenderFillRect(ren, &btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &btn);
            render_text_center(ctx, suit_names[i], bx + btn_w / 2, by + 20,
                               ctx->font_normal, ctx->color_white);
            if(used)
                render_text_center(ctx, "已使用", bx + btn_w / 2, by + 45,
                                   ctx->font_small, (SDL_Color){200,200,200,255});
        }

        render_text_center(ctx, "【化形】选择要执行的花色（每花色限一次），点击取消结束",
                           WINDOW_WIDTH / 2, start_y - 30,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ---- 化形①：选择该花色的一张手牌 ---- */
    if(game->resp_state == RESPONSE_NEED_HUAXING_HAND)
    {
        const char* suit_names[] = {"黑桃", "红桃", "梅花", "方块"};
        char tip[128];
        snprintf(tip, sizeof(tip), "【化形】请选择一张%s花色的手牌",
                 game->huaxing_current_suit >= 0 ? suit_names[game->huaxing_current_suit] : "");
        render_text_center(ctx, tip, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2,
                           ctx->font_large, ctx->color_yellow);
    }

    /* ---- 化形①：选择本回合使用过的锦囊牌名 ---- */
    if(game->resp_state == RESPONSE_NEED_HUAXING_TRICK)
    {
        Player* me = &game->players[0];
        int btn_w = 200, btn_h = 50, gap = 10;
        int start_x = WINDOW_WIDTH / 2 - btn_w / 2;
        int start_y = WINDOW_HEIGHT / 2 - (me->yudie.huaxing_used_trick_count * (btn_h + gap)) / 2;

        for(int i = 0; i < me->yudie.huaxing_used_trick_count; i++)
        {
            int by = start_y + i * (btn_h + gap);
            int used = 0;
            for(int j = 0; j < me->yudie.huaxing_played_name_count; j++)
            {
                if(strcmp(me->yudie.huaxing_played_names[j], me->yudie.huaxing_used_tricks[i]) == 0)
                {
                    used = 1;
                    break;
                }
            }

            if(used)
                SDL_SetRenderDrawColor(ren, 80, 80, 80, 200);
            else
                SDL_SetRenderDrawColor(ren, 50, 100, 150, 200);
            SDL_Rect btn = {start_x, by, btn_w, btn_h};
            SDL_RenderFillRect(ren, &btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &btn);
            render_text_center(ctx, me->yudie.huaxing_used_tricks[i], start_x + btn_w / 2, by + 15,
                               ctx->font_normal, ctx->color_white);
            if(used)
                render_text_center(ctx, "已使用", start_x + btn_w / 2, by + 35,
                                   ctx->font_small, (SDL_Color){200,200,200,255});
        }

        render_text_center(ctx, "【化形】选择要转化的锦囊牌名（不可重复）",
                           WINDOW_WIDTH / 2, start_y - 30,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ---- 雨蝶飞舞拖拽：装备格子虚线 + 待放置的牌 + 拖拽中的牌 ---- */
    if(game->resp_state == RESPONSE_NEED_FEIWUU_DRAG)
    {
        /* 装备区位置 */
        int hand_start_x = 320;
        int feixiao_y = WINDOW_HEIGHT - HERO_HEIGHT - 40;
        int hand_y = feixiao_y + HERO_HEIGHT - CARD_HEIGHT + 10;
        int equip_y = hand_y - CARD_HEIGHT - 25;

        /* 四个装备格子变成虚线 */
        const char* slot_names[] = {"武器", "防具", "进攻马", "防御马"};
        for(int i = 0; i < 4; i++)
        {
            int slot_x = hand_start_x + i * (CARD_WIDTH + 10);
            /* 已放置的槽位不显示虚线 */
            if(game->feiwuu_placed_slots[i]) continue;

            /* 虚线边框 */
            SDL_SetRenderDrawColor(ren, 200, 200, 200, 200);
            /* 画虚线：上下左右各画几段 */
            int dash_len = 8;
            int gap_len = 4;
            /* 上边 */
            for(int x = slot_x; x < slot_x + CARD_WIDTH; x += dash_len + gap_len)
            {
                int len = (x + dash_len > slot_x + CARD_WIDTH) ? (slot_x + CARD_WIDTH - x) : dash_len;
                SDL_RenderDrawLine(ren, x, equip_y, x + len, equip_y);
            }
            /* 下边 */
            for(int x = slot_x; x < slot_x + CARD_WIDTH; x += dash_len + gap_len)
            {
                int len = (x + dash_len > slot_x + CARD_WIDTH) ? (slot_x + CARD_WIDTH - x) : dash_len;
                SDL_RenderDrawLine(ren, x, equip_y + CARD_HEIGHT, x + len, equip_y + CARD_HEIGHT);
            }
            /* 左边 */
            for(int y = equip_y; y < equip_y + CARD_HEIGHT; y += dash_len + gap_len)
            {
                int len = (y + dash_len > equip_y + CARD_HEIGHT) ? (equip_y + CARD_HEIGHT - y) : dash_len;
                SDL_RenderDrawLine(ren, slot_x, y, slot_x, y + len);
            }
            /* 右边 */
            for(int y = equip_y; y < equip_y + CARD_HEIGHT; y += dash_len + gap_len)
            {
                int len = (y + dash_len > equip_y + CARD_HEIGHT) ? (equip_y + CARD_HEIGHT - y) : dash_len;
                SDL_RenderDrawLine(ren, slot_x + CARD_WIDTH, y, slot_x + CARD_WIDTH, y + len);
            }

            /* 槽位名称 */
            render_text_center(ctx, slot_names[i], slot_x + CARD_WIDTH / 2, equip_y + CARD_HEIGHT / 2 - 8,
                               ctx->font_small, (SDL_Color){180, 180, 180, 200});
        }

        /* 待放置的牌显示在屏幕中间下方，横向排列 */
        int card_w = CARD_WIDTH;
        int card_h = CARD_HEIGHT;
        int total_w = game->feiwuu_drag_count * (card_w + 10);
        int start_x = WINDOW_WIDTH / 2 - total_w / 2;
        int start_y = WINDOW_HEIGHT / 2 + 150;

        for(int i = 0; i < game->feiwuu_drag_count; i++)
        {
            if(!game->feiwuu_drag_cards[i]) continue;
            /* 正在拖拽的牌不在这里显示（跟随鼠标显示） */
            if(game->feiwuu_dragging && game->feiwuu_drag_index == i) continue;

            int cx = start_x + i * (card_w + 10);
            render_card(ctx, game->feiwuu_drag_cards[i], cx, start_y, 0);

            /* 悬停发光 */
            render_hover_glow(ctx, cx, start_y, card_w, card_h,
                              game->mouse_x, game->mouse_y);
        }

        /* 正在拖拽的牌跟随鼠标 */
        if(game->feiwuu_dragging && game->feiwuu_drag_index >= 0 &&
           game->feiwuu_drag_index < game->feiwuu_drag_count &&
           game->feiwuu_drag_cards[game->feiwuu_drag_index])
        {
            int drag_x = game->feiwuu_drag_x - CARD_WIDTH / 2;
            int drag_y = game->feiwuu_drag_y - CARD_HEIGHT / 2;
            render_card(ctx, game->feiwuu_drag_cards[game->feiwuu_drag_index], drag_x, drag_y, 0);
        }

        /* 提示文字 */
        render_text_center(ctx, "【飞舞】将所有牌拖拽到对应的装备格子中",
                           WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 100,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ---- 吉尔伽美什·所见：2*2选择牌类型 ---- */
    if(game->resp_state == RESPONSE_NEED_GILGAMESH_SUOJIAN)
    {
        int btn_w = 220, btn_h = 90, gap = 20;
        int total_w = btn_w * 2 + gap;
        int start_x = WINDOW_WIDTH / 2 - total_w / 2;
        int start_y = WINDOW_HEIGHT / 2 - btn_h - gap / 2;

        const char* options[4] = {
            "基本牌\n杀/闪/桃",
            "锦囊牌\n随机锦囊",
            "装备牌\n随机装备",
            "延时锦囊\n随机延时"
        };
        SDL_Color colors[4] = {
            {200, 50, 50, 255}, {50, 150, 50, 255},
            {200, 150, 50, 255}, {100, 50, 150, 255}
        };

        for(int i = 0; i < 4; i++)
        {
            int col = i % 2, row = i / 2;
            int bx = start_x + col * (btn_w + gap);
            int by = start_y + row * (btn_h + gap);

            /* 检查鼠标是否悬停在选项上 */
            int hover = (game->mouse_x >= bx && game->mouse_x <= bx + btn_w &&
                         game->mouse_y >= by && game->mouse_y <= by + btn_h);

            /* 按钮背景：悬停时更亮 */
            if(hover)
                SDL_SetRenderDrawColor(ren, colors[i].r + 40, colors[i].g + 40, colors[i].b + 40, 230);
            else
                SDL_SetRenderDrawColor(ren, colors[i].r, colors[i].g, colors[i].b, 200);
            SDL_Rect btn = {bx, by, btn_w, btn_h};
            SDL_RenderFillRect(ren, &btn);

            /* 按钮边框：悬停时发光（黄色双层边框） */
            if(hover)
            {
                SDL_SetRenderDrawColor(ren, 255, 255, 0, 255);
                SDL_Rect glow1 = {bx - 4, by - 4, btn_w + 8, btn_h + 8};
                SDL_Rect glow2 = {bx - 2, by - 2, btn_w + 4, btn_h + 4};
                SDL_RenderDrawRect(ren, &glow1);
                SDL_RenderDrawRect(ren, &glow2);
            }
            else
            {
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_RenderDrawRect(ren, &btn);
            }

            char line1[64], line2[64];
            sscanf(options[i], "%63[^\n]\n%63[^\n]", line1, line2);
            render_text_center(ctx, line1, bx + btn_w / 2, by + 20,
                               ctx->font_normal, ctx->color_white);
            render_text_center(ctx, line2, bx + btn_w / 2, by + 50,
                               ctx->font_small, ctx->color_white);
        }
        render_text_center(ctx, "【所见】请选择要检索的牌类型（点击取消按钮取消）",
                           WINDOW_WIDTH / 2, start_y - 35,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ---- 吉尔伽美什·乖离：2*2选择花色 ---- */
    if(game->resp_state == RESPONSE_NEED_GILGAMESH_GUAILI)
    {
        int btn_w = 220, btn_h = 90, gap = 20;
        int total_w = btn_w * 2 + gap;
        int start_x = WINDOW_WIDTH / 2 - total_w / 2;
        int start_y = WINDOW_HEIGHT / 2 - btn_h - gap / 2;

        const char* options[4] = {
            "黑桃 ♠",
            "红桃 ♥",
            "梅花 ♣",
            "方块 ♦"
        };
        SDL_Color colors[4] = {
            {50, 50, 50, 255}, {200, 50, 50, 255},
            {50, 100, 50, 255}, {200, 100, 50, 255}
        };

        for(int i = 0; i < 4; i++)
        {
            int col = i % 2, row = i / 2;
            int bx = start_x + col * (btn_w + gap);
            int by = start_y + row * (btn_h + gap);

            /* 检查鼠标是否悬停在选项上 */
            int hover = (game->mouse_x >= bx && game->mouse_x <= bx + btn_w &&
                         game->mouse_y >= by && game->mouse_y <= by + btn_h);

            /* 按钮背景：悬停时更亮 */
            if(hover)
                SDL_SetRenderDrawColor(ren, colors[i].r + 40, colors[i].g + 40, colors[i].b + 40, 230);
            else
                SDL_SetRenderDrawColor(ren, colors[i].r, colors[i].g, colors[i].b, 200);
            SDL_Rect btn = {bx, by, btn_w, btn_h};
            SDL_RenderFillRect(ren, &btn);

            /* 按钮边框：悬停时发光（黄色双层边框） */
            if(hover)
            {
                SDL_SetRenderDrawColor(ren, 255, 255, 0, 255);
                SDL_Rect glow1 = {bx - 4, by - 4, btn_w + 8, btn_h + 8};
                SDL_Rect glow2 = {bx - 2, by - 2, btn_w + 4, btn_h + 4};
                SDL_RenderDrawRect(ren, &glow1);
                SDL_RenderDrawRect(ren, &glow2);
            }
            else
            {
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_RenderDrawRect(ren, &btn);
            }

            render_text_center(ctx, options[i], bx + btn_w / 2, by + 30,
                               ctx->font_large, ctx->color_white);
        }
        render_text_center(ctx, "【乖离】请选择要弃置的花色（点击取消按钮取消）",
                           WINDOW_WIDTH / 2, start_y - 35,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ---- 吉尔伽美什·天辟：选择目标 ---- */
    if(game->resp_state == RESPONSE_NEED_GILGAMESH_TIANPI_TARGET)
    {
        render_text_center(ctx, "【天辟】请选择目标角色（点击头像，点击取消按钮取消）",
                           WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2,
                           ctx->font_normal, ctx->color_yellow);

        /* 鼠标悬停头像高亮 */
        int feixiao_x = WINDOW_WIDTH - HERO_WIDTH - 40;
        int feixiao_y = WINDOW_HEIGHT - HERO_HEIGHT - 40;
        int zhaoyun_x = (WINDOW_WIDTH - HERO_WIDTH) / 2;
        int zhaoyun_y = 60;

        /* 玩家0头像悬停高亮 */
        render_hover_glow(ctx, feixiao_x, feixiao_y, HERO_WIDTH, HERO_HEIGHT,
                          game->mouse_x, game->mouse_y);
        /* 玩家1头像悬停高亮 */
        render_hover_glow(ctx, zhaoyun_x, zhaoyun_y, HERO_WIDTH, HERO_HEIGHT,
                          game->mouse_x, game->mouse_y);
    }

    /* ---- 吉尔伽美什·天辟：选择点数（点击手牌） ---- */
    if(game->resp_state == RESPONSE_NEED_GILGAMESH_TIANPI_RANK)
    {
        render_text_center(ctx, "【天辟】请点击一张手牌，以其点数作为弃置点数（点击取消按钮取消）",
                           WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ---- 八卦阵：需要出闪时亮起，选择是否判定 ---- */
    if(game->resp_state == RESPONSE_NEED_BAGUA && game->bagua_active)
    {
        Player* me = &game->players[0];

        /* 八卦阵亮起（青色边框） */
        if(player_armor_type(me) == ARMOR_BAGUA)
        {
            int ax = hand_start_x + CARD_WIDTH + 10;  /* 防具在武器右边 */
            int ay = hand_y - CARD_HEIGHT - 25;
            /* 多层青色边框模拟发光 */
            SDL_SetRenderDrawColor(ren, 0, 200, 255, 255);  /* 青色 */
            SDL_Rect glow1 = {ax - 4, ay - 4, CARD_WIDTH + 8, CARD_HEIGHT + 8};
            SDL_RenderDrawRect(ren, &glow1);
            SDL_Rect glow2 = {ax - 2, ay - 2, CARD_WIDTH + 4, CARD_HEIGHT + 4};
            SDL_RenderDrawRect(ren, &glow2);
            /* 选中时额外高亮 */
            if(game->bagua_selected)
            {
                SDL_SetRenderDrawColor(ren, 255, 255, 0, 255);  /* 黄色 */
                SDL_Rect sel = {ax - 5, ay - 5, CARD_WIDTH + 10, CARD_HEIGHT + 10};
                SDL_RenderDrawRect(ren, &sel);
            }
        }

        /* 提示文字 */
        if(game->bagua_selected == 0)
            render_text_center(ctx, "【八卦阵】点击八卦阵选中，进行判定；点击取消不发动",
                               WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                               ctx->font_small, (SDL_Color){0, 200, 255, 255});
        else
            render_text_center(ctx, "【八卦阵】已选中，点击确定进行判定（红色视为闪），或点击取消取消选中",
                               WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                               ctx->font_small, (SDL_Color){255, 255, 0, 255});
    }

    /* ---- 圣骑士神圣护盾：2*2选项按钮 ---- */
    if(game->resp_state == RESPONSE_NEED_PALADIN_CHOICE)
    {
        int btn_w = 220;
        int btn_h = 90;
        int gap = 20;
        int total_w = btn_w * 2 + gap;
        int start_x = WINDOW_WIDTH / 2 - total_w / 2;
        int start_y = WINDOW_HEIGHT / 2 - btn_h - gap / 2;

        const char* options[4] = {
            "1.流失1体力\n  圣骑士+2盾",
            "2.弃1牌\n  圣骑士+1盾",
            "3.弃1牌\n  圣骑士-1盾",
            "4.受1伤害\n  圣骑士-2盾"
        };

        SDL_Color colors[4] = {
            {200, 50, 50, 255},   /* 选项1：红色 */
            {50, 150, 50, 255},   /* 选项2：绿色 */
            {200, 150, 50, 255},  /* 选项3：橙色 */
            {100, 50, 150, 255}   /* 选项4：紫色 */
        };

        for(int i = 0; i < 4; i++)
        {
            int col = i % 2;
            int row = i / 2;
            int bx = start_x + col * (btn_w + gap);
            int by = start_y + row * (btn_h + gap);

            int option_num = i + 1;
            int available = paladin_option_available(game, option_num);

            if(available)
            {
                /* 可用：正常颜色 */
                SDL_SetRenderDrawColor(ren, colors[i].r, colors[i].g, colors[i].b, 200);
            }
            else
            {
                /* 不可用：灰色 */
                SDL_SetRenderDrawColor(ren, 80, 80, 80, 200);
            }
            SDL_Rect btn = {bx, by, btn_w, btn_h};
            SDL_RenderFillRect(ren, &btn);
            /* 按钮边框 */
            if(available)
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            else
                SDL_SetRenderDrawColor(ren, 120, 120, 120, 255);
            SDL_RenderDrawRect(ren, &btn);

            /* 按钮文字（分两行） */
            char line1[64], line2[64];
            sscanf(options[i], "%63[^\n]\n%63[^\n]", line1, line2);
            if(available)
            {
                render_text_center(ctx, line1, bx + btn_w / 2, by + 20,
                                   ctx->font_normal, ctx->color_white);
                render_text_center(ctx, line2, bx + btn_w / 2, by + 50,
                                   ctx->font_small, ctx->color_white);
            }
            else
            {
                render_text_center(ctx, line1, bx + btn_w / 2, by + 20,
                                   ctx->font_normal, (SDL_Color){150, 150, 150, 255});
                render_text_center(ctx, line2, bx + btn_w / 2, by + 50,
                                   ctx->font_small, (SDL_Color){150, 150, 150, 255});
            }

            /* 悬停发光：仅可用选项 */
            if(available)
            {
                render_hover_glow(ctx, bx, by, btn_w, btn_h,
                                  game->mouse_x, game->mouse_y);
            }
        }

        /* 标题 */
        render_text_center(ctx, "【神圣护盾】请选择一项",
                           WINDOW_WIDTH / 2, start_y - 35,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ---- 镜流古镜照神：选项1/选项2/取消 ---- */
    if(game->resp_state == RESPONSE_NEED_JINGLIU_GUJING)
    {
        Player* me = &game->players[0];
        int btn_w = 240;
        int btn_h = 100;
        int gap = 30;
        int total_w = btn_w * 2 + gap;
        int start_x = WINDOW_WIDTH / 2 - total_w / 2;
        int start_y = WINDOW_HEIGHT / 2 - btn_h / 2 - 30;

        const char* opt_names[2] = {
            "选项1：失去3标记",
            "选项2：失去5标记"
        };
        const char* opt_desc[2] = {
            "摸3张+获得所有人一张牌",
            "摸5张+对全体出杀"
        };
        SDL_Color opt_colors[2] = {
            {50, 100, 200, 255},
            {200, 50, 50, 255}
        };

        for(int i = 0; i < 2; i++)
        {
            int bx = start_x + i * (btn_w + gap);
            int by = start_y;
            int available = jingliu_gujing_option_available(game, 0, i + 1);

            if(available){
                SDL_SetRenderDrawColor(ren, opt_colors[i].r, opt_colors[i].g, opt_colors[i].b, 220);
            }else{
                SDL_SetRenderDrawColor(ren, 80, 80, 80, 200);
            }
            SDL_Rect btn = {bx, by, btn_w, btn_h};
            SDL_RenderFillRect(ren, &btn);

            if(available)
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            else
                SDL_SetRenderDrawColor(ren, 120, 120, 120, 255);
            SDL_RenderDrawRect(ren, &btn);

            SDL_Color text_color = available ? ctx->color_white : (SDL_Color){150, 150, 150, 255};
            render_text_center(ctx, opt_names[i], bx + btn_w / 2, by + 20,
                               ctx->font_normal, text_color);
            render_text_center(ctx, opt_desc[i], bx + btn_w / 2, by + 55,
                               ctx->font_small, text_color);
            char mark_str[32];
            snprintf(mark_str, sizeof(mark_str), "当前标记：%d", me->jingliu.hong_marks);
            render_text_center(ctx, mark_str, bx + btn_w / 2, by + 80,
                               ctx->font_small, text_color);

            if(available){
                render_hover_glow(ctx, bx, by, btn_w, btn_h,
                                  game->mouse_x, game->mouse_y);
            }
        }

        /* 取消按钮 */
        int cancel_w = 120;
        int cancel_h = 50;
        int cancel_x = WINDOW_WIDTH / 2 - cancel_w / 2;
        int cancel_y = start_y + btn_h + 30;
        SDL_SetRenderDrawColor(ren, 100, 100, 100, 220);
        SDL_Rect cancel_btn = {cancel_x, cancel_y, cancel_w, cancel_h};
        SDL_RenderFillRect(ren, &cancel_btn);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDrawRect(ren, &cancel_btn);
        render_text_center(ctx, "取消", cancel_x + cancel_w / 2, cancel_y + 15,
                           ctx->font_normal, ctx->color_white);
        render_hover_glow(ctx, cancel_x, cancel_y, cancel_w, cancel_h,
                          game->mouse_x, game->mouse_y);

        /* 标题 */
        render_text_center(ctx, "【古镜照神】请选择一项",
                           WINDOW_WIDTH / 2, start_y - 35,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ---- 化形①：2*2花色选择界面 ---- */
    if(game->resp_state == RESPONSE_NEED_HUAXING_SUIT)
    {
        Player* me = &game->players[0];
        int btn_w = 180;
        int btn_h = 100;
        int gap = 25;
        int total_w = btn_w * 2 + gap;
        int start_x = WINDOW_WIDTH / 2 - total_w / 2;
        int start_y = WINDOW_HEIGHT / 2 - btn_h - gap / 2;

        const char* suit_names[4] = {"黑桃", "红桃", "梅花", "方块"};
        SDL_Color suit_colors[4] = {
            {50, 50, 50, 255},    /* 黑桃：黑 */
            {200, 50, 50, 255},   /* 红桃：红 */
            {30, 100, 30, 255},   /* 梅花：深绿 */
            {200, 100, 20, 255}   /* 方块：橙 */
        };
        char suit_symbols[4] = {'S', 'H', 'C', 'D'};

        for(int i = 0; i < 4; i++)
        {
            int col = i % 2;
            int row = i / 2;
            int bx = start_x + col * (btn_w + gap);
            int by = start_y + row * (btn_h + gap);

            /* 检查该花色是否可用 */
            int available = 0;
            if(me->hero_id == HERO_YUDIE && me->yudie.chengdie)
            {
                /* 有该花色手牌 */
                for(int j = 0; j < me->hand_count; j++)
                {
                    if(me->hand[j] && me->hand[j]->suit == i) { available = 1; break; }
                }
                /* 已使用的花色不可选 */
                if(me->yudie.huaxing_used_suits & (1 << i)) available = 0;
            }

            if(available)
                SDL_SetRenderDrawColor(ren, suit_colors[i].r, suit_colors[i].g, suit_colors[i].b, 220);
            else
                SDL_SetRenderDrawColor(ren, 80, 80, 80, 180);

            SDL_Rect btn = {bx, by, btn_w, btn_h};
            SDL_RenderFillRect(ren, &btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, available ? 255 : 100);
            SDL_RenderDrawRect(ren, &btn);

            /* 花色符号 */
            char sym[2] = {suit_symbols[i], '\0'};
            render_text_center(ctx, sym, bx + btn_w/2, by + 25,
                               ctx->font_large, available ? ctx->color_white : (SDL_Color){150,150,150,255});
            /* 花色名称 */
            render_text_center(ctx, suit_names[i], bx + btn_w/2, by + 65,
                               ctx->font_normal, available ? ctx->color_white : (SDL_Color){150,150,150,255});

            if(available)
                render_hover_glow(ctx, bx, by, btn_w, btn_h, game->mouse_x, game->mouse_y);
        }

        /* 标题 */
        render_text_center(ctx, "【化形】选择花色（每花色限一次）",
                           WINDOW_WIDTH / 2, start_y - 40,
                           ctx->font_normal, ctx->color_yellow);

        /* 结束按钮 */
        int end_bx = WINDOW_WIDTH / 2 - 60;
        int end_by = start_y + 2 * btn_h + gap + 20;
        SDL_SetRenderDrawColor(ren, 100, 100, 100, 220);
        SDL_Rect end_btn = {end_bx, end_by, 120, 40};
        SDL_RenderFillRect(ren, &end_btn);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDrawRect(ren, &end_btn);
        render_text_center(ctx, "结束化形", end_bx + 60, end_by + 10,
                           ctx->font_normal, ctx->color_white);
    }

    /* ---- 化形①：选择手牌界面 ---- */
    if(game->resp_state == RESPONSE_NEED_HUAXING_HAND)
    {
        render_text_center(ctx, "【化形】请选择一张该花色的手牌（点击取消按钮返回）",
                           WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 100,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ---- 化形①：选择锦囊牌名界面 ---- */
    if(game->resp_state == RESPONSE_NEED_HUAXING_TRICK)
    {
        Player* me = &game->players[0];
        /* 获取当前花色可用的锦囊牌名 */
        char tricks[10][32];
        int trick_count = 0;
        /* 简化：从记录中获取 */
        for(int i = 0; i < me->yudie.huaxing_record_count && trick_count < 10; i++)
        {
            int dup = 0;
            for(int j = 0; j < trick_count; j++)
                if(strcmp(tricks[j], me->yudie.huaxing_record_names[i]) == 0) { dup = 1; break; }
            for(int j = 0; j < me->yudie.huaxing_played_name_count && !dup; j++)
                if(strcmp(me->yudie.huaxing_played_names[j], me->yudie.huaxing_record_names[i]) == 0) { dup = 1; break; }
            if(!dup) { strncpy(tricks[trick_count], me->yudie.huaxing_record_names[i], 31); tricks[trick_count][31]='\0'; trick_count++; }
        }

        int cols = 4;
        int btn_w = 140;
        int btn_h = 50;
        int gap_x = 12;
        int gap_y = 12;
        int rows = (trick_count + cols - 1) / cols;
        int total_w = cols * btn_w + (cols - 1) * gap_x;
        int start_x = WINDOW_WIDTH / 2 - total_w / 2;
        int start_y = WINDOW_HEIGHT / 2 - (rows * btn_h + (rows-1) * gap_y) / 2;

        for(int i = 0; i < trick_count; i++)
        {
            int col = i % cols;
            int row = i / cols;
            int bx = start_x + col * (btn_w + gap_x);
            int by = start_y + row * (btn_h + gap_y);

            SDL_SetRenderDrawColor(ren, 60, 100, 160, 220);
            SDL_Rect btn = {bx, by, btn_w, btn_h};
            SDL_RenderFillRect(ren, &btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &btn);
            render_text_center(ctx, tricks[i], bx + btn_w/2, by + 15,
                               ctx->font_normal, ctx->color_white);
            render_hover_glow(ctx, bx, by, btn_w, btn_h, game->mouse_x, game->mouse_y);
        }

        render_text_center(ctx, "【化形】选择要转化的锦囊牌名",
                           WINDOW_WIDTH / 2, start_y - 35,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ---- 化形②：选择目标界面 ---- */
    if(game->resp_state == RESPONSE_NEED_HUAXING_TARGET)
    {
        render_text_center(ctx, "【化形②】请点击指定一名角色（点击取消按钮跳过）",
                           WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 120,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ---- 圣骑士破灭护盾：牌名选择界面 ---- */
    if(game->resp_state == RESPONSE_NEED_PALADIN_POMIE_CARD)
    {
        int card_count = paladin_pomie_get_card_count();
        int cols = 5;
        int rows = (card_count + cols - 1) / cols;
        int btn_w = 130;
        int btn_h = 50;
        int gap_x = 10;
        int gap_y = 10;
        int total_w = cols * btn_w + (cols - 1) * gap_x;
        int start_x = WINDOW_WIDTH / 2 - total_w / 2;
        int start_y = WINDOW_HEIGHT / 2 - (rows * btn_h + (rows - 1) * gap_y) / 2 - 30;

        /* 标题 */
        render_text_center(ctx, "【破灭护盾】请选择要打出的牌名",
                           WINDOW_WIDTH / 2, start_y - 35,
                           ctx->font_normal, ctx->color_yellow);

        /* 渲染牌名按钮 */
        for(int i = 0; i < card_count; i++)
        {
            int col = i % cols;
            int row = i / cols;
            int bx = start_x + col * (btn_w + gap_x);
            int by = start_y + row * (btn_h + gap_y);
            const char* name = paladin_pomie_get_card_name(i);

            int selected = (strcmp(game->pomie_selected_card_name, name) == 0);

            if(selected)
            {
                /* 已选中：金色高亮 */
                SDL_SetRenderDrawColor(ren, 90, 90, 150, 255);
                SDL_Rect btn = {bx, by, btn_w, btn_h};
                SDL_RenderFillRect(ren, &btn);
                SDL_SetRenderDrawColor(ren, 255, 220, 50, 255);
                SDL_RenderDrawRect(ren, &btn);
                SDL_Rect btn2 = {bx+2, by+2, btn_w-4, btn_h-4};
                SDL_SetRenderDrawColor(ren, 255, 240, 100, 255);
                SDL_RenderDrawRect(ren, &btn2);
                render_text_center(ctx, name, bx + btn_w / 2, by + btn_h / 2 - 8,
                                   ctx->font_normal, ctx->color_yellow);
            }
            else
            {
                /* 未选中：普通颜色 */
                SDL_SetRenderDrawColor(ren, 45, 45, 65, 255);
                SDL_Rect btn = {bx, by, btn_w, btn_h};
                SDL_RenderFillRect(ren, &btn);
                SDL_SetRenderDrawColor(ren, 120, 120, 140, 255);
                SDL_RenderDrawRect(ren, &btn);
                render_text_center(ctx, name, bx + btn_w / 2, by + btn_h / 2 - 8,
                                   ctx->font_normal, ctx->color_white);
            }

            /* 悬停发光 */
            render_hover_glow(ctx, bx, by, btn_w, btn_h,
                              game->mouse_x, game->mouse_y);
        }

        /* 确定/取消按钮区域 */
        {
            int confirm_w = 100;
            int confirm_h = 40;
            int btn_gap = 30;
            int total_btn_w = confirm_w * 2 + btn_gap;
            int btn_start_x = WINDOW_WIDTH / 2 - total_btn_w / 2;
            int btn_y = start_y + rows * (btn_h + gap_y) + 20;
            int has_selected = (game->pomie_selected_card_name[0] != '\0');

            /* 确定按钮（选中牌名后亮起，否则灰色） */
            SDL_Rect confirm_btn = {btn_start_x, btn_y, confirm_w, confirm_h};
            if(has_selected) {
                SDL_SetRenderDrawColor(ren, 50, 150, 50, 255);
            } else {
                SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
            }
            SDL_RenderFillRect(ren, &confirm_btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &confirm_btn);
            render_text_center(ctx, "确定", btn_start_x + confirm_w / 2, btn_y + 10,
                               ctx->font_normal, has_selected ? ctx->color_white : ctx->color_white);

            /* 取消按钮（始终显示，始终可点） */
            SDL_Rect cancel_btn = {btn_start_x + confirm_w + btn_gap, btn_y, confirm_w, confirm_h};
            SDL_SetRenderDrawColor(ren, 150, 50, 50, 255);
            SDL_RenderFillRect(ren, &cancel_btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &cancel_btn);
            render_text_center(ctx, "取消", btn_start_x + confirm_w + btn_gap + confirm_w / 2, btn_y + 10,
                               ctx->font_normal, ctx->color_white);
        }
    }

    /* ---- 火攻：目标展示的牌（屏幕中央偏上） ---- */
    if(game->huogong_active && game->huogong_show_card)
    {
        int hg_cx = WINDOW_WIDTH / 2 - CARD_WIDTH / 2;
        int hg_cy = WINDOW_HEIGHT / 2 - CARD_HEIGHT / 2 - 80;
        render_card(ctx, game->huogong_show_card, hg_cx, hg_cy, 0);
        render_text_center(ctx, "火攻 - 目标展示的牌",
                           WINDOW_WIDTH / 2, hg_cy - 24,
                           ctx->font_normal, ctx->color_yellow);
    }

    /* ========== 中间只保留游戏结束文字，普通回合不再绘制阶段文字 ========== */
    if(game->phase == PHASE_GAME_OVER)
    {
        char win_str[64];
        snprintf(win_str, sizeof(win_str), "胜者: %s",
                 game->players[game->winner_id].name);
        render_text_center(ctx, win_str, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2,
                           ctx->font_large, ctx->color_red);
        render_text_center(ctx, "按 R 键重新开始",
                           WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 50,
                           ctx->font_normal, ctx->color_white);
    }
    else if(game->phase == PHASE_PLAY && game->current_player == 0)
    {
        render_text_center(ctx,
            "点击手牌使用/装备",
            WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 20,
            ctx->font_normal, ctx->color_white);
    }


    /* ---- 响应询问文字：已全部改用鼠标点击交互（取消/确认按钮），不再显示Y/N提示 ---- */
    /* RESPONSE_NEED_WUXIE / RESPONSE_NEED_BASIC 的提示在通用响应渲染区域显示 */

    else if(game->resp_state == RESPONSE_NEED_WUGU_PICK)
    {
        render_text_center(ctx, "五谷丰登 - 点击选择一张牌",
                           WINDOW_WIDTH/2, 110,
                           ctx->font_normal, ctx->color_yellow);
    }
    else if(game->resp_state == RESPONSE_NEED_TARGET)
    {
        char buf[128];
        if(game->pending_card)
            snprintf(buf, sizeof(buf), "请选择【%s】的目标（点击取消按钮放弃）",
                     card_get_name(game->pending_card));
        else
            snprintf(buf, sizeof(buf), "请选择目标（点击取消按钮放弃）");
        render_text_center(ctx, buf, WINDOW_WIDTH/2, WINDOW_HEIGHT/2 + 40,
                           ctx->font_large, ctx->color_yellow);
    }
    else if(game->resp_state == RESPONSE_NEED_HUOGONG_SHOW)
    {
        render_text_center(ctx, "火攻：请点击一张手牌展示",
                           WINDOW_WIDTH/2, WINDOW_HEIGHT/2 + 40,
                           ctx->font_large, ctx->color_yellow);
    }
    else if(game->resp_state == RESPONSE_NEED_HUOGONG_PICK)
    {
        const char* suit_name = "未知";
        switch(game->huogong_need_suit) {
        case SUIT_SPADE:   suit_name = "黑桃"; break;
        case SUIT_HEART:   suit_name = "红桃"; break;
        case SUIT_CLUB:    suit_name = "梅花"; break;
        case SUIT_DIAMOND: suit_name = "方块"; break;
        }
        char buf[128];
        snprintf(buf, sizeof(buf), "火攻：请选择一张【%s】手牌（点击取消按钮放弃）", suit_name);
        render_text_center(ctx, buf, WINDOW_WIDTH/2, WINDOW_HEIGHT/2 + 40,
                           ctx->font_large, ctx->color_yellow);
    }
    else if(game->resp_state == RESPONSE_NEED_GENERIC_DISCARD)
    {
        char buf[128];
        if(game->generic_discard_source == 201)
        {
            /* 弃牌阶段 */
            snprintf(buf, sizeof(buf), "弃牌阶段：请选择%d张手牌弃置（已选%d张）",
                     game->generic_discard_need, game->generic_discard_selected_count);
        }
        else
        {
            /* 其他通用弃牌 */
            snprintf(buf, sizeof(buf), "请选择%d张手牌弃置（已选%d张）",
                     game->generic_discard_need, game->generic_discard_selected_count);
        }
        render_text_center(ctx, buf, WINDOW_WIDTH/2, WINDOW_HEIGHT/2 + 40,
                           ctx->font_large, ctx->color_yellow);
    }

    /* ---- 左下角：日志 ---- */
    render_text(ctx, "=== 游戏日志 ===", 20, WINDOW_HEIGHT - 180,
                ctx->font_small, ctx->color_white);
    for (int i = 0; i < game->log_count && i < 8; i++) {
        render_text(ctx, game->log_buf[game->log_count - 1 - i],
                    20, WINDOW_HEIGHT - 160 + i * 18,
                    ctx->font_small, ctx->color_white);
    }

    /* ---- 左下角：日志按钮 ---- */
    {
        int btn_w = 60;
        int btn_h = 22;
        int btn_x = 120;
        int btn_y = WINDOW_HEIGHT - 182;

        if(game->log_panel_open)
            SDL_SetRenderDrawColor(ren, 100, 100, 200, 255);  /* 打开状态：蓝色 */
        else
            SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);  /* 灰色 */
        SDL_Rect log_btn = {btn_x, btn_y, btn_w, btn_h};
        SDL_RenderFillRect(ren, &log_btn);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDrawRect(ren, &log_btn);
        render_text_center(ctx, "日志", btn_x + btn_w / 2, btn_y + 3,
                           ctx->font_small, ctx->color_white);
        render_hover_glow(ctx, btn_x, btn_y, btn_w, btn_h,
                          game->mouse_x, game->mouse_y);
    }

    /* ================================================================
     * 通用按钮渲染：所有状态的确定/取消按钮统一在这里处理
     * 根据状态决定：是否显示确定按钮、确定是否可用、是否显示取消按钮
     * ================================================================ */
    {
        int show_confirm = 0;
        int confirm_enabled = 0;
        int show_cancel = 0;

        switch(game->resp_state)
        {
        /* ---- 有确定+取消按钮的状态 ---- */
        case RESPONSE_NEED_WUXIE:
        case RESPONSE_NEED_BASIC:
            /* 通用响应：总是显示按钮，input.c里会判断是否是玩家需要响应 */
            show_confirm = 1;
            confirm_enabled = game->response_pick_selected;
            show_cancel = 1;
            break;
        case RESPONSE_NEED_ZHUQUE:
            show_confirm = 1;
            confirm_enabled = game->zhuque_selected;
            show_cancel = 1;
            break;
        case RESPONSE_NEED_ZHANGBA:
            show_confirm = 1;
            confirm_enabled = (game->zhangba_selected_count == 2);
            show_cancel = 1;
            break;
        case RESPONSE_NEED_BAGUA:
            show_confirm = 1;
            confirm_enabled = game->bagua_selected;
            show_cancel = 1;
            break;
        case RESPONSE_NEED_FEIWUU_PICK:
            show_confirm = 1;
            /* 化蝶0张或1张都能确认，飞舞0张也能确认 */
            confirm_enabled = 1;
            show_cancel = 1;
            break;
        case RESPONSE_NEED_GILGAMESH_SUOJIAN:
        case RESPONSE_NEED_GILGAMESH_GUAILI:
        case RESPONSE_NEED_GILGAMESH_TIANPI_TARGET:
        case RESPONSE_NEED_GILGAMESH_TIANPI_RANK:
            show_cancel = 1;
            break;
        case RESPONSE_NEED_GENERIC_DISCARD:
            if(!game->guanshi_active)
            {
                show_confirm = 1;
                confirm_enabled = (game->generic_discard_selected_count == game->generic_discard_need);
                /* 弃牌阶段（source=201）不能取消，其他情况可以取消 */
                if(game->generic_discard_source != 201)
                    show_cancel = 1;
            }
            break;
        case RESPONSE_NEED_CONFIRM_PLAY:
            show_confirm = 1;
            confirm_enabled = 1;
            show_cancel = 1;
            break;
        case RESPONSE_NEED_TIESUO_TARGET:
            show_confirm = 1;
            confirm_enabled = (game->tiesuo_target_count > 0);
            show_cancel = 1;
            break;
        case RESPONSE_NEED_GROUP_TARGET:
            show_confirm = 1;
            confirm_enabled = 1;  /* 始终可以点击确定指定下一个目标 */
            show_cancel = 1;
            break;
        case RESPONSE_NEED_HUAXING_SUIT:
        case RESPONSE_NEED_HUAXING_HAND:
        case RESPONSE_NEED_HUAXING_TRICK:
            show_cancel = 1;
            break;
        case RESPONSE_NEED_HUOGONG_PICK:
            show_confirm = 1;
            confirm_enabled = (game->huogong_picked_hand >= 0);
            show_cancel = 1;
            break;
        case RESPONSE_NEED_LINYUXIA_YUZHAN:
            show_confirm = 1;
            confirm_enabled = 1;
            show_cancel = 1;
            break;

        /* ---- 只有取消按钮的状态 ---- */
        case RESPONSE_NEED_TARGET:
        case RESPONSE_NEED_GUANSHI:
        case RESPONSE_NEED_HANBING:
            show_cancel = 1;
            break;

        default:
            break;
        }

        int btn_w = 140;
        int btn_h = 45;
        int btn_y = WINDOW_HEIGHT / 2 + 100;
        int confirm_x = WINDOW_WIDTH / 2 + 10;
        int cancel_x = WINDOW_WIDTH / 2 - btn_w - 10;
        int chongzhu_x = WINDOW_WIDTH / 2 - btn_w * 2 - 30;  /* 重铸按钮在最左边 */

        /* 铁索连环：显示重铸按钮 */
        if(game->resp_state == RESPONSE_NEED_TIESUO_TARGET)
        {
            SDL_SetRenderDrawColor(ren, 50, 100, 150, 255);  /* 蓝色 */
            SDL_Rect chongzhu_btn = {chongzhu_x, btn_y, btn_w, btn_h};
            SDL_RenderFillRect(ren, &chongzhu_btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &chongzhu_btn);
            render_text_center(ctx, "重铸", chongzhu_x + btn_w / 2, btn_y + 10,
                               ctx->font_normal, ctx->color_white);
            render_hover_glow(ctx, chongzhu_x, btn_y, btn_w, btn_h,
                              game->mouse_x, game->mouse_y);
        }

        /* 确定按钮 */
        if(show_confirm)
        {
            if(confirm_enabled)
                SDL_SetRenderDrawColor(ren, 0, 150, 0, 255);  /* 绿色 */
            else
                SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);  /* 灰色 */
            SDL_Rect confirm_btn = {confirm_x, btn_y, btn_w, btn_h};
            SDL_RenderFillRect(ren, &confirm_btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &confirm_btn);
            render_text_center(ctx, "确定", confirm_x + btn_w / 2, btn_y + 10,
                               ctx->font_normal, ctx->color_white);
            render_hover_glow(ctx, confirm_x, btn_y, btn_w, btn_h,
                              game->mouse_x, game->mouse_y);
        }

        /* 取消按钮 */
        if(show_cancel)
        {
            SDL_SetRenderDrawColor(ren, 120, 50, 50, 255);  /* 暗红色 */
            SDL_Rect cancel_btn = {cancel_x, btn_y, btn_w, btn_h};
            SDL_RenderFillRect(ren, &cancel_btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &cancel_btn);
            render_text_center(ctx, "取消", cancel_x + btn_w / 2, btn_y + 10,
                               ctx->font_normal, ctx->color_white);
            render_hover_glow(ctx, cancel_x, btn_y, btn_w, btn_h,
                              game->mouse_x, game->mouse_y);
        }
    }

    /* ---- 长按技能描述：屏幕中心显示 ---- */
    if(game->long_press_skill_idx >= 0)
    {
        Player* me = &game->players[0];
        if(me->hero && game->long_press_skill_idx < me->hero->skill_count)
        {
            Skill* skill = &me->hero->skills[game->long_press_skill_idx];

            /* 半透明背景框 */
            int box_w = 600;
            int box_h = 280;
            int box_x = WINDOW_WIDTH / 2 - box_w / 2;
            int box_y = WINDOW_HEIGHT / 2 - box_h / 2;
            SDL_SetRenderDrawColor(ren, 20, 20, 40, 240);
            SDL_Rect box = {box_x, box_y, box_w, box_h};
            SDL_RenderFillRect(ren, &box);
            SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);  /* 金色边框 */
            SDL_RenderDrawRect(ren, &box);

            /* 技能名称 */
            char name_buf[64];
            const char* type_str = "";
            if(skill->type == SKILL_LOCKED) type_str = "锁定技";
            else if(skill->type == SKILL_PASSIVE) type_str = "被动技";
            else if(skill->type == SKILL_ACTIVE) type_str = "主动技";
            snprintf(name_buf, sizeof(name_buf), "【%s】%s", type_str, skill->name);
            render_text_center(ctx, name_buf, WINDOW_WIDTH / 2, box_y + 25,
                               ctx->font_normal, ctx->color_yellow);

            /* 分隔线 */
            SDL_SetRenderDrawColor(ren, 255, 215, 0, 150);
            SDL_Rect line = {box_x + 50, box_y + 55, box_w - 100, 1};
            SDL_RenderFillRect(ren, &line);

            /* 技能描述（按UTF-8字节数换行，每行约126字节=42个中文字符） */
            const char* desc = skill->desc;
            int desc_len = strlen(desc);
            int line_height = 24;
            int max_bytes_per_line = 126;  /* 42个中文字符 * 3字节 */
            int lines = (desc_len + max_bytes_per_line - 1) / max_bytes_per_line;
            if(lines > 8) lines = 8;  /* 最多显示8行 */

            for(int i = 0; i < lines; i++)
            {
                char line_buf[256];
                int start = i * max_bytes_per_line;
                int len = desc_len - start;
                if(len > max_bytes_per_line) len = max_bytes_per_line;
                /* 避免在UTF-8字符中间截断：向前找到完整字符边界 */
                while(len > 0 && (desc[start + len] & 0xC0) == 0x80)
                {
                    len--;  /* 回退到完整字符边界 */
                }
                strncpy(line_buf, desc + start, len);
                line_buf[len] = '\0';
                render_text_center(ctx, line_buf, WINDOW_WIDTH / 2, box_y + 75 + i * line_height,
                                   ctx->font_small, ctx->color_white);
            }

            /* 提示文字 */
            render_text_center(ctx, "（点击描述框外关闭）",
                               WINDOW_WIDTH / 2, box_y + box_h - 28,
                               ctx->font_small, (SDL_Color){150, 150, 150, 255});
        }
    }

    /* ---- 龙胆模式：出牌阶段激活时显示取消按钮 ---- */
    {
        Player* me = &game->players[0];
        if(me->hero_id == HERO_ZHAOYUN && me->longdan_active &&
           game->resp_state == RESPONSE_NONE && game->current_player == 0)
        {
            int btn_w = 140;
            int btn_h = 45;
            int btn_x = WINDOW_WIDTH / 2 - btn_w / 2;
            int btn_y = WINDOW_HEIGHT / 2 + 100;

            /* 取消按钮 */
            SDL_SetRenderDrawColor(ren, 120, 50, 50, 255);
            SDL_Rect cancel_btn = {btn_x, btn_y, btn_w, btn_h};
            SDL_RenderFillRect(ren, &cancel_btn);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &cancel_btn);
            render_text_center(ctx, "取消龙胆", btn_x + btn_w / 2, btn_y + 10,
                               ctx->font_normal, ctx->color_white);
            render_hover_glow(ctx, btn_x, btn_y, btn_w, btn_h,
                              game->mouse_x, game->mouse_y);

            /* 提示文字 */
            render_text_center(ctx, "【龙胆】已激活：杀当闪、闪当杀，点击取消退出",
                               WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 70,
                               ctx->font_small, (SDL_Color){255, 215, 0, 255});
        }
    }

    /* ---- 日志弹窗 ---- */
    if(game->log_panel_open)
    {
        int panel_w = 800;
        int panel_h = 500;
        int panel_x = (WINDOW_WIDTH - panel_w) / 2;
        int panel_y = (WINDOW_HEIGHT - panel_h) / 2;

        /* 半透明背景遮罩 */
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
        SDL_RenderFillRect(ren, &(SDL_Rect){0, 0, WINDOW_WIDTH, WINDOW_HEIGHT});

        /* 弹窗背景 */
        SDL_SetRenderDrawColor(ren, 30, 30, 40, 255);
        SDL_Rect panel_rect = {panel_x, panel_y, panel_w, panel_h};
        SDL_RenderFillRect(ren, &panel_rect);
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDrawRect(ren, &panel_rect);

        /* 标题栏 */
        SDL_SetRenderDrawColor(ren, 50, 50, 70, 255);
        SDL_Rect title_rect = {panel_x, panel_y, panel_w, 40};
        SDL_RenderFillRect(ren, &title_rect);
        render_text(ctx, "游戏日志（滚轮翻看，点击外部关闭）",
                     panel_x + 15, panel_y + 10,
                     ctx->font_normal, ctx->color_yellow);

        /* 日志内容区域 */
        int content_y = panel_y + 50;
        int content_h = panel_h - 60;
        int line_h = 18;
        int visible_lines = content_h / line_h;

        /* 计算显示范围：最新的日志在底部 */
        int total = game->log_count;
        int scroll = game->log_scroll;
        if(scroll < 0) scroll = 0;
        if(scroll > total - visible_lines) scroll = total - visible_lines;
        if(scroll < 0) scroll = 0;
        game->log_scroll = scroll;

        int start_idx = total - visible_lines - scroll;
        if(start_idx < 0) start_idx = 0;

        for(int i = 0; i < visible_lines && (start_idx + i) < total; i++)
        {
            int idx = start_idx + i;
            SDL_Color line_color = ctx->color_white;
            /* 最新的一条用黄色高亮 */
            if(idx == total - 1)
                line_color = ctx->color_yellow;
            render_text(ctx, game->log_buf[idx],
                        panel_x + 15, content_y + i * line_h,
                        ctx->font_small, line_color);
        }

        /* 滚动条 */
        if(total > visible_lines)
        {
            int scrollbar_w = 8;
            int scrollbar_x = panel_x + panel_w - 15;
            int scrollbar_h = content_h;
            int thumb_h = scrollbar_h * visible_lines / total;
            if(thumb_h < 20) thumb_h = 20;
            int thumb_y = content_y + (scrollbar_h - thumb_h) * scroll / (total - visible_lines);

            SDL_SetRenderDrawColor(ren, 60, 60, 80, 255);
            SDL_RenderFillRect(ren, &(SDL_Rect){scrollbar_x, content_y, scrollbar_w, scrollbar_h});
            SDL_SetRenderDrawColor(ren, 120, 120, 150, 255);
            SDL_RenderFillRect(ren, &(SDL_Rect){scrollbar_x, thumb_y, scrollbar_w, thumb_h});
        }
    }

    SDL_RenderPresent(ren);
}

int render_get_hand_card_index(const RenderContext* render, const GameState* game, int x, int y) {
    (void)render;  // 暂时未用到

    // 只有当前回合玩家是自己（玩家0）时才处理手牌点击
    if (game->current_player != 0) return -1;

    const Player* me = &game->players[0];
    int count = me->hand_count;
    if (count == 0) return -1;

    // ---- 与 render_game 中绘制玩家手牌完全相同的坐标计算 ----
    int feixiao_x = WINDOW_WIDTH - HERO_WIDTH - 40;
    int feixiao_y = WINDOW_HEIGHT - HERO_HEIGHT - 40;
    int hand_start_x = feixiao_x - count * (CARD_WIDTH + 5) - 20;
    if (hand_start_x < 20) hand_start_x = 20;
    int hand_y = feixiao_y + HERO_HEIGHT - CARD_HEIGHT + 10;

    // 遍历手牌区域
    for (int i = 0; i < count; i++) {
        int card_x = hand_start_x + i * (CARD_WIDTH + 5);
        if (x >= card_x && x <= card_x + CARD_WIDTH &&
            y >= hand_y && y <= hand_y + CARD_HEIGHT) {
            return i;
        }
    }
    return -1;
}
