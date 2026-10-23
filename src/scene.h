#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>

/* 逻辑画布尺寸固定；窗口缩放不会改变地图和面板坐标。 */
#define WIDTH 1280
#define HEIGHT 800

/* 按行存储的 0x00RRGGBB 像素；SDL 按 ARGB8888 上传且不启用透明混合。 */
extern uint32_t pixels[WIDTH * HEIGHT];

/* 读取 game.h 中的状态并重绘整张画布，不推进回合。 */
void drawScene(unsigned int seed, int paused);
/* 将当前画布写为二进制 PPM（P6），不重新绘制；成功返回 1，失败返回 0。 */
int savePreview(const char *fileName);

#endif
