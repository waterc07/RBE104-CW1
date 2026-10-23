#ifndef GAME_H
#define GAME_H

#define ROWS 9
#define COLS 19
#define MAX_FORKLIFTS 4

/* 行列从 0 开始；向下增加 row，向右增加 col。 */
typedef struct {
    int row, col;
} Position;

/* 地图只存墙 #、地面空格、包裹 P 和配送点 D；每行末尾保留 '\0'。 */
extern char map[ROWS][COLS + 1];
/* 机器人和叉车位置单独存储，不覆盖地图中的包裹或配送点。 */
extern Position robot, forklifts[MAX_FORKLIFTS];
/* carrying 为 0/1；totalPackages 是初始总数，包含已配送和正在携带的包裹。 */
extern int forkCount, lives, score, carrying, delivered, totalPackages, turns;
extern char message[160];
extern int paused;

typedef enum {
    GAME_PLAYING,
    GAME_WON,
    GAME_LOST
} GameStatus;

/* 重置整局并设置 rand() 种子；调用方须保证 count 在 1..MAX_FORKLIFTS。 */
void initialise(int count, unsigned int seed);
/* 只接收小写 w/a/s/d；成功移动返回 1，撞墙或无效命令返回 0。
 * 暂停或游戏结束时返回 0。碰撞回合仍算成功移动。 */
int takeTurn(char command);
/* 生命耗尽优先判负，即使本回合已送达全部包裹。 */
GameStatus getGameStatus(void);
/* w/a/s/d 移动，p 暂停/继续，r 使用原 seed 和叉车数重开。
 * 仅成功移动返回 1；退出由前端处理。 */
int processCommand(char command);

#endif
