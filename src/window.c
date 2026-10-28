/* SDL handles only the window and keyboard. Game rules remain standard C. */
/* 流程：解析参数 → 创建窗口资源 → 处理事件并重绘 → 按逆序释放资源。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "game.h"
#include "scene.h"
#include "audio.h"

static int resizeScene(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture **texture)
{
    SDL_Rect usable;
    SDL_Texture *replacement;
    int width = sceneWidth, height = sceneHeight;
    int top = 32, left = 8, bottom = 8, right = 8;
    int display = SDL_GetWindowDisplayIndex(window);
    replacement = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                    sceneWidth, sceneHeight);
    if (!replacement) {
        return 0;
    }
    if (SDL_RenderSetLogicalSize(renderer, sceneWidth, sceneHeight) != 0) {
        SDL_DestroyTexture(replacement);
        return 0;
    }
    SDL_DestroyTexture(*texture);
    *texture = replacement;
    /* 优先按原像素尺寸显示，仅在屏幕工作区不足时等比缩小，保留窗口边框。 */
    if (display >= 0 && SDL_GetDisplayUsableBounds(display, &usable) == 0) {
        SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right);
        if (width > usable.w - left - right) {
            width = usable.w - left - right;
            height = width * sceneHeight / sceneWidth;
        }
        if (height > usable.h - top - bottom) {
            height = usable.h - top - bottom;
            width = height * sceneWidth / sceneHeight;
        }
    }
    SDL_SetWindowSize(window, width, height);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    return 1;
}

