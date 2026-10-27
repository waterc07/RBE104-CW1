/* RBE104TC learning prototype; AI-assisted. */
/* 终端、SDL、预览和测试共用游戏状态、规则与回合处理。 */
#include <stdlib.h>
#include <string.h>

#include "game.h"

/* map 是可变的局内地图；currentLevel.cells 保存不可由拾取修改的初始地图。
 * 前端只显示这些状态，移动、配送和胜负由本文件统一处理。 */
char map[MAX_ROWS][MAX_COLS + 1];
int mapRows, mapCols;
Level currentLevel;
Position robot, forklifts[MAX_FORKLIFTS];
int forkCount, lives, score, carrying, delivered, totalPackages, turns;
char message[160];
int paused;
HealthPickup healthPickup;
ForkliftAI forkliftAI[MAX_FORKLIFTS];
/* 只供叉车消耗；地图生成器的随机状态保存在它自己的局部变量中。 */
static uint32_t randomState;
char levelSolution[MAX_SOLUTION + 1];

int isFloor(int row, int col)
{
    /* && 从左到右短路求值，确认行列合法后才访问 map。 */
    return row >= 0 && row < mapRows && col >= 0 && col < mapCols && map[row][col] != '#';
}

void restartGame(void)
{
    int row, col, itemIndex;
    /* 使用地图副本保存拾取结果，重开时恢复原始包裹。 */
    memcpy(map, currentLevel.cells, sizeof map);
    mapRows = currentLevel.rows;
    mapCols = currentLevel.cols;
    robot = currentLevel.start;
    paused = 0;
    lives = MAX_LIVES;
    score = carrying = delivered = turns = totalPackages = 0;
    /* 清除未启用的叉车槽位，避免从其他关卡返回时快照残留旧位置。 */
    memset(forklifts, 0, sizeof forklifts);
    memset(forkliftAI, 0, sizeof forkliftAI);
    for (itemIndex = 0; itemIndex < forkCount; itemIndex++) {
        forklifts[itemIndex] = currentLevel.forkStarts[itemIndex];
        forkliftAI[itemIndex] = (ForkliftAI){-1, 0, 0};
    }
    for (row = 0; row < mapRows; row++) {
        for (col = 0; col < mapCols; col++) {
            if (map[row][col] == 'P') {
                totalPackages++;
            }
        }
    }
    /* 地图即使经历内部 seed 重试，叉车仍从对外关卡号开始。
     * 重开不调用生成器或求解器，直接恢复验收过的地图与随机序列。 */
    randomState = currentLevel.number;
    {
        /* 独立序列，不改变叉车的随机数消耗。重开复现掉落概率和回合。 */
        uint32_t dropSeed = currentLevel.number ^ UINT32_C(0xA341316C);
        memset(&healthPickup, 0, sizeof healthPickup);
        healthPickup.status = (nextRandom(&dropSeed) >> 31) ? HEALTH_PENDING : HEALTH_NONE;
        healthPickup.spawnTurn = 5 + (int)((nextRandom(&dropSeed) >> 16) % 16);
    }
    strcpy(message, "Collect P, then return to D. Plan one move at a time.");
}

void captureGame(GameSnapshot *state)
{
    /* 先清零结构体，再复制状态，使测试中的快照比较不包含未初始化字节。
     * 当前关卡、尺寸和叉车数量在一轮搜索中固定，不逐节点复制。 */
    memset(state, 0, sizeof *state);
    memcpy(state->board, map, sizeof map);
    state->bot = robot;
    memcpy(state->traffic, forklifts, sizeof forklifts);
    memcpy(state->trafficAI, forkliftAI, sizeof forkliftAI);
    state->lifeCount = lives; state->points = score;
    state->hasPackage = carrying; state->deliveredCount = delivered;
    state->turnCount = turns; state->isPaused = paused;
    state->randomState = randomState;
    state->health = healthPickup;
}

void restoreGame(const GameSnapshot *state)
{
    /* 只恢复可变状态；调用方必须保持相同的 currentLevel 和 forkCount。
     * message 不影响规则，若需要恢复提示文本，调用方须另外保存。 */
    memcpy(map, state->board, sizeof map);
    robot = state->bot;
    memcpy(forklifts, state->traffic, sizeof forklifts);
    memcpy(forkliftAI, state->trafficAI, sizeof forkliftAI);
    lives = state->lifeCount; score = state->points;
    carrying = state->hasPackage; delivered = state->deliveredCount;
    turns = state->turnCount; paused = state->isPaused;
    randomState = state->randomState;
    healthPickup = state->health;
}

