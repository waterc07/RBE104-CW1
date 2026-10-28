/* 通过真实 SDL 主循环验证胜利结算：获胜路线、庆祝动画、三个按钮、
 * 下一关同步切换和菜单 RESUME。dummy 驱动只覆盖事件处理与状态，不覆盖真实
 * 键盘、桌面焦点、全屏显示和音频听感。 */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <assert.h>
#include <string.h>
#include <SDL.h>
#include "../src/game.h"
#include "../src/scene.h"

static int stage, frames, step, hold, routeTurns;
static int victoryEffects;
static int effectsAtMenu;
static char route[MAX_SOLUTION + 1];
static GameSnapshot wonState, currentState;
static uint32_t celebrationFirst, celebrationLater;
static int haveFirstCelebration;

static void presentForTest(SDL_Renderer *renderer);

/* 用与正式程序相同的函数名包含 window.c，再把入口和每帧显示调用换成测试钩子。
 * 胜利音效也换成计数钩子，用来验证动画期间不会逐帧重播。 */
#define main gameWindowMain
#define SDL_RenderPresent presentForTest
#define playEffect recordEffect
#include "../src/window.c"
#undef main
#undef SDL_RenderPresent
#undef playEffect

/* audio.h 已被 window.c 包含，SFX_VICTORY 在此可见。 */
void recordEffect(int effect)
{
    if (effect == SFX_VICTORY) {
        victoryEffects++;
    }
}

static void press(SDL_Keycode key)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    assert(SDL_PushEvent(&event) == 1);
}

/* 鼠标事件在正式程序中已被 SDL 转换为逻辑坐标；PushEvent 绕过转换，
 * 因此这里直接写入逻辑坐标。 */
static void click(int x, int y)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
    assert(SDL_PushEvent(&event) == 1);
}

/* 胜利卡片居中：偏移 (sceneWidth-880)/2,(sceneHeight-600)/2；
 * 按钮相对卡片 y 539..577，x 64..304 / 320..560 / 576..816。 */
static void clickCard(int button)
{
    int relativeX;
    if (button == 0) {
        relativeX = 184;
    } else if (button == 1) {
        relativeX = 440;
    } else {
        relativeX = 696;
    }
    click((sceneWidth - 880) / 2 + relativeX, (sceneHeight - 600) / 2 + 558);
}

static SDL_Keycode moveKey(char command)
{
    switch (command) {
    case 'w':
        return SDLK_w;
    case 'a':
        return SDLK_a;
    case 's':
        return SDLK_s;
    case 'd':
        return SDLK_d;
    default:
        assert(0 && "unexpected route command");
        return SDLK_UNKNOWN;
    }
}

/* 碰撞提示期间窗口会丢弃移动键；按回合数确认上一步真正生效后再发下一键，
 * 被丢弃的键重发，因此路线状态与纯核心回放保持一致。 */
static void advanceRoute(void)
{
    if (turns != routeTurns) {
        routeTurns = turns;
        step++;
    }
    assert(route[step]);
    press(moveKey(route[step]));
}

static uint32_t frameFingerprint(void)
{
    uint32_t fingerprint = UINT32_C(2166136261);
    int itemIndex;
    for (itemIndex = 0; itemIndex < sceneWidth * sceneHeight; itemIndex++) {
        fingerprint = (fingerprint ^ pixels[itemIndex]) * UINT32_C(16777619);
    }
    return fingerprint;
}

