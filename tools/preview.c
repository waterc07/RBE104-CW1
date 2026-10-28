#include "game.h"
#include "scene.h"

/* 无 SDL 的预览入口：验收默认关卡后，绘制初始状态并保存 P6 文件。
 * 本文件独立定义 main，不能与窗口或终端入口一起链接。 */
int main(void)
{
    if (!initialise(2, DEFAULT_LEVEL)) {
        return 1;
    }
    drawScene(DEFAULT_LEVEL, 0);
    return savePreview("preview.ppm") ? 0 : 1;
}