int initialise(int count, unsigned int levelNumber)
{
    Level candidate, previousLevel = currentLevel;
    GameSnapshot previous;
    char previousSolution[MAX_SOLUTION + 1];
    char selectedSolution[MAX_SOLUTION + 1];
    int attempt, i, checkedCount, accepted, previousCount = forkCount, previousTotal = totalPackages;
    int rows, cols;
    if (count < 1 || count > MAX_FORKLIFTS || !levelDimensions(levelNumber, &rows, &cols)) return 0;
    /* 换关可能失败；先保存当前关卡和局内状态，失败后继续保留旧局。
     * 无效配置在保存前直接返回，不触碰现有状态。 */
    captureGame(&previous);
    memcpy(previousSolution, levelSolution, sizeof previousSolution);
    for (attempt = 0; attempt <= LEVEL_ATTEMPTS; attempt++) {
        if (!generateLevel(&candidate, levelNumber, attempt) || !validateLevel(&candidate, NULL, 0)) continue;
        currentLevel = candidate;
        accepted = 1;
        /* 所有数量都验证，不能只验证 count：否则同一编号可能随叉车数
         * 选择不同重试候选，破坏“关卡号唯一决定地图”的约定。 */
        for (checkedCount = 1; checkedCount <= MAX_FORKLIFTS; checkedCount++) {
            forkCount = checkedCount;
            restartGame();
            if (!findWinningRoute(levelSolution)) { accepted = 0; break; }
            /* 搜索返回后已恢复初始状态；这里仍显式重开并正式回放，
             * 只有核心实际判胜才接受，不能仅信任搜索器的成功返回值。 */
            restartGame();
            for (i = 0; levelSolution[i] && getGameStatus() == GAME_PLAYING; i++)
                takeTurn(levelSolution[i]);
            if (getGameStatus() != GAME_WON) { accepted = 0; break; }
            if (checkedCount == count) memcpy(selectedSolution, levelSolution, sizeof selectedSolution);
        }
        if (!accepted) continue;
        /* 验收结束后切回玩家所选数量，不能把第四种配置的终局留给前端。 */
        forkCount = count;
        memcpy(levelSolution, selectedSolution, sizeof levelSolution);
        restartGame();
        return 1;
    }
    /* 候选耗尽时回滚关卡、配置、随机状态和旧路线，再写出失败提示。 */
    currentLevel = previousLevel;
    mapRows = previousLevel.rows; mapCols = previousLevel.cols;
    forkCount = previousCount; totalPackages = previousTotal;
    restoreGame(&previous);
    memcpy(levelSolution, previousSolution, sizeof previousSolution);
    strcpy(message, "Level generation failed: no verified winning route. Choose another level.");
    return 0;
}

int collision(void)
{
    /* 分别在机器人移动后和叉车移动后调用，覆盖双方主动进入对方格子。 */
    int itemIndex;
    for (itemIndex = 0; itemIndex < forkCount; itemIndex++) {
        if (robot.row == forklifts[itemIndex].row && robot.col == forklifts[itemIndex].col) {
            return 1;
        }
    }
    return 0;
}

int isSafeZone(int row, int col)
{
    int r, c;
    if (abs(row - currentLevel.start.row) + abs(col - currentLevel.start.col) <= SAFE_RADIUS)
        return 1;
    for (r = row - SAFE_RADIUS; r <= row + SAFE_RADIUS; r++)
        for (c = col - SAFE_RADIUS; c <= col + SAFE_RADIUS; c++)
            if (isFloor(r, c) && map[r][c] == 'D' && abs(row - r) + abs(col - c) <= SAFE_RADIUS)
                return 1;
    return 0;
}