static void presentForTest(SDL_Renderer *renderer)
{
    SDL_Window *window;
    int logicalWidth, logicalHeight;
    SDL_RenderPresent(renderer);
    assert(++frames < 4000);
    switch (stage) {
    case 0:
        /* 启动时是 720 高的菜单，默认窗口化，并且尚未初始化任何关卡。 */
        assert(sceneHeight == 720);
        window = SDL_RenderGetWindow(renderer);
        assert(window != NULL);
        assert(!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
        SDL_RenderGetLogicalSize(renderer, &logicalWidth, &logicalHeight);
        assert(logicalWidth == 1280 && logicalHeight == 720);
        assert(currentLevel.number == 0 && turns == 0);
        click(984, 559); /* START 按钮中心，菜单逻辑坐标。 */
        stage = 1;
        break;
    case 1:
        if (sceneHeight >= 800) {
            /* START 在后台验证后进入；默认第 1 关、2 辆叉车。 */
            assert(currentLevel.number == 1 && forkCount == 2 && turns == 0);
            assert(getGameStatus() == GAME_PLAYING);
            strcpy(route, levelSolution);
            step = 0;
            routeTurns = turns;
            stage = 2;
        }
        break;
    case 2:
        if (getGameStatus() == GAME_WON) {
            assert(delivered == totalPackages && score == totalPackages * 100);
            /* 首次通关只播一次胜利音效，而不是每个庆祝帧都重播。 */
            assert(victoryEffects == 1);
            captureGame(&wonState);
            hold = 0;
            stage = 3;
        } else {
            assert(getGameStatus() == GAME_PLAYING);
            advanceRoute();
        }
        break;
    case 3:
        /* 庆祝动画不推进回合、成绩或叉车位置；同时动画像素必须随时间变化。 */
        captureGame(&currentState);
        assert(memcmp(&wonState, &currentState, sizeof wonState) == 0);
        assert(victoryEffects == 1);
        if (hold == 15) {
            celebrationFirst = frameFingerprint();
            haveFirstCelebration = 1;
        }
        if (hold == 40) {
            assert(haveFirstCelebration);
            celebrationLater = frameFingerprint();
            assert(celebrationLater != celebrationFirst);
        }
        if (++hold == 45) {
            clickCard(1); /* R 重玩按钮。 */
            stage = 4;
        }
        break;
    case 4:
        if (getGameStatus() == GAME_PLAYING && turns == 0 && delivered == 0) {
            assert(forkCount == 2 && !score && !paused);
            strcpy(route, levelSolution);
            step = 0;
            routeTurns = turns;
            stage = 5;
        }
        break;
    case 5:
        if (getGameStatus() == GAME_WON) {
            captureGame(&wonState);
            effectsAtMenu = victoryEffects;
            hold = 0;
            stage = 6;
        } else {
            assert(getGameStatus() == GAME_PLAYING);
            advanceRoute();
        }
        break;
    case 6:
        /* N 会被碰撞提示屏蔽，等提示结束后再按；窗口内 N 直接执行
         * processCommand('n')，不经过菜单加载线程。 */
        if (++hold < 100) {
            break;
        }
        press(SDLK_n);
        stage = 7;
        break;
    case 7:
        if (currentLevel.number == 2 && getGameStatus() == GAME_PLAYING && turns == 0) {
            assert(forkCount == 2 && sceneHeight >= 800);
            strcpy(route, levelSolution);
            step = 0;
            routeTurns = turns;
            stage = 8;
        }
        break;
    case 8:
        if (getGameStatus() == GAME_WON) {
            captureGame(&wonState);
            effectsAtMenu = victoryEffects;
            hold = 0;
            stage = 9;
        } else {
            assert(getGameStatus() == GAME_PLAYING);
            advanceRoute();
        }
        break;
    case 9:
        clickCard(2); /* M 主菜单按钮。 */
        stage = 10;
        break;
    case 10:
        if (sceneHeight == 720) {
            /* 菜单期间游戏不推进；RESUME 会恢复精确的已获胜状态。 */
            captureGame(&currentState);
            assert(memcmp(&wonState, &currentState, sizeof wonState) == 0);
            assert(victoryEffects == effectsAtMenu);
            click(912, 624); /* RESUME 按钮中心。 */
            stage = 11;
        }
        break;
    case 11:
        if (sceneHeight >= 800) {
            captureGame(&currentState);
            assert(memcmp(&wonState, &currentState, sizeof wonState) == 0);
            assert(getGameStatus() == GAME_WON);
            /* 从菜单恢复已获胜关卡会重新播动画，但不重放胜利音效。 */
            assert(victoryEffects == effectsAtMenu);
            hold = 0;
            stage = 12;
        }
        break;
    case 12:
        captureGame(&currentState);
        assert(memcmp(&wonState, &currentState, sizeof wonState) == 0);
        if (hold == 5) {
            celebrationFirst = frameFingerprint();
            haveFirstCelebration = 1;
        }
        if (hold == 25) {
            assert(haveFirstCelebration);
            celebrationLater = frameFingerprint();
            assert(celebrationLater != celebrationFirst);
        }
        if (++hold == 30) {
            clickCard(0); /* NEXT 按钮：当前 2 关可以进入第 3 关。 */
            stage = 13;
        }
        break;
    case 13:
        if (currentLevel.number == 3 && getGameStatus() == GAME_PLAYING && turns == 0) {
            assert(forkCount == 2);
            press(SDLK_q);
            stage = 14;
        }
        break;
    default:
        break;
    }
}

int main(void)
{
    char *args[] = {"ParcelShift", NULL};
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    SDL_SetMainReady();
    assert(gameWindowMain(1, args) == 0);
    assert(stage == 14);
    puts("PASS: menu START, live winning moves, celebration without state changes, single victory "
         "sound, mouse restart, synchronous next level, mouse menu, exact RESUME and exit.");
    return 0;
}
