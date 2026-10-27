#ifndef GAME_H
#define GAME_H

#include "level.h"

/* 地图只存墙 #、地面空格、包裹 P 和配送点 D；每行末尾保留 '\0'。 */
extern char map[MAX_ROWS][MAX_COLS + 1];
/* MAX_* 是容量，mapRows/mapCols 才是当前地图的有效边界。 */
extern int mapRows, mapCols;
extern Level currentLevel;
/* 机器人和叉车位置单独存储，不覆盖地图中的包裹或配送点。 */
extern Position robot, forklifts[MAX_FORKLIFTS];
/* carrying 为 0/1；totalPackages 是初始总数，包含已配送和正在携带的包裹。 */
extern int forkCount, lives, score, carrying, delivered, totalPackages, turns;
/* 最近一次行动提示；只用于显示，不参与胜负与搜索状态判定。 */
extern char message[160];
extern int paused;

#define MAX_LIVES 3
typedef enum { HEALTH_NONE, HEALTH_PENDING, HEALTH_ACTIVE, HEALTH_COLLECTED } HealthStatus;
typedef struct {
    Position position;
    int spawnTurn;
    HealthStatus status;
} HealthPickup;
extern HealthPickup healthPickup;

#define SAFE_RADIUS 1
#define DETECTION_STEPS 4
#define CHASE_TURNS 6
#define CHASE_COOLDOWN 4
#define COLLISION_NOTICE_MS 1500
typedef struct {
    int direction, chaseLeft, cooldown;
} ForkliftAI;
extern ForkliftAI forkliftAI[MAX_FORKLIFTS];
int isSafeZone(int row, int col);
void moveForklifts(void);

typedef enum {
    GAME_PLAYING,
    GAME_WON,
    GAME_LOST
} GameStatus;

/* count 为 1..4，levelNumber 为 1..99999；成功返回 1 并开始新局。
 * 结构与四种叉车配置的获胜路线全部通过才接受；失败返回 0，保留旧局。 */
int initialise(int count, unsigned int levelNumber);
/* 当前关卡必须已加载；恢复初始状态，不重新生成或搜索，不改变关卡配置。 */
void restartGame(void);
/* 当前所选叉车数量的验收路线，小写 WASD 字符串，不包含暂停、重开或退出。 */
extern char levelSolution[MAX_SOLUTION + 1];

/* 搜索分支的可变状态副本。关卡、尺寸、叉车数及初始包裹总数在搜索期间
 * 固定，故不放进每个节点；message 只供显示，由求解入口另行保存。 */
typedef struct {
    char board[MAX_ROWS][MAX_COLS + 1];
    Position bot, traffic[MAX_FORKLIFTS];
    ForkliftAI trafficAI[MAX_FORKLIFTS];
    int lifeCount, points, hasPackage, deliveredCount, turnCount, isPaused;
    uint32_t randomState;
    HealthPickup health;
} GameSnapshot;
void captureGame(GameSnapshot *state);
void restoreGame(const GameSnapshot *state);
/* 从当前状态寻找获胜路线，返回 1/0；不论成败都恢复局内状态及提示。
 * 输出缓冲区须有 MAX_SOLUTION+1 字节；失败清空输出，不表示已证明无解。 */
int findWinningRoute(char route[MAX_SOLUTION + 1]);
/* 只接收小写 w/a/s/d；成功移动返回 1，撞墙或无效命令返回 0。
 * 暂停或游戏结束时返回 0。碰撞回合仍算成功移动。 */
int takeTurn(char command);
/* 生命耗尽优先判负，即使本回合已送达全部包裹。 */
GameStatus getGameStatus(void);
/* w/a/s/d 移动，p 暂停/继续，r 重开当前关卡，获胜后 n 进入下一关。
 * 仅成功移动返回 1；退出由前端处理。 */
int processCommand(char command);

#endif
