#ifndef MENU_H
#define MENU_H

#include <SDL.h>

/* 菜单只保存待应用的配置，不复制关卡生成或规则。
 * 背景缓冲已是 1280×720 的 0x00RRGGBB 像素，可直接交给 scene.c 绘制。 */
typedef struct {
    char level[6];       /* 关卡号文本，由数字键或 +/- 修改。 */
    int count;           /* 追击机器人数量 2..5。 */
    int hover;           /* 鼠标悬停的按钮下标，-1 表示没有。 */
    int focus;           /* 键盘焦点按钮下标，Tab/方向键循环。 */
    int editing;         /* 是否正在输入关卡号。 */
    int replaceInput;    /* 下一次数字输入是否先清空旧关卡号。 */
    int hasGame;         /* 是否有可继续的当前局，决定 RESUME 是否可用。 */
    int busy;            /* 后台验证期间锁住菜单，避免改写共享游戏状态。 */
    const char *error;   /* 最近一次操作的错误提示，NULL 表示不显示。 */
    uint32_t *background; /* 1280×720 背景副本；加载失败时为 NULL。 */
} Menu;

/* 菜单事件只返回这三种动作；其他输入通过 Menu 字段保留。 */
enum { MENU_NONE, MENU_START, MENU_RESUME, MENU_QUIT };

void initMenu(Menu *menu, unsigned int level, int count);
void closeMenu(Menu *menu);
void drawMenu(const Menu *menu);
int menuEvent(Menu *menu, const SDL_Event *event);
unsigned int menuLevel(const Menu *menu);
int menuTier(unsigned int level);

#endif
