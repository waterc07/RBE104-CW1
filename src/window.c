/* SDL 只负责窗口、输入和显示；游戏规则仍由 game.c 统一处理。 */
/* 流程：解析参数 → 创建窗口资源 → 菜单验证关卡 → 游戏循环 → 按逆序释放资源。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "game.h"
#include "scene.h"
#include "audio.h"
#include "menu.h"

/* 关卡验证会调用共享求解器，可能持续数秒。放到后台线程，主线程在菜单里
 * 只绘制菜单，不读取局内状态；done 用 SDL 原子变量通知结果已写好。 */
typedef struct {
    int count, result;
    unsigned int seed;
    SDL_atomic_t done;
} LoadJob;

static int loadLevel(void *data)
{
    LoadJob *job = data;
    job->result = initialise(job->count, job->seed);
    SDL_AtomicSet(&job->done, 1);
    return 0;
}

static int resizeScene(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture **texture)
{
    SDL_Rect usable;
    SDL_Texture *replacement;
    int width = sceneWidth, height = sceneHeight;
    int display = SDL_GetWindowDisplayIndex(window);
    int top = 32, left = 8, bottom = 8, right = 8;
    replacement = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, sceneWidth, sceneHeight);
    if (!replacement) {
        return 0;
    }
    /* 逻辑尺寸让 SDL 把窗口坐标换算成画布坐标，鼠标点击和缩放后仍对得上。 */
    if (SDL_RenderSetLogicalSize(renderer, sceneWidth, sceneHeight) != 0) {
        SDL_DestroyTexture(replacement);
        return 0;
    }
    SDL_DestroyTexture(*texture);
    *texture = replacement;
    /* 全屏时由系统拉伸，不再改窗口大小；窗口化时优先按原像素显示，
     * 屏幕工作区不足才等比缩小，保留窗口边框和宽高比。 */
    if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)) {
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
    }
    return 1;
}

