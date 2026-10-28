/* 通过真实 SDL 主循环验证菜单与游戏切换：无参数启动菜单、START 后台进入、
 * M/ESC 返回菜单、RESUME 精确保留当前局、HARD 预设、F11 全屏和退出。
 * 本文件与 test_menu.c 共同替代已删除的 test_window.c：旧开始设置页的
 * 空字段、必填校验和输入编辑覆盖在 test_menu.c，进入/返回/缩放窗口流程在这里。
 * dummy 驱动只验证事件与状态，不验证真实窗口、焦点或桌面全屏效果。 */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <assert.h>
#include <string.h>
#include <SDL.h>
#include "../src/game.h"
#include "../src/scene.h"

static int stage, frames, menuFrames;
static GameSnapshot savedState, currentState;

static void presentForTest(SDL_Renderer *renderer);

/* 用与正式程序相同的函数名包含 window.c，只替换入口和每帧显示调用。 */
#define main gameWindowMain
#define SDL_RenderPresent presentForTest
#include "../src/window.c"
#undef main
#undef SDL_RenderPresent

static void press(SDL_Keycode key)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    assert(SDL_PushEvent(&event) == 1);
}

/* 鼠标事件在正式程序中已由 SDL 转成逻辑坐标；PushEvent 绕过转换，
 * 因此这里直接写逻辑坐标。 */
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

static int isFullscreen(SDL_Renderer *renderer)
{
    SDL_Window *window = SDL_RenderGetWindow(renderer);
    assert(window != NULL);
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

static void presentForTest(SDL_Renderer *renderer)
{
    int logicalWidth, logicalHeight;
    SDL_RenderPresent(renderer);
    assert(++frames < 4000);
    switch (stage) {
    case 0:
        /* 无参数启动：720 高的菜单、默认窗口化，尚未开始任何一局。 */
        assert(sceneHeight == 720);
        assert(!isFullscreen(renderer));
        SDL_RenderGetLogicalSize(renderer, &logicalWidth, &logicalHeight);
        assert(logicalWidth == 1280 && logicalHeight == 720);
        assert(currentLevel.number == 0 && turns == 0);
        click(984, 559); /* START：后台验证默认第 1 关、2 辆叉车。 */
        stage = 1;
        break;
    case 1:
        if (sceneHeight >= 800) {
            assert(sceneWidth >= 1280);
            assert(currentLevel.number == 1 && forkCount == 2 && turns == 0);
            assert(getGameStatus() == GAME_PLAYING);
            /* 用验收路线的第一步，确保这次按键一定推进一个回合。 */
            press(moveKey(levelSolution[0]));
            stage = 2;
        }
        break;
    case 2:
        assert(turns == 1);
        captureGame(&savedState);
        press(SDLK_m); /* 游戏内 M 返回菜单，保留当前局。 */
        stage = 3;
        break;
    case 3:
        if (sceneHeight == 720) {
            /* 菜单期间游戏状态必须冻结，且 RESUME 恢复同一快照。 */
            captureGame(&currentState);
            assert(memcmp(&savedState, &currentState, sizeof savedState) == 0);
            if (++menuFrames == 3) {
                click(912, 624); /* RESUME 按钮中心。 */
                stage = 4;
            }
        }
        break;
    case 4:
        if (sceneHeight >= 800) {
            captureGame(&currentState);
            assert(memcmp(&savedState, &currentState, sizeof savedState) == 0);
            assert(turns == 1 && getGameStatus() == GAME_PLAYING);
            press(SDLK_F11);
            stage = 5;
        }
        break;
    case 5:
        assert(isFullscreen(renderer));
        captureGame(&currentState);
        assert(memcmp(&savedState, &currentState, sizeof savedState) == 0);
        press(SDLK_F11);
        stage = 6;
        break;
    case 6:
        assert(!isFullscreen(renderer));
        captureGame(&currentState);
        assert(memcmp(&savedState, &currentState, sizeof savedState) == 0);
        press(SDLK_ESCAPE); /* Esc 与 M 一样返回菜单，不退出程序。 */
        stage = 7;
        break;
    case 7:
        if (sceneHeight == 720) {
            captureGame(&currentState);
            assert(memcmp(&savedState, &currentState, sizeof savedState) == 0);
            click(1128, 146); /* HARD 预设：30000 关、5 辆叉车、13x27。 */
            stage = 8;
        }
        break;
    case 8:
        if (sceneHeight == 720) {
            click(984, 559); /* START 使用预设重新验证并进入。 */
            stage = 9;
        }
        break;
    case 9:
        if (sceneHeight >= 800) {
            assert(currentLevel.number == 30000 && forkCount == 5 && turns == 0);
            assert(mapRows == 13 && mapCols == 27 && totalPackages == 5);
            assert(getGameStatus() == GAME_PLAYING);
            press(SDLK_m);
            stage = 10;
        }
        break;
    case 10:
        if (sceneHeight == 720) {
            click(1128, 624); /* EXIT 按钮中心，正常结束主循环。 */
            stage = 11;
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
    assert(stage == 11);
    puts("PASS: windowed menu, asynchronous START, M/ESC to menu, exact-state RESUME, HARD "
         "preset, F11 fullscreen toggle, EXIT (replaces old test_window.c setup-page flow).");
    return 0;
}
