/* 开始菜单只选择原有种子、叉车数和地图档位，不复制关卡生成或追踪规则。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"
#include "scene.h"

/* 地图档位 1..5 对应原有尺寸分段；档位 1 和 5 只能直接输入关卡号进入，
 * 菜单的下拉按钮只提供 2..4 档，不新增另一套尺寸生成逻辑。 */
static const unsigned int first[] = {0, 1, 1000, 10000, 30000, 60000};
static const unsigned int last[] = {0, 999, 9999, 29999, 59999, 99999};

/* 绘制和鼠标命中必须使用同一张按钮表，否则缩放后点击位置会与画面不一致。 */
static const SDL_Rect controls[] = {
    /* 难度预设：EASY / NORMAL / HARD。 */
    {774, 125, 132, 42}, {918, 125, 132, 42}, {1062, 125, 132, 42},
    /* 关卡号：减号、输入框、加号。 */
    {774, 215, 48, 46}, {834, 215, 300, 46}, {1146, 215, 48, 46},
    /* 追击机器人数量 2..5。 */
    {774, 312, 96, 46}, {882, 312, 96, 46}, {990, 312, 96, 46}, {1098, 312, 96, 46},
    /* 地图档位 2..4。 */
    {774, 408, 132, 46}, {918, 408, 132, 46}, {1062, 408, 132, 46},
    /* 开始、继续、退出。 */
    {774, 530, 420, 58}, {774, 606, 276, 36}, {1062, 606, 132, 36}
};

unsigned int menuLevel(const Menu *menu)
{
    /* 空输入框按 0 处理，由开始按钮的统一校验报告范围错误。 */
    return (unsigned int)strtoul(menu->level, NULL, 10);
}

int menuTier(unsigned int level)
{
    int tier;
    /* 档位边界与 levelDimensions() 使用同一组关卡区间。 */
    for (tier = 1; tier <= 5; tier++) {
        if (level >= first[tier] && level <= last[tier]) {
            return tier;
        }
    }
    return 0;
}

static void setLevel(Menu *menu, unsigned int value)
{
    /* 越界输入夹到有效范围，输入框最多保留五位数字。 */
    if (value < 1) {
        value = 1;
    }
    if (value > 99999) {
        value = 99999;
    }
    snprintf(menu->level, sizeof menu->level, "%u", value);
    menu->error = NULL;
}

static void setTier(Menu *menu, int tier)
{
    unsigned int seed = menuLevel(menu);
    int oldTier = menuTier(seed);
    unsigned int offset;
    /* 换档时尽量保留关卡在原区间内的相对位置，避免从最大地图切回时总是回到
     * 区间起点；原关卡号不在任何档位时从新区间第一关开始。 */
    offset = oldTier ? seed - first[oldTier] : 0;
    setLevel(menu, first[tier] + offset % (last[tier] - first[tier] + 1));
}

void initMenu(Menu *menu, unsigned int level, int count)
{
    char path[2048];
    char *base;
    SDL_Surface *image, *converted;
    int row;
    memset(menu, 0, sizeof *menu);
    menu->count = count;
    menu->focus = 13; /* 默认焦点在 START DELIVERY，回车即可开始。 */
    menu->hover = -1;
    setLevel(menu, level);
    /* 背景从 EXE 旁的 assets/ 读取，不依赖启动时的工作目录。 */
    base = SDL_GetBasePath();
    snprintf(path, sizeof path, "%sassets/menu_background.bmp", base ? base : "");
    SDL_free(base);
    image = SDL_LoadBMP(path);
    if (!image) {
        fprintf(stderr, "Menu background: %s\n", SDL_GetError());
        return;
    }
    converted = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(image);
    if (!converted) {
        return;
    }
    /* 菜单画布固定为 1280×720；尺寸不符时仍用纯色背景，不拉伸图片。
     * 逐行复制以去掉 SDL 表面的行间距，后面的绘制只处理连续像素。 */
    if (converted->w == 1280 && converted->h == 720) {
        menu->background = malloc(1280 * 720 * sizeof *menu->background);
        if (menu->background) {
            for (row = 0; row < 720; row++) {
                memcpy(menu->background + row * 1280,
                       (Uint8 *)converted->pixels + row * converted->pitch,
                       1280 * sizeof *menu->background);
            }
        }
    }
    SDL_FreeSurface(converted);
}

void closeMenu(Menu *menu)
{
    free(menu->background);
    menu->background = NULL;
}

