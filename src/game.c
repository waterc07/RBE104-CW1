/* RBE104TC learning prototype; AI-assisted. */
/* 终端、SDL、预览和测试共用游戏状态、规则与回合处理。 */
#include <stdlib.h>
#include <string.h>

#include "game.h"

char map[ROWS][COLS + 1];
const char initialMap[ROWS][COLS + 1] = {
    "###################",
    "#D    #     P     #",
    "# ### # ### ### # #",
    "#   #   #       # #",
    "# P ### # ### #   #",
    "#       #     # P #",
    "# ### ##### ### # #",
    "#                 #",
    "###################"
};
Position robot, forklifts[MAX_FORKLIFTS];
const Position start = {1, 1};
const Position forkStarts[MAX_FORKLIFTS] = {{7, 9}, {3, 11}, {5, 11}, {7, 16}};
int forkCount, lives, score, carrying, delivered, totalPackages, turns;
char message[160];
int paused;
static unsigned int gameSeed;

int isFloor(int row, int col)
{
    return row >= 0 && row < ROWS && col >= 0 && col < COLS && map[row][col] != '#';
}

void initialise(int count, unsigned int seed)
{
    int row, col, itemIndex;
    /* 使用地图副本保存拾取结果，重开时恢复原始包裹。 */
    memcpy(map, initialMap, sizeof map);
    robot = start;
    paused = 0;
    gameSeed = seed;
    forkCount = count;
    lives = 3;
    score = carrying = delivered = turns = totalPackages = 0;
    for (itemIndex = 0; itemIndex < forkCount; itemIndex++) {
        forklifts[itemIndex] = forkStarts[itemIndex];
    }
    for (row = 0; row < ROWS; row++) {
        for (col = 0; col < COLS; col++) {
            if (map[row][col] == 'P') {
                totalPackages++;
            }
        }
    }
    /* 同一 C 库内，相同种子和操作序列可复现；不同 C 库不保证一致。 */
    srand(seed);
    strcpy(message, "Collect P, then return to D. Plan one move at a time.");
}

int collision(void)
{
    int itemIndex;
    for (itemIndex = 0; itemIndex < forkCount; itemIndex++) {
        if (robot.row == forklifts[itemIndex].row && robot.col == forklifts[itemIndex].col) {
            return 1;
        }
    }
    return 0;
}

void moveForklifts(void)
{
    const int rowOffsets[4] = {-1, 1, 0, 0};
    const int colOffsets[4] = {0, 0, -1, 1};
    int itemIndex, otherForkliftIndex, directionIndex, row, col, blocked, choices[4], count;
    /* 按数组顺序移动；后面的叉车会看到前面叉车更新后的位置。 */
    for (itemIndex = 0; itemIndex < forkCount; itemIndex++) {
        count = 0;
        for (directionIndex = 0; directionIndex < 4; directionIndex++) {
            row = forklifts[itemIndex].row + rowOffsets[directionIndex];
            col = forklifts[itemIndex].col + colOffsets[directionIndex];
            if (!isFloor(row, col)) {
                continue;
            }
            blocked = 0;
            for (otherForkliftIndex = 0; otherForkliftIndex < forkCount; otherForkliftIndex++) {
                if (otherForkliftIndex != itemIndex && forklifts[otherForkliftIndex].row == row && forklifts[otherForkliftIndex].col == col) {
                    blocked = 1;
                }
            }
            if (!blocked) {
                choices[count++] = directionIndex;
            }
        }
        /* 只从不撞墙、不被其他叉车占用的方向中抽选；无路则原地等待。 */
        if (count > 0) {
            directionIndex = choices[rand() % count];
            forklifts[itemIndex].row += rowOffsets[directionIndex];
            forklifts[itemIndex].col += colOffsets[directionIndex];
        }
    }
}

void handlePackage(void)
{
    /* 拾取后才从地图移除包裹；携带状态在配送时清零。 */
    if (map[robot.row][robot.col] == 'P') {
        if (!carrying) {
            carrying = 1;
            map[robot.row][robot.col] = ' ';
            strcpy(message, "Package collected. Return to D.");
        } else {
            strcpy(message, "Already carrying one; this package stays here.");
        }
    } else if (map[robot.row][robot.col] == 'D' && carrying) {
        carrying = 0;
        delivered++;
        score += 100;
        strcpy(message, "Delivery complete! +100 points.");
    }
}

int takeTurn(char command)
{
    int row = robot.row, col = robot.col, hit, itemIndex;
    if (paused || getGameStatus() != GAME_PLAYING) {
        return 0;
    }
    switch (command) {
        case 'w':
            row--;
            break;
        case 's':
            row++;
            break;
        case 'a':
            col--;
            break;
        case 'd':
            col++;
            break;
        default:
            strcpy(message, "Use one of W A S D, or Q to quit.");
            return 0;
    }
    /* 验证目标格后再修改状态：无效移动不让叉车行动，也不消耗随机数。 */
    if (!isFloor(row, col)) {
        strcpy(message, "Wall: choose another direction. No turn used.");
        return 0;
    }
    robot.row = row;
    robot.col = col;
    turns++;
    strcpy(message, "Move completed.");
    /* 先检查机器人是否走入叉车；未碰撞时才拾取或配送。 */
    hit = collision();
    if (!hit) {
        handlePackage();
    }
    /* 即使上一步已碰撞，叉车仍行动；hit 保留本回合曾发生碰撞的结果。 */
    moveForklifts();
    if (collision()) {
        hit = 1;
    }
    /* 两次检查合并结算，最多扣一命；重生不清空 carrying 和已配送数量。 */
    if (hit) {
        lives--;
        robot = start;
        for (itemIndex = 0; itemIndex < forkCount; itemIndex++) {
            forklifts[itemIndex] = forkStarts[itemIndex];
        }
        strcpy(message, "Collision! -1 life. Back to D; carried package is kept.");
    }
    return 1;
}

GameStatus getGameStatus(void)
{
    /* A fatal collision takes priority over the final delivery. */
    if (lives <= 0) {
        return GAME_LOST;
    }
    return delivered >= totalPackages ? GAME_WON : GAME_PLAYING;
}

int processCommand(char command)
{
    if (command == 'r') {
        initialise(forkCount, gameSeed);
        return 0;
    }
    if (command == 'p') {
        if (getGameStatus() == GAME_PLAYING) {
            paused = !paused;
        }
        return 0;
    }
    return takeTurn(command);
}
