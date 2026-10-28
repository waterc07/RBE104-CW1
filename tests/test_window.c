/* 向 SDL 队列发送按键，检查正式事件循环；不验证物理键盘和桌面焦点。 */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
static void checkFrame(SDL_Renderer *renderer);
/* 在正式循环每次完成一帧时检查状态，再排入下一组按键。
 * 用宏替换显示调用和入口名，使测试可以复用 window.c 的原始循环。 */
#define SDL_RenderPresent checkFrame
#define main windowMain
#include "../src/window.c"
#undef main
#undef SDL_RenderPresent

static int frame;

static void press(SDL_Keycode key)
{
    /* 只设置事件循环使用的字段；其余字段清零，避免被当成长按事件。 */
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    assert(SDL_PushEvent(&event) == 1);
}

static void checkFrame(SDL_Renderer *renderer)
{
    SDL_RenderPresent(renderer);
    switch (frame++) {
    case 0:
        assert(currentLevel.number == 0);
        press(SDLK_RETURN); /* Both fields empty. */
        break;
    case 1:
        assert(currentLevel.number == 0);
        press(SDLK_0);
        press(SDLK_RETURN);
        break;
    case 2:
        assert(currentLevel.number == 0);
        press(SDLK_DELETE);
        press(SDLK_6);
        press(SDLK_0);
        press(SDLK_0);
        press(SDLK_0);
        press(SDLK_0);
        press(SDLK_RETURN); /* Missing forklift count. */
        break;
    case 3:
        assert(currentLevel.number == 0);
        press(SDLK_TAB);
        press(SDLK_4);
        press(SDLK_BACKSPACE);
        press(SDLK_RETURN); /* Clearing the count makes it required again. */
        break;
    case 4:
        assert(currentLevel.number == 0);
        press(SDLK_LEFT);
        press(SDLK_RIGHT);
        press(SDLK_RIGHT);
        press(SDLK_RIGHT);
        press(SDLK_RETURN);
        break;
    case 5:
        assert(currentLevel.number == 60000 && forkCount == 4 && turns == 0);
        assert(mapRows == 17 && mapCols == 31);
        assert(sceneWidth > 1280 && sceneHeight > 800);
        press(SDLK_SPACE);
        break;
    case 6:
        assert(paused && turns == 0);
        press(SDLK_r);
        break;
    case 7:
        assert(!paused && turns == 0 && currentLevel.number == 60000 && forkCount == 4);
        press(SDLK_ESCAPE);
        break;
    case 8:
        break; /* 退出按键处理后，正式循环还会完成最后一次重绘。 */
    default:
        assert(0 && "window failed to exit");
    }
}

int main(void)
{
    char *args[] = {"parcel_shift", NULL};
    SDL_SetMainReady();
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    assert(windowMain(1, args) == 0);
    assert(frame == 9);
    puts("PASS: blank setup, required fields, editing, start, resize, pause, restart and exit.");
    return 0;
}