void moveForklifts(void)
{
    const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
    int distance[MAX_ROWS][MAX_COLS], safe[MAX_ROWS][MAX_COLS];
    Position queue[MAX_ROWS * MAX_COLS];
    int r, c, i, j, d, head = 0, tail = 0;
    /* 从玩家反向 BFS，发现和追踪都按可走路径距离计算，墙后不会直接感知。 */
    for (r = 0; r < mapRows; r++)
        for (c = 0; c < mapCols; c++) {
            distance[r][c] = -1;
            safe[r][c] = isSafeZone(r, c);
        }
    if (!safe[robot.row][robot.col]) {
        distance[robot.row][robot.col] = 0;
        queue[tail++] = robot;
    }
    while (head < tail) {
        Position p = queue[head++];
        for (d = 0; d < 4; d++) {
            r = p.row + dr[d]; c = p.col + dc[d];
            if (isFloor(r, c) && !safe[r][c] && distance[r][c] < 0) {
                distance[r][c] = distance[p.row][p.col] + 1;
                queue[tail++] = (Position){r, c};
            }
        }
    }
    for (i = 0; i < forkCount; i++) {
        ForkliftAI *ai = &forkliftAI[i];
        int choices[4], count = 0, chosen = -1, best = MAX_ROWS * MAX_COLS;
        int range = distance[forklifts[i].row][forklifts[i].col];
        if (ai->chaseLeft && range < 0) {
            ai->chaseLeft = 0;
            ai->cooldown = CHASE_COOLDOWN;
        }
        if (ai->cooldown > 0) ai->cooldown--;
        else if (!ai->chaseLeft && range >= 0 && range <= DETECTION_STEPS)
            ai->chaseLeft = CHASE_TURNS;
        for (d = 0; d < 4; d++) {
            int blocked = 0;
            r = forklifts[i].row + dr[d]; c = forklifts[i].col + dc[d];
            if (!isFloor(r, c) || safe[r][c]) continue;
            for (j = 0; j < forkCount; j++)
                if (j != i && forklifts[j].row == r && forklifts[j].col == c) blocked = 1;
            if (blocked) continue;
            choices[count++] = d;
            if (ai->chaseLeft && distance[r][c] >= 0 && distance[r][c] < best) {
                best = distance[r][c]; chosen = d;
            }
        }
        if (!ai->chaseLeft) {
            /* 巡逻沿当前方向前进；受阻才转向，有其他出路时避免立即折返。 */
            for (j = 0; j < count; j++)
                if (choices[j] == ai->direction) chosen = ai->direction;
            if (chosen < 0 && count > 0) {
                int alternatives[4], n = 0;
                for (j = 0; j < count; j++)
                    if (ai->direction < 0 || choices[j] != (ai->direction ^ 1))
                        alternatives[n++] = choices[j];
                chosen = n ? alternatives[nextRandom(&randomState) % (uint32_t)n] : choices[0];
            }
        }
        if (chosen >= 0) {
            forklifts[i].row += dr[chosen]; forklifts[i].col += dc[chosen];
            ai->direction = chosen;
        }
        if (ai->chaseLeft > 0 && --ai->chaseLeft == 0) ai->cooldown = CHASE_COOLDOWN;
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

static void spawnHealth(void)
{
    Position choices[MAX_ROWS * MAX_COLS];
    int r, c, i, count = 0;
    uint32_t dropSeed = currentLevel.number ^ UINT32_C(0xC8013EA4);
    if (healthPickup.status != HEALTH_PENDING || turns < healthPickup.spawnTurn ||
        getGameStatus() != GAME_PLAYING) return;
    for (r = 0; r < mapRows; r++) {
        for (c = 0; c < mapCols; c++) {
            int occupied = robot.row == r && robot.col == c;
            if (map[r][c] != ' ') continue;
            for (i = 0; i < forkCount; i++)
                if (forklifts[i].row == r && forklifts[i].col == c) occupied = 1;
            if (!occupied) choices[count++] = (Position){r, c};
        }
    }
    /* 没有空地时延后尝试；状态只会从待生成转到一个道具，不会补刷。 */
    if (count) {
        healthPickup.position = choices[(nextRandom(&dropSeed) >> 16) % (uint32_t)count];
        healthPickup.status = HEALTH_ACTIVE;
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
        robot = currentLevel.start;
        for (itemIndex = 0; itemIndex < forkCount; itemIndex++) {
            forklifts[itemIndex] = currentLevel.forkStarts[itemIndex];
            forkliftAI[itemIndex] = (ForkliftAI){-1, 0, 0};
        }
        strcpy(message, "Collision! -1 life. Back to D; carried package is kept.");
    }
    /* 碰撞回合不拾取，避免回血抵消扣命并遮掉前端碰撞提示。满血保留道具。 */
    if (!hit && healthPickup.status == HEALTH_ACTIVE && lives < MAX_LIVES &&
        robot.row == healthPickup.position.row && robot.col == healthPickup.position.col) {
        lives++;
        healthPickup.status = HEALTH_COLLECTED;
        strcpy(message, "Health collected. +1 life.");
    }
    spawnHealth();
    return 1;
}

GameStatus getGameStatus(void)
{
    /* 最后一次配送后叉车仍行动；若同回合生命归零，优先判负。 */
    if (lives <= 0) {
        return GAME_LOST;
    }
    return delivered >= totalPackages ? GAME_WON : GAME_PLAYING;
}

int processCommand(char command)
{
    /* 重开、换关、暂停不算移动回合。胜负后的可用操作在这里统一限制，
     * Q 则由各前端退出循环或释放窗口资源，不属于核心命令。 */
    if (command == 'r') {
        restartGame();
        return 0;
    }
    if (command == 'n') {
        if (getGameStatus() != GAME_WON)
            strcpy(message, "Complete this level before advancing. R to restart or Q to quit.");
        else if (currentLevel.number == 99999)
            strcpy(message, "Last level number. R to restart or Q to quit.");
        else if (!initialise(forkCount, currentLevel.number + 1))
            strcpy(message, "Next level has no verified route. R to restart or Q to quit.");
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
