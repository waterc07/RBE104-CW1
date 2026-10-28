#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "scene.h"

static void replay(void)
{
    /* 复制验收路线后逐步交给共享核心，确认返回路线确实能获胜。
     * 此检查没有窗口和键盘输入，不能替代 SDL 交互验收。 */
    int i;
    char route[MAX_SOLUTION + 1];
    strcpy(route, levelSolution);
    assert(route[0]);
    for (i = 0; route[i]; i++) {
        assert(processCommand(route[i]) == 1);
        if (delivered < totalPackages) {
            assert(getGameStatus() == GAME_PLAYING);
        }
    }
    assert(getGameStatus() == GAME_WON && delivered == totalPackages &&
           score == totalPackages * 100 && lives > 0);
}

/* 固定测试地图把规则测试与随机生成分开；编号 1042 只是夹具标识，
 * 不是正常生成的第 1042 关。改完状态后调用 restartGame 统一初始化。 */
static void fixture(void)
{
    const char *cells[9] = {"###################", "#D    #     P     #", "# ### # ### ### # #",
                            "#   #   #       # #", "# P ### # ### #   #", "#       #     # P #",
                            "# ### ##### ### # #", "#                 #", "###################"};
    int r;
    memset(&currentLevel, 0, sizeof currentLevel);
    currentLevel.number = 1042;
    currentLevel.rows = 9;
    currentLevel.cols = 19;
    currentLevel.start = (Position){1, 1};
    currentLevel.forkStarts[0] = (Position){7, 9};
    currentLevel.forkStarts[1] = (Position){3, 11};
    currentLevel.forkStarts[2] = (Position){5, 11};
    currentLevel.forkStarts[3] = (Position){7, 16};
    for (r = 0; r < 9; r++) {
        strcpy(currentLevel.cells[r], cells[r]);
    }
    forkCount = 1;
    restartGame();
}

static void testRules(void)
{
    int i;
    const char *route = "sssdawww";
    GameSnapshot expected, actual;
    fixture();
    assert(!takeTurn('w') && !takeTurn('x') && turns == 0);
    for (i = 0; route[i]; i++) {
        takeTurn(route[i]);
    }
    assert(score == 100 && delivered == 1 && !carrying && lives == 3);
    captureGame(&expected);
    processCommand('r');
    processCommand('p');
    assert(paused && !processCommand('s') && !takeTurn('d') && turns == 0);
    processCommand('p');
    for (i = 0; route[i]; i++) {
        processCommand(route[i]);
    }
    captureGame(&actual);
    assert(memcmp(&expected, &actual, sizeof expected) == 0);
    processCommand('p');
    processCommand('r');
    assert(!paused && turns == 0 && lives == 3 && !score);
    for (i = 0; i < 3; i++) {
        forklifts[0] = (Position){1, 2};
        takeTurn('d');
        assert(lives == 2 - i && robot.row == 1 && robot.col == 1);
    }
    assert(getGameStatus() == GAME_LOST && !processCommand('s'));
    processCommand('p');
    assert(!paused);
    processCommand('r');
    assert(lives == 3);
    fixture();
    carrying = 1;
    robot = (Position){4, 1};
    takeTurn('d');
    assert(carrying == 1 && map[4][2] == 'P');
    /* 配送点及相邻安全格不能进入叉车，因此最后一次配送不会在此处受撞。 */
    fixture();
    robot = (Position){1, 2};
    carrying = 1;
    delivered = 2;
    lives = 1;
    forklifts[0] = (Position){2, 1};
    map[3][1] = '#';
    takeTurn('a');
    assert(delivered == 3 && lives == 1 && getGameStatus() == GAME_WON);
    lives = 0;
    assert(getGameStatus() == GAME_LOST);
    processCommand('r');
    /* 主动进入叉车格时，先在碰撞格放下携带包裹，再回到出生点。 */
    carrying = 1;
    forklifts[0] = (Position){1, 2};
    takeTurn('d');
    assert(!carrying && lives == 2 && map[1][2] == 'P');
    fixture();
    captureGame(&expected);
    memset(map, '#', sizeof map);
    map[1][1] = 'D';
    captureGame(&actual);
    {
        GameSnapshot restored;
        char route[MAX_SOLUTION + 1];
        assert(!findWinningRoute(route) && !route[0]);
        captureGame(&restored);
        assert(memcmp(&actual, &restored, sizeof actual) == 0);
    }
    restoreGame(&expected);
    puts("PASS: delivery, walls, input, pause, restart, collisions, carrying and end conditions.");
}

