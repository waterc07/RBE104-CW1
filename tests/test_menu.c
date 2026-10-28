/* 菜单选择叉车数与关卡种子；这里直接调用 menu.c 的事件与绘制接口。
 * 不打开游戏窗口，因此不验证真实鼠标坐标转换、窗口焦点或实际按键重复率。 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/menu.h"
#include "../src/game.h"
#include "../src/scene.h"

/* 鼠标与文本事件都直接交给 menuEvent；坐标是菜单画布的逻辑坐标。 */
static int click(Menu *menu, int x, int y)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
    return menuEvent(menu, &event);
}

static void typeText(Menu *menu, const char *text)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_TEXTINPUT;
    snprintf(event.text.text, sizeof event.text.text, "%s", text);
    menuEvent(menu, &event);
}

static int pressKey(Menu *menu, SDL_Keycode key)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    return menuEvent(menu, &event);
}

/* 六种地图尺寸各取一个代表关卡，验证 5 辆叉车的验收路线。 */
static void testFiveForkliftRoutes(void)
{
    const unsigned int levels[] = {1, 1000, 10000, 30000, 60000, 99999};
    size_t i;
    for (i = 0; i < sizeof levels / sizeof levels[0]; i++) {
        char route[MAX_SOLUTION + 1];
        int step;
        assert(initialise(5, levels[i]));
        assert(forkCount == 5 && turns == 0 && lives == MAX_LIVES);
        strcpy(route, levelSolution);
        for (step = 0; route[step] && getGameStatus() == GAME_PLAYING; step++) {
            takeTurn(route[step]);
        }
        assert(getGameStatus() == GAME_WON && delivered == totalPackages &&
               score == totalPackages * 100 && lives > 0);
        restartGame();
        assert(forkCount == 5 && turns == 0 && lives == MAX_LIVES && !delivered && !score);
        printf("PASS: five forklifts, level %u, %dx%d, winning route replay.\n", levels[i], mapRows,
               mapCols);
        fflush(stdout);
    }
}

int main(void)
{
    /* 菜单绘制只用像素缓冲和位图；dummy 视频驱动避免依赖桌面环境。 */
    Menu menu;
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    initMenu(&menu, 1, 2);
    assert(menu.background != NULL);
    assert(menuLevel(&menu) == 1 && menu.count == 2);
    assert(!menu.hasGame && !menu.busy && !menu.editing);
    /* 焦点默认在 START，未玩过时 RESUME 禁用。 */
    assert(menu.focus == 13);
    assert(click(&menu, 912, 624) == MENU_NONE);
    assert(menu.error == NULL);

    /* 难度预设同时设置档位、叉车数和地图尺寸；三档分别落在 1000、10000、30000。 */
    click(&menu, 840, 146);
    assert(menuTier(menuLevel(&menu)) == 2 && menu.count == 2 && menuLevel(&menu) == 1000);
    click(&menu, 984, 146);
    assert(menuTier(menuLevel(&menu)) == 3 && menu.count == 3 && menuLevel(&menu) == 10000);
    click(&menu, 1128, 146);
    assert(menuTier(menuLevel(&menu)) == 4 && menu.count == 5 && menuLevel(&menu) == 30000);

    /* 叉车数按钮 2..5 直接改写 count，不改变当前种子。 */
    {
        int i;
        for (i = 0; i < 4; i++) {
            click(&menu, 800 + i * 108, 330);
            assert(menu.count == i + 2);
        }
    }
    assert(menuLevel(&menu) == 30000);
    /* 地图尺寸按钮把当前种子换算到对应档位，保持档内偏移。 */
    click(&menu, 840, 425);
    assert(menuTier(menuLevel(&menu)) == 2);
    click(&menu, 984, 425);
    assert(menuTier(menuLevel(&menu)) == 3);
    click(&menu, 1128, 425);
    assert(menuTier(menuLevel(&menu)) == 4);
    /* 档位边界必须与关卡生成器的尺寸分界一致。 */
    assert(menuTier(0) == 0 && menuTier(1) == 1 && menuTier(999) == 1);
    assert(menuTier(1000) == 2 && menuTier(9999) == 2);
    assert(menuTier(10000) == 3 && menuTier(29999) == 3);
    assert(menuTier(30000) == 4 && menuTier(59999) == 4);
    assert(menuTier(60000) == 5 && menuTier(99999) == 5);
    assert(menuTier(100000) == 0);

    /* 种子输入框：非法 0 不启动；再次编辑会替换旧内容；上下界在按钮处被夹紧。 */
    click(&menu, 900, 230);
    assert(menu.editing);
    typeText(&menu, "0");
    assert(menuLevel(&menu) == 0);
    assert(click(&menu, 984, 559) == MENU_NONE && menu.error != NULL);
    click(&menu, 900, 230);
    typeText(&menu, "99999");
    assert(menuLevel(&menu) == 99999);
    click(&menu, 1170, 230);
    assert(menuLevel(&menu) == 99999);
    click(&menu, 900, 230);
    typeText(&menu, "1");
    click(&menu, 798, 230);
    assert(menuLevel(&menu) == 1);
    /* 编辑时超过五位或在最大长度后继续输入不能溢出缓冲区。 */
    click(&menu, 900, 230);
    typeText(&menu, "123456789");
    assert(menuLevel(&menu) == 12345);

    /* busy 期间所有按钮和键盘输入都被忽略，防止重复启动后台验证。 */
    menu.busy = 1;
    assert(click(&menu, 840, 146) == MENU_NONE && menuLevel(&menu) == 12345);
    assert(click(&menu, 984, 559) == MENU_NONE);
    assert(pressKey(&menu, SDLK_RETURN) == MENU_NONE);
    menu.busy = 0;

    /* 合法种子时 START 返回启动请求；RESUME 只在已有对局时可用，EXIT 始终可用。 */
    click(&menu, 900, 230);
    typeText(&menu, "42");
    assert(click(&menu, 984, 559) == MENU_START);
    menu.hasGame = 1;
    assert(click(&menu, 912, 624) == MENU_RESUME);
    assert(click(&menu, 1128, 624) == MENU_QUIT);
    /* 键盘路径：ESC 有对局时回到 RESUME，无对局时退出；TAB 循环焦点。 */
    assert(pressKey(&menu, SDLK_ESCAPE) == MENU_RESUME);
    menu.hasGame = 0;
    assert(pressKey(&menu, SDLK_ESCAPE) == MENU_QUIT);
    menu.focus = 13;
    pressKey(&menu, SDLK_TAB);
    assert(menu.focus == 14);
    menu.focus = 13;
    menu.editing = 0;
    assert(pressKey(&menu, SDLK_RETURN) == MENU_START);
    menu.focus = 14;
    assert(pressKey(&menu, SDLK_RETURN) == MENU_NONE);

    /* 渲染菜单预览；绘制与命中测试共用同一组按钮坐标。 */
    click(&menu, 1128, 146);
    drawMenu(&menu);
    assert(savePreview("menu_preview.ppm"));
    closeMenu(&menu);
    assert(menu.background == NULL);
    SDL_Quit();

    testFiveForkliftRoutes();
    puts("PASS: menu presets, count and size buttons, seed editing and bounds, busy lock, "
         "START/RESUME/EXIT actions, render and five-forklift routes.");
    return 0;
}
