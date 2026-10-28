#ifndef LEVEL_H
#define LEVEL_H

#include <stdint.h>

/* 数组容量覆盖最大关卡；实际遍历边界使用 Level.rows/cols。 */
#define MAX_ROWS 17
#define MAX_COLS 31
#define MAX_FORKLIFTS 4
/* 首次候选不计入重试次数，因此最多检查 101 张候选地图。 */
#define LEVEL_ATTEMPTS 100
#define MAX_SOLUTION 4096
#define LEVEL_GENERATOR_VERSION 6
#define DEFAULT_LEVEL 1U

/* 零起始行列坐标：row 向下增加，col 向右增加。 */
typedef struct {
    int row, col;
} Position;

/* 保存关卡初始数据。拾取包裹只修改游戏地图，不修改这里的 cells。 */
typedef struct {
    int rows, cols;
    /* 每个有效行在 cells[row][cols] 放置字符串终止符。 */
    char cells[MAX_ROWS][MAX_COLS + 1];
    Position start, forkStarts[MAX_FORKLIFTS];
    unsigned int number;     /* 用户输入的关卡号，也是叉车初始 seed。 */
    uint32_t generationSeed; /* 本次候选的内部 seed，不用于选择尺寸。 */
    int attempt;             /* 0 为首次生成，1..100 为内部重试。 */
} Level;

/* 修改调用方持有的随机状态；生成器与游戏分别持有状态，互不消耗序列。 */
uint32_t nextRandom(uint32_t *state);
/* 有效编号返回 1 并写出尺寸；编号越界返回 0，不修改输出。 */
int levelDimensions(unsigned int number, int *rows, int *cols);
/* 9×15 为 3 个；9×19、11×23 为 4 个；13×27、17×31 为 5 个。无效编号返回 0。 */
int levelPackageCount(unsigned int number);
/* 生成候选而非已验收地图；返回 1 后仍须做结构检查和实际通关验证。 */
int generateLevel(Level *level, unsigned int number, int attempt);
/* 只检查静态结构，不模拟叉车。error 可为 NULL；失败返回 0。 */
int validateLevel(const Level *level, char *error, int errorSize);

#endif