static void testPackageDrop(void)
{
    GameSnapshot dropped, restored;
    int i;
    /* 手动夹具：拾取后走入叉车，落点应是碰撞格而非起点或原包裹格。 */
    fixture();
    forkCount = 0;
    robot = (Position){4, 1};
    assert(processCommand('d') && carrying && map[4][2] == ' ');
    forkCount = 1;
    forklifts[0] = (Position){5, 2};
    assert(processCommand('s'));
    assert(!carrying && lives == 2 && map[5][2] == 'P' && map[4][2] == ' ');
    assert(robot.row == currentLevel.start.row && robot.col == currentLevel.start.col);
    assert(score == 0 && delivered == 0 && totalPackages == 3 && turns == 2);
    forkCount = 0;
    robot = (Position){5, 1};
    assert(processCommand('d') && carrying && map[5][2] == ' ');
    processCommand('r');
    assert(!carrying && lives == MAX_LIVES && map[4][2] == 'P' && map[5][2] == ' ');

    /* 叉车主动撞到刚拾取包裹的机器人。 */
    fixture();
    robot = (Position){4, 1};
    forklifts[0] = (Position){5, 2};
    processCommand('d');
    assert(!carrying && lives == 2 && map[4][2] == 'P');

    /* 落点已有包裹：快照恢复后仍可分两次拾取和配送。 */
    fixture();
    forkCount = 0;
    robot = (Position){1, 11};
    processCommand('d');
    assert(carrying);
    robot = (Position){4, 1};
    forkCount = 1;
    forklifts[0] = (Position){4, 2};
    processCommand('d');
    assert(!carrying && lives == 2 && map[4][2] == 'P');
    captureGame(&dropped);
    processCommand('r');
    restoreGame(&dropped);
    captureGame(&restored);
    assert(memcmp(&dropped, &restored, sizeof dropped) == 0);
    forkCount = 0;
    for (i = 0; i < 2; i++) {
        robot = (Position){4, 1};
        processCommand('d');
        assert(carrying && map[4][2] == (i == 0 ? 'P' : ' '));
        robot = (Position){1, 2};
        processCommand('a');
        assert(!carrying && delivered == i + 1 && score == (i + 1) * 100);
    }
    /* 致命碰撞仍掉落；空手碰撞不生成包裹。 */
    fixture();
    robot = (Position){1, 11};
    forkCount = 0;
    processCommand('d');
    robot = (Position){5, 1};
    forkCount = 1;
    forklifts[0] = (Position){5, 2};
    lives = 1;
    processCommand('d');
    assert(!carrying && map[5][2] == 'P' && getGameStatus() == GAME_LOST);
    fixture();
    robot = (Position){5, 1};
    forklifts[0] = (Position){5, 2};
    processCommand('d');
    assert(!carrying && lives == 2 && map[5][2] == ' ');
    puts(
        "PASS: collision package drops, pickup, stacking, snapshots, restart and fatal collision.");
}

