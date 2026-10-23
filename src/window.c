/* SDL handles only the window and keyboard. Game rules remain standard C. */
/* 流程：解析参数 → 创建窗口资源 → 处理事件并重绘 → 按逆序释放资源。 */
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include "game.h"
#include "scene.h"

int main(int argCount, char **argValues)
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Event event;
    int running = 1, count = 2;
    unsigned int seed = 42;
    /* 可选参数依次为 seed 和叉车数；省略时使用 42、2。 */
    if (argCount > 1) {
        char *parseEnd;
        unsigned long value = strtoul(argValues[1], &parseEnd, 10);
        if (!*argValues[1] || *parseEnd || value > 99999) {
            fprintf(stderr, "Seed must be 0-99999.\n");
            return 1;
        }
        seed = (unsigned int)value;
    }
    if (argCount > 2) {
        if (argValues[2][0] < '1' || argValues[2][0] > '4' || argValues[2][1]) {
            fprintf(stderr, "Forklifts must be 1-4.\n");
            return 1;
        }
        count = argValues[2][0] - '0';
    }
    initialise(count, seed);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "%s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("Parcel Shift", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH,
                              HEIGHT, SDL_WINDOW_RESIZABLE);
    /* 资源逐级依赖；任一级失败都进入下面的统一清理分支。 */
    renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : NULL;
    texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                           SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT)
                       : NULL;
    if (!texture) {
        fprintf(stderr, "%s\n", SDL_GetError());
        if (renderer) {
            SDL_DestroyRenderer(renderer);
        }
        if (window) {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
        return 1;
    }
    /* 缩放时保持 1280×800 逻辑坐标和宽高比，多余窗口区域留边。 */
    SDL_RenderSetLogicalSize(renderer, WIDTH, HEIGHT);
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
            /* 忽略系统长按连发，每次按下最多移动一格。 */
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                SDL_Keycode key = event.key.keysym.sym;
                /* 退出和重开在暂停、胜负结算后也有效；重开沿用原种子。 */
                if (key == SDLK_q || key == SDLK_ESCAPE) {
                    running = 0;
                } else if (key == SDLK_r) {
                    processCommand('r');
                } else if (key == SDLK_SPACE) {
                    processCommand('p');
                } else {
                    if (key == SDLK_w || key == SDLK_UP) {
                        processCommand('w');
                    } else if (key == SDLK_s || key == SDLK_DOWN) {
                        processCommand('s');
                    } else if (key == SDLK_a || key == SDLK_LEFT) {
                        processCommand('a');
                    } else if (key == SDLK_d || key == SDLK_RIGHT) {
                        processCommand('d');
                    }
                }
            }
        }
        /* 暂停和结束时仍重绘；回合只由上面的移动事件推进。 */
        drawScene(seed, paused);
        /* pitch 是每行字节数；整张像素缓冲区上传到流式纹理。 */
        SDL_UpdateTexture(texture, NULL, pixels, WIDTH * (int)sizeof pixels[0]);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        /* 每帧至少等待 16 ms，不使用时间步推进游戏。 */
        SDL_Delay(16);
    }
    /* 先销毁依赖资源，最后关闭 SDL。 */
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