static void button(const Menu *menu, int id, const char *label, int selected, int disabled)
{
    SDL_Rect rect = controls[id];
    int highlighted = !disabled && (id == menu->hover || id == menu->focus);
    int scale = id == 13 || id == 4 ? 3 : 2;
    uint32_t edge, fill;
    /* 三种状态同时出现时以禁用优先；选中项用紫色底和底部亮条标出，
     * 悬停或键盘焦点只提亮边框，不改变选中含义。 */
    if (disabled) {
        edge = 0x30263D;
        fill = 0x1B1527;
    } else if (selected) {
        edge = highlighted ? 0xE8D6FF : 0xBB8CFF;
        fill = 0x67419A;
    } else {
        edge = highlighted ? 0xE8D6FF : 0x574269;
        fill = highlighted ? 0x493061 : 0x281D39;
    }
    menuRect(rect.x, rect.y + 4, rect.w, rect.h, 0x0D0817);
    menuRect(rect.x, rect.y, rect.w, rect.h, edge);
    menuRect(rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4, fill);
    if (selected) {
        menuRect(rect.x + 8, rect.y + rect.h - 5, rect.w - 16, 2, 0xC9A6FF);
    }
    menuText(rect.x + (rect.w - (int)strlen(label) * 6 * scale) / 2,
             rect.y + (rect.h - 7 * scale) / 2, label, scale,
             disabled ? 0x695E79 : 0xF7EFFF);
}

void drawMenu(const Menu *menu)
{
    int i, rows = 0, cols = 0, tier = menuTier(menuLevel(menu));
    char info[80];
    const char *sizes[] = {"2  9X19", "3 11X23", "4 13X27"};
    menuCanvas(menu->background);
    /* 左侧保留背景插画，右侧用暗色面板承载真实控件；文字和按钮都由
     * scene.c 的像素绘制函数生成，不增加字体库依赖。 */
    menuRect(48, 42, 8, 96, 0xB88BFA);
    menuText(72, 48, "PARCEL", 7, 0xF5ECFF);
    menuText(72, 108, "SHIFT", 7, 0xD3AAFF);
    menuText(74, 172, "A LITTLE BOT. A BIG DELIVERY.", 2, 0xD4BFED);
    menuRect(742, 40, 488, 624, 0x61477E);
    menuRect(744, 42, 484, 620, 0x171020);
    menuRect(774, 66, 6, 25, 0x9CF2D4);
    menuText(794, 67, "MISSION SETUP", 3, 0xF0E5FF);
    menuText(774, 102, "DIFFICULTY PRESETS", 1, 0xB1A0C8);
    /* 预设只是现有参数的快捷组合，不改变追踪或碰撞规则。 */
    button(menu, 0, "EASY", tier == 2 && menu->count == 2, menu->busy);
    button(menu, 1, "NORMAL", tier == 3 && menu->count == 3, menu->busy);
    button(menu, 2, "HARD", tier == 4 && menu->count == 5, menu->busy);
    menuText(774, 189, "LEVEL / SEED", 2, 0xCAB7E0);
    menuText(1038, 194, "1 - 99999", 1, 0x9C89B4);
    button(menu, 3, "-", 0, menu->busy);
    button(menu, 4, menu->level[0] ? menu->level : "0", menu->editing, menu->busy);
    button(menu, 5, "+", 0, menu->busy);
    if (menu->editing) {
        menuText(846, 266, "TYPE NUMBER / BACKSPACE TO EDIT", 1, 0xBFA7DC);
    }
    menuText(774, 286, "PURSUIT ROBOTS", 2, 0xCAB7E0);
    for (i = 0; i < 4; i++) {
        snprintf(info, sizeof info, "%d", i + 2);
        button(menu, 6 + i, info, menu->count == i + 2, menu->busy);
    }
    menuText(774, 382, "MAP SIZE", 2, 0xCAB7E0);
    for (i = 0; i < 3; i++) {
        button(menu, 10 + i, sizes[i], tier == i + 2, menu->busy);
    }
    /* 档位信息直接读取共享生成模块，不在这里重复尺寸或包裹规则。 */
    if (tier) {
        levelDimensions(menuLevel(menu), &rows, &cols);
        snprintf(info, sizeof info, "TIER %d / %dX%d / %d PARCELS", tier, rows, cols,
                 levelPackageCount(menuLevel(menu)));
        menuText(774, 472, info, 2, 0xA8E8D0);
        snprintf(info, sizeof info, "SEED RANGE %u - %u / SIZE FOLLOWS SEED", first[tier],
                 last[tier]);
        menuText(774, 497, info, 1, 0xA28DBA);
    } else {
        menuText(774, 480, "ENTER A LEVEL FROM 1 TO 99999", 2, 0xFDA7BA);
    }
    button(menu, 13, menu->busy ? "PREPARING..." : "START DELIVERY", 1, menu->busy);
    button(menu, 14, "RESUME", 0, !menu->hasGame || menu->busy);
    button(menu, 15, "EXIT", 0, menu->busy);
    if (menu->error) {
        menuRect(742, 670, 488, 28, 0x3B1631);
        menuText(756, 680, menu->error, 1, 0xFFD5E0);
    }
    menuRect(48, 616, 624, 72, 0x190F29);
    menuText(64, 631, "COLLECT. RETURN HOME. OUTSMART THE CHASE.", 2, 0xE3CEF6);
    menuText(64, 661, "WASD MOVE / M MENU / F11 FULLSCREEN / TAB SELECT", 1, 0xB69DCB);
    if (menu->busy) {
        menuText(774, 650, "CHECKING A WINNING ROUTE...", 1, 0xA8E8D0);
    }
}

