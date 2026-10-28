/* 候选地图生成与静态检查；本模块不修改正在进行的游戏。 */
#include <stdio.h>
#include <string.h>
#include "level.h"

uint32_t nextRandom(uint32_t *state)
{
    /* uint32_t 赋值保留低 32 位。固定公式避免不同 C 库 rand() 序列的差异。 */
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

int levelDimensions(unsigned int number, int *rows, int *cols)
{
    /* 这里只看对外关卡号。内部重试 seed 即使超过 99999，也不能改变尺寸。 */
    if (number < 1 || number > 99999) {
        return 0;
    }
    if (number < 1000) {
        *rows = 9;
        *cols = 15;
    } else if (number < 10000) {
        *rows = 9;
        *cols = 19;
    } else if (number < 30000) {
        *rows = 11;
        *cols = 23;
    } else if (number < 60000) {
        *rows = 13;
        *cols = 27;
    } else {
        *rows = 17;
        *cols = 31;
    }
    return 1;
}

int levelPackageCount(unsigned int number)
{
    if (number < 1 || number > 99999) {
        return 0;
    }
    if (number < 1000) {
        return 3;
    }
    if (number < 30000) {
        return 4;
    }
    return 5;
}

static int fail(char *error, int size, const char *text)
{
    /* 批量验收可不收集错误文本；交互调用可提供有容量限制的错误缓冲区。 */
    if (error && size > 0) {
        snprintf(error, (size_t)size, "%s", text);
    }
    return 0;
}

int validateLevel(const Level *level, char *error, int errorSize)
{
    int rows, cols, r, c, i, j, head = 0, tail = 0, floors = 0, packages = 0, drops = 0;
    unsigned char seen[MAX_ROWS][MAX_COLS] = {{0}};
    Position queue[MAX_ROWS * MAX_COLS];
    const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
    /* 先核对尺寸再访问地图，避免使用候选提供的错误边界索引数组。 */
    if (!levelDimensions(level->number, &rows, &cols) || rows != level->rows ||
        cols != level->cols) {
        return fail(error, errorSize, "Dimensions do not match level range.");
    }
    /* 这一轮统计全部可走格，而不只统计包裹：后面的 BFS 要拒绝任何孤岛。 */
    for (r = 0; r < rows; r++) {
        if (level->cells[r][cols] != '\0') {
            return fail(error, errorSize, "Missing row terminator.");
        }
        for (c = 0; c < cols; c++) {
            char cell = level->cells[r][c];
            if (cell != '#' && cell != ' ' && cell != 'P' && cell != 'D') {
                return fail(error, errorSize, "Invalid map character.");
            }
            if ((r == 0 || c == 0 || r == rows - 1 || c == cols - 1) && cell != '#') {
                return fail(error, errorSize, "Open map boundary.");
            }
            if (cell != '#') {
                floors++;
            }
            if (cell == 'P') {
                packages++;
            }
            if (cell == 'D') {
                drops++;
            }
        }
    }
    if (packages != levelPackageCount(level->number) || drops != 1) {
        return fail(error, errorSize, "Incorrect package count or delivery point count.");
    }
    r = level->start.row;
    c = level->start.col;
    if (r < 1 || r >= rows - 1 || c < 1 || c >= cols - 1 || level->cells[r][c] != 'D') {
        return fail(error, errorSize, "Invalid robot start.");
    }
    /* 四个候选出生点都须合法，使地图不依赖玩家选择的叉车数量。
     * 叉车只能出生在空地，因此也排除了包裹和配送点重叠。 */
    for (i = 0; i < MAX_FORKLIFTS; i++) {
        r = level->forkStarts[i].row;
        c = level->forkStarts[i].col;
        if (r < 1 || r >= rows - 1 || c < 1 || c >= cols - 1 || level->cells[r][c] != ' ') {
            return fail(error, errorSize, "Invalid forklift start.");
        }
        for (j = 0; j < i; j++) {
            if (r == level->forkStarts[j].row && c == level->forkStarts[j].col) {
                return fail(error, errorSize, "Duplicate forklift start.");
            }
        }
    }
    /* BFS 队列使用 [head, tail)：入队时即标记，保证每格最多入队一次。
     * 队列容量等于最大格子数，不需要动态分配或循环队列。 */
    queue[tail] = level->start;
    tail++;
    seen[level->start.row][level->start.col] = 1;
    while (head < tail) {
        Position p = queue[head];
        head++;
        for (i = 0; i < 4; i++) {
            r = p.row + dr[i];
            c = p.col + dc[i];
            if (r >= 0 && r < rows && c >= 0 && c < cols && !seen[r][c] &&
                level->cells[r][c] != '#') {
                seen[r][c] = 1;
                queue[tail] = (Position){r, c};
                tail++;
            }
        }
    }
    /* 非墙格数量相等意味着包裹、出生点和所有道路都能从配送点走到。
     * 此结论不包含动态叉车风险，后续仍须用正式回合规则求解。 */
    if (tail != floors) {
        return fail(error, errorSize, "Disconnected walkable cells.");
    }
    if (error && errorSize > 0) {
        error[0] = '\0';
    }
    return 1;
}

int generateLevel(Level *level, unsigned int number, int attempt)
{
    Position choices[MAX_ROWS * MAX_COLS], temp;
    int r, c, i, j, count = 0, packages = levelPackageCount(number);
    uint32_t state;
    if (attempt < 0 || attempt > LEVEL_ATTEMPTS) {
        return 0;
    }
    /* 清零未使用的行、列与字段，使保存、复制和回归比较不受旧数据影响。 */
    memset(level, 0, sizeof *level);
    if (!levelDimensions(number, &level->rows, &level->cols)) {
        return 0;
    }
    level->number = number;
    level->attempt = attempt;
    /* 首次使用原编号；每关独占连续 100 个内部 seed，且不占用 1..99999。
     * 尺寸已由 number 确定，下面不再根据 generationSeed 查尺寸范围。 */
    if (attempt == 0) {
        level->generationSeed = number;
    } else {
        level->generationSeed =
            UINT32_C(100000) + (number - 1) * LEVEL_ATTEMPTS + (unsigned int)(attempt - 1);
    }
    state = level->generationSeed;
    level->start = (Position){1, 1};
    for (r = 0; r < level->rows; r++) {
        for (c = 0; c < level->cols; c++) {
            if (r == 0 || c == 0 || r == level->rows - 1 || c == level->cols - 1) {
                level->cells[r][c] = '#';
            } else {
                level->cells[r][c] = ' ';
            }
        }
    }
    /* 只在内部区域添加墙，保留靠近外墙的一圈道路和起点附近空地。
     * 不紧贴上方或左侧已有墙，以限制障碍聚集；这不构成连通性证明。 */
    for (r = 2; r < level->rows - 2; r++) {
        for (c = 2; c < level->cols - 2; c++) {
            if (nextRandom(&state) % 100 < 24 && level->cells[r - 1][c] != '#' &&
                level->cells[r][c - 1] != '#') {
                level->cells[r][c] = '#';
            }
        }
    }
    /* (1,1) 到 (r,c) 的曼哈顿距离为 r+c-2；阈值 7 留出至少五步距离。
     * choices 的前几项分别用于包裹和四个出生点，避免重叠。 */
    for (r = 1; r < level->rows - 1; r++) {
        for (c = 1; c < level->cols - 1; c++) {
            if (level->cells[r][c] == ' ' && r + c >= 7) {
                choices[count++] = (Position){r, c};
            }
        }
    }
    if (count < packages + MAX_FORKLIFTS) {
        return 0;
    }
    /* Fisher-Yates 洗牌：相同 seed 和固定遍历顺序得到相同位置排列。 */
    for (i = count - 1; i > 0; i--) {
        j = (int)(nextRandom(&state) % (uint32_t)(i + 1));
        temp = choices[i];
        choices[i] = choices[j];
        choices[j] = temp;
    }
    level->cells[1][1] = 'D';
    for (i = 0; i < packages; i++) {
        level->cells[choices[i].row][choices[i].col] = 'P';
    }
    for (i = 0; i < MAX_FORKLIFTS; i++) {
        level->forkStarts[i] = choices[i + packages];
    }
    return 1;
}
