#ifndef INPUT_H
#define INPUT_H


#include <SDL2/SDL.h>
#include "game.h"
#include "render.h"


/* ===== 输入状态 ===== */
typedef struct {
    int selected_hand_index;  /* 选中的手牌下标，-1表示未选中 */
    int mouse_x;
    int mouse_y;

    /* 长按检测 */
    Uint32 mouse_down_time;    /* 鼠标按下的时间（0表示未按下） */
    int mouse_down_x;
    int mouse_down_y;
    int long_press_skill_idx;  /* 长按显示的技能下标（-1表示没有） */

    /* 双击检测 */
    Uint32 last_click_time;    /* 上次点击时间 */
    int last_click_x;
    int last_click_y;

    /* 日志弹窗拖动滚动 */
    int log_dragging;         /* 是否正在拖动日志 */
    int log_drag_start_y;     /* 拖动起始鼠标Y坐标 */
    int log_drag_start_scroll; /* 拖动起始时的滚动位置 */
} InputState;


/* ===== 函数声明 ===== */
void input_init(InputState* state);
void input_destroy(InputState* state);
void input_handle_event(InputState* state, GameState* game,
                        RenderContext* ctx, SDL_Event* e);
void input_update(InputState* state, GameState* game);  /* 每帧调用，检测长按 */
int input_hit_hand_card(GameState* game, RenderContext* ctx, int mx, int my);
int input_hit_player(GameState* game, int mx, int my);
int input_hit_skill(GameState* game, int mx, int my);
int input_hit_skill_for_long_press(GameState* game, int mx, int my);  /* 长按检测（包括被动技） */
int input_hit_character_button(int mx, int my);  /* 角色选择：检测点击的角色按钮 */

/* 双按钮：确定（右）+ 取消（左） */
int input_hit_confirm_button(int mx, int my);  /* 检测确定按钮 */
int input_hit_cancel_button(int mx, int my);   /* 检测取消按钮 */
int input_hit_chongzhu_button(int mx, int my); /* 检测重铸按钮（铁索连环） */

void input_handle_response_y(GameState* g);
void input_handle_response_n(GameState* g);


#endif /* INPUT_H */