static int activate(Menu *menu, int id)
{
    unsigned int seed = menuLevel(menu);
    /* 验证线程会改写共享游戏状态，期间所有按钮只更新悬停，不提交新配置。 */
    if (menu->busy) {
        return MENU_NONE;
    }
    menu->error = NULL;
    menu->editing = id == 4;
    /* 第一次点击输入框时整段替换旧关卡号，之后连续输入只追加数字。 */
    menu->replaceInput = menu->editing;
    if (menu->editing) {
        SDL_StartTextInput();
    } else {
        SDL_StopTextInput();
    }
    if (id >= 0 && id <= 2) {
        setTier(menu, id + 2);
        menu->count = id == 2 ? 5 : id + 2;
    } else if (id == 3) {
        setLevel(menu, seed > 1 ? seed - 1 : 1);
    } else if (id == 5) {
        setLevel(menu, seed < 99999 ? seed + 1 : 99999);
    } else if (id >= 6 && id <= 9) {
        menu->count = id - 4;
    } else if (id >= 10 && id <= 12) {
        setTier(menu, id - 8);
    } else if (id == 13) {
        if (seed < 1 || seed > 99999) {
            menu->error = "PLEASE ENTER A LEVEL FROM 1 TO 99999.";
        } else {
            return MENU_START;
        }
    } else if (id == 14 && menu->hasGame) {
        return MENU_RESUME;
    } else if (id == 15) {
        return MENU_QUIT;
    }
    return MENU_NONE;
}

int menuEvent(Menu *menu, const SDL_Event *event)
{
    int i;
    if (menu->busy) {
        return MENU_NONE;
    }
    if (event->type == SDL_MOUSEMOTION ||
        (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)) {
        SDL_Point point;
        /* 开启逻辑尺寸后 SDL 已把鼠标坐标换算到 1280×720 画布，
         * 所以这里与按钮表直接比较，不再按窗口实际大小缩放。 */
        if (event->type == SDL_MOUSEMOTION) {
            point.x = event->motion.x;
            point.y = event->motion.y;
        } else {
            point.x = event->button.x;
            point.y = event->button.y;
        }
        menu->hover = -1;
        for (i = 0; i < 16; i++) {
            if (SDL_PointInRect(&point, &controls[i])) {
                menu->hover = i;
            }
        }
        if (event->type == SDL_MOUSEBUTTONDOWN && menu->hover >= 0) {
            menu->focus = menu->hover;
            return activate(menu, menu->focus);
        }
    } else if (event->type == SDL_TEXTINPUT && menu->editing) {
        const char *character;
        /* 只接受数字，忽略输入法送来的其他字符；最多五位。 */
        for (character = event->text.text; *character; character++) {
            if (*character >= '0' && *character <= '9') {
                size_t length;
                if (menu->replaceInput) {
                    menu->level[0] = '\0';
                    menu->replaceInput = 0;
                }
                length = strlen(menu->level);
                if (length < 5) {
                    menu->level[length] = *character;
                    menu->level[length + 1] = '\0';
                }
            }
        }
        menu->error = NULL;
    } else if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        SDL_Keycode key = event->key.keysym.sym;
        /* Esc 在有当前局时等同于返回游戏，没有当前局时退出，避免误丢进度。 */
        if (key == SDLK_ESCAPE) {
            return menu->hasGame ? MENU_RESUME : MENU_QUIT;
        }
        if (key == SDLK_TAB) {
            int step = (event->key.keysym.mod & KMOD_SHIFT) ? 15 : 1;
            menu->focus = (menu->focus + step) % 16;
            menu->editing = 0;
            SDL_StopTextInput();
        } else if (key == SDLK_BACKSPACE && menu->editing) {
            size_t length = strlen(menu->level);
            if (menu->replaceInput) {
                menu->level[0] = '\0';
            } else if (length > 0) {
                menu->level[length - 1] = '\0';
            }
            menu->replaceInput = 0;
        } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            return activate(menu, menu->editing ? 13 : menu->focus);
        } else if (key == SDLK_SPACE && !menu->editing) {
            return activate(menu, menu->focus);
        }
    }
    return MENU_NONE;
}
