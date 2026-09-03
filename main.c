#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "game.h"
#include "render.h"
#include "input.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    /* 设置控制台为 UTF-8 编码，避免中文调试信息乱码 */
    system("chcp 65001 >nul");

    /* 资源目录：程序所在目录下的res文件夹 */
    const char* res_dir = "res";

    /* 初始化游戏（使用静态变量避免栈溢出，GameState包含200条日志缓冲区约25KB） */
    static GameState game;
    game_init(&game);

    /* 初始化渲染 */
    static RenderContext render;
    if (render_init(&render, res_dir) < 0) {
        fprintf(stderr, "Failed to initialize render.\n");
        game_destroy(&game);
        return 1;
    }

    /* 初始化输入 */
    static InputState input;
    input_init(&input);

    /* 主循环 */
    int running = 1;
    SDL_Event e;

    while (running) {
        /* 事件处理 */
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }

            input_handle_event(&input, &game, &render, &e);

           /* --------键盘按键处理（空格已由input_handle_event处理，响应已改用鼠标点击）-------- */
        }

    game_update(&game);

    /* 倒计时更新（每帧约16ms） */
    game_countdown_update(&game, 0.016f);

    /* 输入更新：检测长按 */
    input_update(&input, &game);

    /* 渲染 */
    render_game(&render, &game);

    /* 简单帧率控制 */
    SDL_Delay(16);
}

    /* 清理：原来bug：input_init 改成 input_destroy */
    input_destroy(&input);
    render_destroy(&render);
    game_destroy(&game);

    return 0;
}
