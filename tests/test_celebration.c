/* 胜利庆祝的离线检查：直接调用 scene.c 的绘制接口，不打开窗口。
 * 动画只读取经过的毫秒数；这里验证它绘制像素、逐帧变化，并且不改游戏状态。 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "scene.h"

/* 用 FNV-1a 汇总整张画布，便于比较两帧是否真的发生变化。 */
static uint32_t frameFingerprint(void)
{
    uint32_t fingerprint = UINT32_C(2166136261);
    int itemIndex;
    for (itemIndex = 0; itemIndex < sceneWidth * sceneHeight; itemIndex++) {
        fingerprint = (fingerprint ^ pixels[itemIndex]) * UINT32_C(16777619);
    }
    return fingerprint;
}

/* 逐回合执行验收路线；路线来自本次构建的共享核心，不能写死。 */
static void playWinningRoute(const char *route)
{
    int itemIndex;
    for (itemIndex = 0; route[itemIndex] && getGameStatus() == GAME_PLAYING; itemIndex++) {
        takeTurn(route[itemIndex]);
    }
    assert(getGameStatus() == GAME_WON);
}

int main(int argc, char **argv)
{
    GameSnapshot before, after;
    char route[MAX_SOLUTION + 1];
    char path[256];
    uint32_t firstFrame = 0, animationFrame = 0;
    unsigned int frame;
    /* 末尾可选一个目录参数，把逐帧 PPM 写到该目录用于人工查看。 */
    assert(argc <= 2);
    assert(initialise(2, 1));
    strcpy(route, levelSolution);
    playWinningRoute(route);
    captureGame(&before);

    /* 先确认庆祝层确实在静态场景上写入像素，否则后面的帧比较没有意义。 */
    drawScene(currentLevel.number, paused);
    firstFrame = frameFingerprint();
    drawVictoryCelebration(0);
    assert(frameFingerprint() != firstFrame);

    /* 动画帧必须随时间变化，同时任何一帧都不能推进回合、成绩或随机状态。 */
    for (frame = 0; frame < 64; frame++) {
        drawScene(currentLevel.number, paused);
        drawVictoryCelebration((uint32_t)(frame * 50));
        animationFrame = frameFingerprint();
        if (frame == 0) {
            firstFrame = animationFrame;
        }
        if (frame == 10) {
            assert(animationFrame != firstFrame);
        }
        captureGame(&after);
        assert(memcmp(&before, &after, sizeof before) == 0);
        if (argc == 2) {
            snprintf(path, sizeof path, "%s/frame_%03u.ppm", argv[1], frame);
            assert(savePreview(path));
        }
    }

    /* 重开回到游戏态后，绘制庆祝帧仍不能修改局内状态。 */
    processCommand('r');
    assert(getGameStatus() == GAME_PLAYING && turns == 0 && !delivered && !score);
    captureGame(&before);
    drawScene(currentLevel.number, paused);
    drawVictoryCelebration(300);
    captureGame(&after);
    assert(memcmp(&before, &after, sizeof before) == 0);

    /* 最大地图上庆祝卡片按扩展后的画布居中；这里检查尺寸和绘制都不越界。 */
    assert(initialise(5, 99999));
    assert(forkCount == 5);
    strcpy(route, levelSolution);
    playWinningRoute(route);
    assert(delivered == totalPackages);
    captureGame(&before);
    drawScene(currentLevel.number, paused);
    drawVictoryCelebration(600);
    assert(sceneWidth == MAX_SCENE_WIDTH && sceneHeight == MAX_SCENE_HEIGHT);
    assert(frameFingerprint() != 0);
    captureGame(&after);
    assert(memcmp(&before, &after, sizeof before) == 0);
    puts(
        "PASS: real winning route, changing celebration frames, unchanged state, restart and "
        "final large map.");
    return 0;
}