static void testHealth(void)
{
    int i, r, c, present = 0, absent = 0, earlyPresent = 0;
    GameSnapshot before, after;
    fixture();
    for (i = 1; i <= 1000; i++) {
        currentLevel.number = (unsigned int)i;
        restartGame();
        assert(healthPickup.spawnTurn >= 5 && healthPickup.spawnTurn <= 20);
        present += healthPickup.status == HEALTH_PENDING;
        if (i <= 20) {
            earlyPresent += healthPickup.status == HEALTH_PENDING;
        }
        absent += healthPickup.status == HEALTH_NONE;
        captureGame(&before);
        restartGame();
        captureGame(&after);
        assert(memcmp(&before, &after, sizeof before) == 0);
    }
    assert(present >= 400 && present <= 600 && absent >= 400 && absent <= 600);
    assert(earlyPresent >= 5 && earlyPresent <= 15);
    /* 正常生成的第 4 关，在安全区来回移动，不手动注入道具或回合数。 */
    assert(initialise(1, 4));
    assert(healthPickup.status == HEALTH_PENDING && healthPickup.spawnTurn == 11);
    for (i = 0; i < 11; i++) {
        assert(processCommand(i % 2 ? 'a' : 'd'));
        assert(healthPickup.status == (i < 10 ? HEALTH_PENDING : HEALTH_ACTIVE));
    }
    assert(strstr(message, "Health dropped!") && lives == MAX_LIVES);
    drawScene(currentLevel.number, 0);
    assert(savePreview("health_natural.ppm"));
    captureGame(&before);
    processCommand('r');
    for (i = 0; i < 11; i++) {
        assert(processCommand(i % 2 ? 'a' : 'd'));
    }
    captureGame(&after);
    assert(memcmp(&before, &after, sizeof before) == 0);
    fixture();
    forkCount = 0;
    healthPickup = (HealthPickup){{0, 0}, 5, HEALTH_PENDING};
    turns = 4;
    assert(!processCommand('w') && healthPickup.status == HEALTH_PENDING);
    processCommand('p');
    assert(!processCommand('d') && healthPickup.status == HEALTH_PENDING);
    processCommand('p');
    assert(processCommand('d') && healthPickup.status == HEALTH_ACTIVE);
    r = healthPickup.position.row;
    c = healthPickup.position.col;
    assert(map[r][c] == ' ' && !(robot.row == r && robot.col == c));
    drawScene(currentLevel.number, 0);
    assert(savePreview("health.ppm"));
    captureGame(&before);
    healthPickup = (HealthPickup){{1, 3}, 5, HEALTH_ACTIVE};
    processCommand('d');
    assert(lives == MAX_LIVES && healthPickup.status == HEALTH_ACTIVE);
    processCommand('a');
    lives = 2;
    processCommand('d');
    assert(lives == MAX_LIVES && healthPickup.status == HEALTH_COLLECTED);
    for (i = 0; i < 100; i++) {
        processCommand(i % 2 ? 'd' : 'a');
    }
    assert(healthPickup.status == HEALTH_COLLECTED);
    restoreGame(&before);
    captureGame(&after);
    assert(memcmp(&before, &after, sizeof before) == 0);
    fixture();
    lives = 1;
    healthPickup = (HealthPickup){{1, 2}, 5, HEALTH_ACTIVE};
    forklifts[0] = (Position){1, 2};
    processCommand('d');
    assert(lives == 0 && getGameStatus() == GAME_LOST && healthPickup.status == HEALTH_ACTIVE);
    fixture();
    forkCount = 0;
    healthPickup = (HealthPickup){{0, 0}, 5, HEALTH_NONE};
    for (i = 0; i < 100; i++) {
        processCommand(i % 2 ? 'a' : 'd');
    }
    assert(healthPickup.status == HEALTH_NONE);
    /* 唯一空地被玩家占用时延后掉落；玩家离开后再生成。 */
    fixture();
    forkCount = 0;
    memset(map, '#', sizeof map);
    map[1][1] = 'D';
    map[1][2] = ' ';
    healthPickup = (HealthPickup){{0, 0}, 5, HEALTH_PENDING};
    turns = 4;
    processCommand('d');
    assert(healthPickup.status == HEALTH_PENDING);
    processCommand('a');
    assert(healthPickup.status == HEALTH_ACTIVE && healthPickup.position.row == 1 &&
           healthPickup.position.col == 2);
    puts(
        "PASS: optional single health drop, timing, full health, pickup, collisions and snapshots.");
}

