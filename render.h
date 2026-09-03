#ifndef RENDER_H
#define RENDER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include "game.h"

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

#define CARD_WIDTH   80
#define CARD_HEIGHT  110
#define HERO_WIDTH   150
#define HERO_HEIGHT  210

/* ===== 渲染上下文 ===== */
typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font_small;   /* 小字体 */
    TTF_Font* font_normal;  /* 正常字体 */
    TTF_Font* font_large;   /* 大字体 */

    /* 武将贴图 */
    SDL_Texture* tex_zhaoyun;
    SDL_Texture* tex_feixiao;
    SDL_Texture* hero_texture[8];  /* 8个角色贴图（索引对应HeroId） */

    /* 颜色 */
    SDL_Color color_white;
    SDL_Color color_black;
    SDL_Color color_red;
    SDL_Color color_green;
    SDL_Color color_blue;
    SDL_Color color_yellow;
    SDL_Color color_bg;
} RenderContext;

/* ===== 函数声明 ===== */

/* 初始化渲染 */
int render_init(RenderContext* ctx, const char* res_dir);

/* 销毁渲染 */
void render_destroy(RenderContext* ctx);

/* 渲染整个游戏画面 */
void render_game(RenderContext* ctx, GameState* game);

/* 渲染一张卡牌 */
void render_card(RenderContext* ctx, Card* card, int x, int y, int selected);

/* 渲染文本 */
void render_text(RenderContext* ctx, const char* text, int x, int y,
                 TTF_Font* font, SDL_Color color);

/* 渲染文本居中 */
void render_text_center(RenderContext* ctx, const char* text, int cx, int y,
                        TTF_Font* font, SDL_Color color);

/* 获取文本宽度 */
int render_text_width(RenderContext* ctx, const char* text, TTF_Font* font);

/* 通用悬停发光函数：鼠标移到可点击区域上时微微发亮 */
void render_hover_glow(RenderContext* ctx, int x, int y, int w, int h,
                       int mouse_x, int mouse_y);

/* 通用双按钮渲染：确定（右，绿色/灰色）+ 取消（左，灰色） */
void render_dual_buttons(RenderContext* ctx, int confirm_enabled, int mouse_x, int mouse_y);

// 根据屏幕坐标 (x, y) 返回被点击的手牌索引，失败返回 -1
int render_get_hand_card_index(const RenderContext* render, const GameState* game, int x, int y);

#endif /* RENDER_H */
