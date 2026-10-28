#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include "level.h"

/* 固定格子和图标尺寸，地图增大时扩展画布；窗口可按屏幕空间等比缩放。 */
#define TILE_SIZE 44
#define MAX_SCENE_WIDTH (1280 + TILE_SIZE * MAX_COLS - 836)
#define MAX_SCENE_HEIGHT (800 + TILE_SIZE * MAX_ROWS - 396)
extern int sceneWidth, sceneHeight;
void updateSceneSize(void);

/* 按行存储的 0x00RRGGBB 像素；SDL 按 ARGB8888 上传且不启用透明混合。 */
extern uint32_t pixels[MAX_SCENE_WIDTH * MAX_SCENE_HEIGHT];

/* 关卡初始化成功后调用。读取 game.h 状态并重绘，不推进回合或消耗随机数。
 * seed 是当前关卡号，只用于显示；画布随当前有效行列数扩展。 */
void drawScene(unsigned int seed, int paused);
void drawSetup(const char *seed, int count, int field, const char *error);
void drawCollisionNotice(void);
/* 将当前画布写为二进制 PPM（P6），不重新绘制；成功返回 1，失败返回 0。 */
int savePreview(const char *fileName);

#endif