static void testTraffic(void)
{
    int r, c, i;
    GameSnapshot saved, restored;
    fixture();
    for (r = 1; r < mapRows - 1; r++) {
        for (c = 1; c < mapCols - 1; c++) {
            map[r][c] = ' ';
        }
    }
    map[1][1] = 'D';
    robot = (Position){5, 12};
    forklifts[0] = (Position){5, 8};
    moveForklifts();
    assert(forklifts[0].col == 9 && forkliftAI[0].chaseLeft == CHASE_TURNS - 1);
    robot = (Position){5, 16};
    for (i = 1; i < CHASE_TURNS; i++) {
        moveForklifts();
    }
    assert(forkliftAI[0].chaseLeft == 0 && forkliftAI[0].cooldown == CHASE_COOLDOWN);
    for (i = 0; i < CHASE_COOLDOWN; i++) {
        robot = (Position){4, forklifts[0].col};
        moveForklifts();
        assert(forkliftAI[0].chaseLeft == 0);
    }
    robot = (Position){4, forklifts[0].col};
    moveForklifts();
    assert(forkliftAI[0].chaseLeft > 0);
    captureGame(&saved);
    moveForklifts();
    restoreGame(&saved);
    captureGame(&restored);
    assert(memcmp(&saved, &restored, sizeof saved) == 0);
    robot = currentLevel.start;
    moveForklifts();
    assert(forkliftAI[0].chaseLeft == 0);
    for (i = 0; i < 1000; i++) {
        moveForklifts();
        assert(!isSafeZone(forklifts[0].row, forklifts[0].col));
    }
    /* 墙使实际道路绕远，即使坐标看起来近，也不应在四步范围内发现玩家。 */
    forklifts[0] = (Position){5, 8};
    forkliftAI[0] = (ForkliftAI){-1, 0, 0};
    robot = (Position){5, 12};
    map[5][10] = '#';
    moveForklifts();
    assert(!forkliftAI[0].chaseLeft);
    /* 直路上继续沿原方向巡逻，不能每回合重新随机转向。 */
    robot = currentLevel.start;
    forklifts[0] = (Position){7, 8};
    forkliftAI[0] = (ForkliftAI){3, 0, 0};
    moveForklifts();
    assert(forklifts[0].col == 9);
    moveForklifts();
    assert(forklifts[0].col == 10);
    /* 所有方向都被堵住时等待，不穿墙也不强行选一个方向。 */
    map[6][10] = map[7][9] = map[7][11] = '#';
    moveForklifts();
    assert(forklifts[0].row == 7 && forklifts[0].col == 10);
    forkliftAI[0].chaseLeft = 3;
    drawScene(currentLevel.number, 0);
    assert(savePreview("chasing.ppm"));
    drawCollisionNotice();
    assert(savePreview("collision.ppm"));
    restartGame();
    assert(forkliftAI[0].direction == -1 && !forkliftAI[0].chaseLeft && !forkliftAI[0].cooldown);
    puts(
        "PASS: safe zones, path detection, chase limit, cooldown, patrol, waiting and AI snapshots.");
}

static void testValidation(void)
{
    Level valid, bad, retry;
    char error[160];
    int r, c;
    assert(generateLevel(&valid, 42, 0) && validateLevel(&valid, error, sizeof error));
    bad = valid;
    bad.cells[0][1] = ' ';
    assert(!validateLevel(&bad, error, sizeof error));
    bad = valid;
    bad.cells[1][1] = 'X';
    assert(!validateLevel(&bad, error, sizeof error));
    bad = valid;
    bad.rows++;
    assert(!validateLevel(&bad, error, sizeof error));
    bad = valid;
    bad.forkStarts[1] = bad.forkStarts[0];
    assert(!validateLevel(&bad, error, sizeof error));
    bad = valid;
    bad.forkStarts[0] = bad.start;
    assert(!validateLevel(&bad, error, sizeof error));
    bad = valid;
    bad.cells[1][2] = 'P';
    assert(!validateLevel(&bad, error, sizeof error));
    bad = valid;
    for (r = 0; r < valid.rows; r++) {
        for (c = 0; c < valid.cols; c++) {
            if (valid.cells[r][c] == 'P') {
                bad.cells[r - 1][c] = '#';
                bad.cells[r + 1][c] = '#';
                bad.cells[r][c - 1] = '#';
                bad.cells[r][c + 1] = '#';
                r = valid.rows;
                break;
            }
        }
    }
    assert(!validateLevel(&bad, error, sizeof error));
    assert(generateLevel(&retry, 999, 1) && retry.rows == 9 && retry.cols == 15);
    assert(retry.generationSeed >= 100000);
    assert(generateLevel(&bad, 999, 100));
    assert(generateLevel(&valid, 1000, 1) && valid.generationSeed == bad.generationSeed + 1);
    assert(!generateLevel(&bad, 0, 0) && !generateLevel(&bad, 100000, 0));
    assert(!generateLevel(&bad, 42, 101));
    puts("PASS: structural rejection and disjoint retry seeds with fixed dimensions.");
}