int main(int argCount, char **argValues)
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture = NULL;
    SDL_Event event;
    int running = 1, count = 0, textureWidth, textureHeight, failed = 0;
    unsigned int seed = 0;
    Uint32 collisionStarted = 0;
    int collisionNotice = 0;
    int configuring = 1, field = 0;
    char levelInput[6] = "", setupError[80] = "";
    /* 可选参数只预填设置页；未提供的字段留空。 */
    if (argCount > 1) {
        char *parseEnd;
        /* parseEnd 必须指向字符串末尾，拒绝 12abc 这样的部分数字输入。 */
        unsigned long value = strtoul(argValues[1], &parseEnd, 10);
        if (!*argValues[1] || *parseEnd || value < 1 || value > 99999) {
            fprintf(stderr, "Level must be 1-99999.\n");
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
    if (argCount > 3) {
        fprintf(stderr, "Usage: parcel_shift [level 1-99999] [forklifts 1-4]\n");
        return 1;
    }
    if (seed != 0) {
        snprintf(levelInput, sizeof levelInput, "%u", seed);
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "%s\n", SDL_GetError());
        return 1;
    }
    sceneWidth = 1280;
    sceneHeight = 800;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    window = SDL_CreateWindow("Parcel Shift", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280,
                              800, SDL_WINDOW_RESIZABLE);
    /* 资源逐级依赖；任一级失败都进入下面的统一清理分支。 */
    renderer = NULL;
    if (window != NULL) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    /* scene.c 在 CPU 像素数组绘图；流式纹理只是把整帧送给 SDL 显示。 */
    if (!renderer || !resizeScene(window, renderer, &texture)) {
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
    textureWidth = sceneWidth;
    textureHeight = sceneHeight;
    /* 游戏只使用按键事件；关闭文本输入，避免输入法接管操作键。 */
    SDL_StopTextInput();
    initAudio();
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                SDL_StopTextInput();
            }
            if (event.type == SDL_QUIT) {
                running = 0;
            }
            /* 忽略系统长按连发，每次按下最多移动一格。 */
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                SDL_Keycode key = event.key.keysym.sym;
                if (configuring) {
                    size_t length = strlen(levelInput);
                    int digit = -1;
                    /* 主键盘与小键盘的数字键编号不同，分别转换为 0..9。
                     * -1 表示这次按键不是数字；field 为 0 时编辑种子，为 1 时编辑叉车数。 */
                    if (key >= SDLK_0 && key <= SDLK_9) {
                        digit = (int)(key - SDLK_0);
                    } else if (key >= SDLK_KP_1 && key <= SDLK_KP_9) {
                        digit = (int)(key - SDLK_KP_1 + 1);
                    } else if (key == SDLK_KP_0) {
                        digit = 0;
                    }
                    setupError[0] = '\0';
                    if (key == SDLK_ESCAPE || key == SDLK_q) {
                        running = 0;
                    } else if (key == SDLK_TAB || key == SDLK_UP || key == SDLK_DOWN) {
                        field = !field;
                    } else if (field == 0 && key == SDLK_BACKSPACE && length > 0) {
                        levelInput[length - 1] = '\0';
                    } else if (field == 0 && key == SDLK_DELETE) {
                        levelInput[0] = '\0';
                    } else if (field == 0 && digit >= 0 && length < sizeof levelInput - 1) {
                        levelInput[length] = (char)('0' + digit);
                        levelInput[length + 1] = '\0';
                    } else if (field == 1 && digit >= 1 && digit <= MAX_FORKLIFTS) {
                        count = digit;
                    } else if (field == 1 && (key == SDLK_BACKSPACE || key == SDLK_DELETE)) {
                        count = 0;
                    } else if (field == 1 && key == SDLK_LEFT) {
                        if (count > 1) {
                            count--;
                        } else {
                            count = 1;
                        }
                    } else if (field == 1 && key == SDLK_RIGHT && count < MAX_FORKLIFTS) {
                        count++;
                    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                        /* 输入缓冲区只接受数字，长度不超过五位。
                         * 两个字段都合法后才生成关卡，生成失败时仍停在设置页。 */
                        seed = (unsigned int)strtoul(levelInput, NULL, 10);
                        if (seed < 1 || seed > 99999) {
                            snprintf(setupError, sizeof setupError, "ENTER A SEED FROM 1 TO 99999");
                        } else if (count < 1 || count > MAX_FORKLIFTS) {
                            snprintf(setupError, sizeof setupError,
                                     "ENTER A FORKLIFT COUNT FROM 1 TO 4");
                        } else if (!initialise(count, seed)) {
                            snprintf(setupError, sizeof setupError,
                                     "GENERATION FAILED - CHOOSE ANOTHER SEED");
                        } else {
                            configuring = 0;
                        }
                    }
                    /* 设置页的按键已经处理，不再传给下面的游戏操作分支。 */
                    continue;
                }
                int previousLives = lives;
                int previousTurns = turns;
                int previousCarrying = carrying;
                int previousDelivered = delivered;
                int isMove = key == SDLK_w || key == SDLK_UP || key == SDLK_s || key == SDLK_DOWN ||
                             key == SDLK_a || key == SDLK_LEFT || key == SDLK_d ||
                             key == SDLK_RIGHT;
                if (collisionNotice && key != SDLK_q && key != SDLK_ESCAPE && key != SDLK_r) {
                    continue;
                }
                /* 退出和重开在暂停、胜负结算后也有效；重开沿用原种子。 */
                if (key == SDLK_q || key == SDLK_ESCAPE) {
                    running = 0;
                } else if (key == SDLK_r) {
                    processCommand('r');
                    stopEffects();
                    collisionNotice = 0;
                } else if (key == SDLK_n) {
                    /* 能否换关由共享核心判断，窗口不复制胜负或关卡边界规则。 */
                    processCommand('n');
                    stopEffects();
                } else if (key == SDLK_SPACE) {
                    processCommand('p');
                    if (paused) {
                        stopEffects();
                    }
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
                /* 用操作前后的状态变化选择声音。撞墙、暂停和无效按键不会增加
                 * turns，所以不播放脚步声；同回合只选一种事件音效。 */
                if (isMove && turns > previousTurns) {
                    if (delivered > previousDelivered) {
                        if (delivered >= totalPackages) {
                            playEffect(SFX_VICTORY);
                        } else {
                            playEffect(SFX_DELIVERY);
                        }
                    } else if (lives < previousLives) {
                        playEffect(SFX_CAPTURE);
                    } else if (!previousCarrying && carrying) {
                        playEffect(SFX_PICKUP);
                    } else {
                        playEffect(SFX_STEP);
                    }
                }
                if (lives < previousLives) {
                    collisionStarted = SDL_GetTicks();
                    collisionNotice = 1;
                }
            }
        }
        if (collisionNotice && SDL_GetTicks() - collisionStarted >= COLLISION_NOTICE_MS) {
            collisionNotice = 0;
        }
        /* 暂停和结束时仍重绘；回合只由上面的移动事件推进。 */
        /* N 可能改变当前关卡；不能继续显示启动参数中的旧 seed。 */
        if (configuring) {
            drawSetup(levelInput, count, field, setupError);
        } else {
            drawScene(currentLevel.number, paused);
        }
        /* 开始页进入大地图或 N 换尺寸后，像素画布尺寸会变化。
         * 纹理必须随之重建；普通帧继续使用原纹理。 */
        if (textureWidth != sceneWidth || textureHeight != sceneHeight) {
            if (!resizeScene(window, renderer, &texture)) {
                fprintf(stderr, "%s\n", SDL_GetError());
                failed = 1;
                break;
            }
            textureWidth = sceneWidth;
            textureHeight = sceneHeight;
        }
        if (collisionNotice) {
            drawCollisionNotice();
        }
        /* pitch 是每行字节数；整张像素缓冲区上传到流式纹理。 */
        SDL_UpdateTexture(texture, NULL, pixels, sceneWidth * (int)sizeof pixels[0]);
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
    closeAudio();
    SDL_Quit();
    return failed;
}