int main(int argCount, char **argValues)
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture = NULL;
    SDL_Thread *loader = NULL;
    SDL_Event event;
    LoadJob job;
    Menu menu;
    int running = 1, count = 2, textureWidth = 0, textureHeight = 0, failed = 0;
    int inMenu = 1, collisionNotice = 0, victoryActive = 0;
    unsigned int seed = DEFAULT_LEVEL;
    Uint32 collisionStarted = 0, victoryStarted = 0;
    SDL_zero(job);
    /* 命令行参数只预填菜单；仍需在菜单里确认后开始。 */
    if (argCount > 1) {
        char *parseEnd;
        unsigned long value = strtoul(argValues[1], &parseEnd, 10);
        if (!*argValues[1] || *parseEnd || value < 1 || value > 99999) {
            fprintf(stderr, "Level must be 1-99999.\n");
            return 1;
        }
        seed = (unsigned int)value;
    }
    if (argCount > 2) {
        if (argValues[2][0] < '1' || argValues[2][0] > '5' || argValues[2][1]) {
            fprintf(stderr, "Pursuit robots must be 1-5.\n");
            return 1;
        }
        count = argValues[2][0] - '0';
    }
    if (argCount > 3) {
        fprintf(stderr, "Usage: parcel_shift [level 1-99999] [robots 1-5]\n");
        return 1;
    }
    /* 高 DPI 屏幕上保持像素精确：先声明 per-monitor 感知再初始化视频，
     * 否则 Windows 会把 1280×720 窗口按显示缩放放大，可能超出工作区并裁剪菜单。 */
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "%s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    window = SDL_CreateWindow("Parcel Shift", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280,
                              720, SDL_WINDOW_RESIZABLE);
    renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : NULL;
    if (!renderer) {
        fprintf(stderr, "%s\n", SDL_GetError());
        if (window) {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
        return 1;
    }
    initMenu(&menu, seed, count);
    /* 游戏只处理按键；菜单输入关卡号时才临时打开文本输入。 */
    SDL_StopTextInput();
    initAudio();
    while (running) {
        /* 后台验证结束才读取结果，此时 initialise 已不再写共享状态。 */
        if (loader && SDL_AtomicGet(&job.done)) {
            SDL_WaitThread(loader, NULL);
            loader = NULL;
            menu.busy = 0;
            if (job.result) {
                inMenu = 0;
                menu.hasGame = 1;
                collisionNotice = 0;
                stopEffects();
                SDL_StopTextInput();
            } else {
                menu.error = "NO VERIFIED ROUTE. PLEASE TRY ANOTHER LEVEL.";
            }
        }
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
                break;
            }
            /* F11 在菜单和游戏里都可用；改完全屏后强制重建纹理和逻辑尺寸。 */
            if (event.type == SDL_KEYDOWN && !event.key.repeat &&
                event.key.keysym.sym == SDLK_F11) {
                Uint32 flags = SDL_GetWindowFlags(window);
                SDL_SetWindowFullscreen(window,
                                        flags & SDL_WINDOW_FULLSCREEN
                                            ? 0
                                            : SDL_WINDOW_FULLSCREEN_DESKTOP);
                textureWidth = 0;
                continue;
            }
            if (inMenu) {
                int action = menuEvent(&menu, &event);
                if (action == MENU_QUIT) {
                    running = 0;
                    break;
                }
                if (action == MENU_RESUME) {
                    inMenu = 0;
                    collisionNotice = 0;
                    SDL_StopTextInput();
                }
                if (action == MENU_START) {
                    /* 验证期间菜单锁住；线程结束后由循环顶部切到游戏。 */
                    job.count = menu.count;
                    job.seed = menuLevel(&menu);
                    SDL_AtomicSet(&job.done, 0);
                    menu.busy = 1;
                    menu.error = NULL;
                    SDL_StopTextInput();
                    stopEffects();
                    loader = SDL_CreateThread(loadLevel, "level-check", &job);
                    if (!loader) {
                        menu.busy = 0;
                        menu.error = "COULD NOT START LEVEL CHECK. PLEASE RETRY.";
                    }
                }
                continue;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                SDL_StopTextInput();
            }
            /* 获胜后把三个卡片按钮的点击换算成对应按键，复用同一套结算操作。
             * 鼠标坐标已由 SDL 按逻辑尺寸换算，这里只需减去卡片偏移。 */
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
                getGameStatus() == GAME_WON) {
                int x = event.button.x - (sceneWidth - 880) / 2;
                int y = event.button.y - (sceneHeight - 600) / 2;
                SDL_Keycode action = 0;
                if (y >= 539 && y < 577) {
                    if (x >= 64 && x < 304 && currentLevel.number < 99999) {
                        action = SDLK_n;
                    } else if (x >= 320 && x < 560) {
                        action = SDLK_r;
                    } else if (x >= 576 && x < 816) {
                        action = SDLK_m;
                    }
                }
                if (action) {
                    SDL_zero(event);
                    event.type = SDL_KEYDOWN;
                    event.key.keysym.sym = action;
                }
            }
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                SDL_Keycode key = event.key.keysym.sym;
                int previousLives = lives;
                int previousTurns = turns;
                int previousCarrying = carrying;
                int previousDelivered = delivered;
                int isMove = key == SDLK_w || key == SDLK_UP || key == SDLK_s ||
                             key == SDLK_DOWN || key == SDLK_a || key == SDLK_LEFT ||
                             key == SDLK_d || key == SDLK_RIGHT;
                /* 返回菜单不结束当前局；RESUME 会恢复同一状态。 */
                if (key == SDLK_m || key == SDLK_ESCAPE) {
                    inMenu = 1;
                    menu.hasGame = 1;
                    menu.count = forkCount;
                    snprintf(menu.level, sizeof menu.level, "%u", currentLevel.number);
                    menu.error = NULL;
                    menu.editing = 0;
                    menu.focus = 14;
                    menu.hover = -1;
                    collisionNotice = 0;
                    stopEffects();
                    SDL_StopTextInput();
                    continue;
                }
                /* 碰撞提示停留期间只接受退出和重开。 */
                if (collisionNotice && key != SDLK_q && key != SDLK_r) {
                    continue;
                }
                if (key == SDLK_q) {
                    running = 0;
                } else if (key == SDLK_r) {
                    processCommand('r');
                    stopEffects();
                    collisionNotice = 0;
                } else if (key == SDLK_n && getGameStatus() == GAME_WON &&
                           currentLevel.number < 99999) {
                    /* 下一关沿用当前叉车数；99999 由共享核心保护，这里也不派发。 */
                    processCommand('n');
                    stopEffects();
                    collisionNotice = 0;
                } else if (key == SDLK_SPACE) {
                    processCommand('p');
                    if (paused) {
                        stopEffects();
                    }
                } else if (key == SDLK_w || key == SDLK_UP) {
                    processCommand('w');
                } else if (key == SDLK_s || key == SDLK_DOWN) {
                    processCommand('s');
                } else if (key == SDLK_a || key == SDLK_LEFT) {
                    processCommand('a');
                } else if (key == SDLK_d || key == SDLK_RIGHT) {
                    processCommand('d');
                }
                /* 用操作前后的状态差选择声音；撞墙、暂停和无效按键不推进回合，
                 * 因此不会误播脚步声，同一回合只触发一种事件音效。 */
                if (isMove && turns > previousTurns) {
                    if (getGameStatus() == GAME_WON && delivered > previousDelivered) {
                        playEffect(SFX_VICTORY);
                    } else if (lives < previousLives) {
                        playEffect(SFX_CAPTURE);
                    } else if (delivered > previousDelivered) {
                        playEffect(SFX_DELIVERY);
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
        if (!running) {
            break;
        }
        /* 绘制只读状态：菜单只看 Menu 字段，游戏只读 game.h 的全局状态。 */
        if (inMenu) {
            drawMenu(&menu);
            victoryActive = 0;
        } else {
            if (collisionNotice && SDL_GetTicks() - collisionStarted >= COLLISION_NOTICE_MS) {
                collisionNotice = 0;
            }
            drawScene(currentLevel.number, paused);
            /* 通关后计时只用于动画；回合和成绩由共享核心保持不变。 */
            if (getGameStatus() == GAME_WON) {
                if (!victoryActive) {
                    victoryStarted = SDL_GetTicks();
                    victoryActive = 1;
                }
                drawVictoryCelebration(SDL_GetTicks() - victoryStarted);
            } else {
                victoryActive = 0;
            }
            if (collisionNotice) {
                drawCollisionNotice();
            }
        }
        /* 菜单（1280×720）与游戏大地图尺寸不同，画布变化时重建纹理和窗口。 */
        if (textureWidth != sceneWidth || textureHeight != sceneHeight) {
            if (!resizeScene(window, renderer, &texture)) {
                fprintf(stderr, "%s\n", SDL_GetError());
                failed = 1;
                break;
            }
            textureWidth = sceneWidth;
            textureHeight = sceneHeight;
        }
        SDL_UpdateTexture(texture, NULL, pixels, sceneWidth * (int)sizeof pixels[0]);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        /* 固定约 16ms 一帧；动画按真实毫秒计算，但游戏回合只由按键推进。 */
        SDL_Delay(16);
    }
    /* 先等后台验证结束，再释放它可能读取的共享资源和窗口。 */
    if (loader) {
        SDL_WaitThread(loader, NULL);
    }
    closeMenu(&menu);
    closeAudio();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return failed;
}