static void testLevels(void)
{
    const unsigned int numbers[] = {1, 999, 1000, 9999, 10000, 29999, 30000, 59999, 60000, 99999};
    const int rows[] = {9, 9, 9, 9, 11, 11, 13, 13, 17, 17};
    const int cols[] = {15, 15, 19, 19, 23, 23, 27, 27, 31, 31};
    const int packages[] = {3, 3, 4, 4, 4, 4, 5, 5, 5, 5};
    size_t i;
    Level original;
    GameSnapshot before, after;
    char solution[MAX_SOLUTION + 1];
    for (i = 0; i < sizeof numbers / sizeof numbers[0]; i++) {
        assert(initialise(1, numbers[i]));
        assert(mapRows == rows[i] && mapCols == cols[i]);
        assert(totalPackages == packages[i] && levelPackageCount(numbers[i]) == packages[i]);
        assert(validateLevel(&currentLevel, NULL, 0));
        original = currentLevel;
        {
            Level missing = currentLevel;
            int r, c, removed = 0;
            for (r = 0; r < mapRows && !removed; r++) {
                for (c = 0; c < mapCols; c++) {
                    if (missing.cells[r][c] == 'P') {
                        missing.cells[r][c] = ' ';
                        removed = 1;
                        break;
                    }
                }
            }
            assert(removed && !validateLevel(&missing, NULL, 0));
        }
        captureGame(&before);
        strcpy(solution, levelSolution);
        replay();
        processCommand('r');
        captureGame(&after);
        assert(memcmp(&before, &after, sizeof before) == 0);
        assert(findWinningRoute(levelSolution));
        captureGame(&after);
        assert(memcmp(&before, &after, sizeof before) == 0);
        assert(strcmp(solution, levelSolution) == 0);
        assert(initialise(4, numbers[i]));
        assert(memcmp(&original, &currentLevel, sizeof original) == 0);
        assert(totalPackages == packages[i]);
        replay();
        printf("PASS: level %u, %dx%d, attempt %d, route replay at 1 and 4 forklifts.\n",
               numbers[i], mapRows, mapCols, currentLevel.attempt);
    }
    assert(initialise(2, 42));
    original = currentLevel;
    assert(initialise(3, 42) && memcmp(&original, &currentLevel, sizeof original) == 0);
    captureGame(&before);
    assert(initialise(4, 1000));
    assert(initialise(3, 42));
    captureGame(&after);
    assert(memcmp(&before, &after, sizeof before) == 0);
    replay();
    processCommand('n');
    assert(currentLevel.number == 43 && turns == 0);
    captureGame(&before);
    original = currentLevel;
    assert(!initialise(0, 42) && !initialise(5, 42) && !initialise(1, 0) && !initialise(1, 100000));
    captureGame(&after);
    assert(memcmp(&before, &after, sizeof before) == 0 &&
           memcmp(&original, &currentLevel, sizeof original) == 0);
    assert(initialise(4, 99999));
    replay();
    processCommand('n');
    assert(currentLevel.number == 99999 && getGameStatus() == GAME_WON);
    drawScene(currentLevel.number, paused);
    assert(savePreview("large.ppm"));
    puts(
        "PASS: remaining traffic counts, next level, final level and invalid configuration preservation.");
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--audit") == 0) {
        int i, count = atoi(argv[2]);
        if (count < 1 || count > 99999) {
            return 1;
        }
        for (i = 0; i < count; i++) {
            unsigned int number = ((unsigned int)i * 7919U) % 99999U + 1;
            uint32_t fingerprint = UINT32_C(2166136261);
            int r, c, j;
            if (!initialise(i % 4 + 1, number)) {
                return 1;
            }
            for (r = 0; r < mapRows; r++) {
                for (c = 0; c < mapCols; c++) {
                    fingerprint = (fingerprint ^ (unsigned char)map[r][c]) * UINT32_C(16777619);
                }
            }
            for (j = 0; j < MAX_FORKLIFTS; j++) {
                fingerprint =
                    (fingerprint ^ (uint32_t)currentLevel.forkStarts[j].row) * UINT32_C(16777619);
                fingerprint =
                    (fingerprint ^ (uint32_t)currentLevel.forkStarts[j].col) * UINT32_C(16777619);
            }
            for (j = 0; levelSolution[j]; j++) {
                fingerprint = (fingerprint ^ (unsigned char)levelSolution[j]) * UINT32_C(16777619);
            }
            replay();
            printf("%u %d %d %u %lu %d %d\n", number, mapRows, mapCols, currentLevel.generationSeed,
                   (unsigned long)fingerprint, turns, lives);
        }
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "--route") == 0) {
        if (!initialise(atoi(argv[3]), (unsigned int)strtoul(argv[2], NULL, 10))) {
            return 1;
        }
        puts(levelSolution);
        return 0;
    }
    testRules();
    testPackageDrop();
    testHealth();
    testTraffic();
    testValidation();
    testLevels();
    assert(initialise(2, 42));
    replay();
    drawScene(42, paused);
    assert(savePreview("delivered.ppm"));
    puts("PASS: rendering and preview output; SDL event interaction requires a window check.");
    return 0;
}
