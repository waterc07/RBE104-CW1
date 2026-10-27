/* RBE104TC learning prototype; AI-assisted. */
/* 终端前端只负责整行输入、命令转换和字符显示，游戏规则由 game.c 执行。 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "game.h"

static void display(void)
{
    int row, col, itemIndex;
    char symbol;
    printf("\n=== WAREHOUSE DELIVERY ===\n");
    /* Remaining 包括正在携带但尚未配送的包裹，不是地图上 P 的数量。 */
    printf("Score: %d | Lives: %d | Remaining: %d | Turns: %d\n", score, lives,
           totalPackages - delivered, turns);
    printf("Level: %u | Map: %dx%d\n", currentLevel.number, mapRows, mapCols);
    printf("Carrying: %s | Forklifts: %d\n", carrying ? "PACKAGE" : "nothing", forkCount);
    for (row = 0; row < mapRows; row++) {
        for (col = 0; col < mapCols; col++) {
            /* 按地图、叉车、机器人顺序覆盖显示，不改写地图内容。 */
            symbol = map[row][col];
            if (healthPickup.status == HEALTH_ACTIVE && healthPickup.position.row == row &&
                healthPickup.position.col == col) symbol = '+';
            for (itemIndex = 0; itemIndex < forkCount; itemIndex++) {
                if (forklifts[itemIndex].row == row && forklifts[itemIndex].col == col) {
                    symbol = forkliftAI[itemIndex].chaseLeft > 0 ? '!' : 'F';
                }
            }
            if (robot.row == row && robot.col == col) {
                symbol = 'R';
            }
            printf("%c ", symbol);
        }
        putchar('\n');
    }
    printf("%s\n", message);
    puts("R robot | P package | D delivery | F patrol | ! chasing | + health | # wall");
}

/* 返回 1：读到完整行；0：输入结束；-1：行过长，已丢弃剩余字符。 */
static int readLine(char line[], int size)
{
    int inputChar;
    if (fgets(line, size, stdin) == NULL) {
        return 0;
    }
    if (strchr(line, '\n') != NULL || feof(stdin)) {
        return 1;
    }
    /* 缓冲区恰好装满时，多读一个字符区分完整行与超长行。 */
    inputChar = getchar();
    if (inputChar == EOF || inputChar == '\n') {
        return 1;
    }
    /* 丢弃超长行剩余字符，不能把余下的一部分当作下一次有效命令。 */
    while (inputChar != '\n' && inputChar != EOF) {
        inputChar = getchar();
    }
    return -1;
}

/* 返回合法数值，EOF 返回 -1。defaultValue 为 0 时不提供空行默认值；
 * 非零默认值须在调用方给出的范围内，本程序仅用于关卡默认值 1。 */
static int readNumber(const char prompt[], int minimum, int maximum, int defaultValue)
{
    char line[80];
    int status, itemIndex, value, digits, valid;
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        status = readLine(line, sizeof line);
        if (status == 0) {
            return -1;
        }
        /* 只把完整空行视为默认选择；EOF 仍正常退出，不自动开始游戏。 */
        if (status == 1 && defaultValue != 0 &&
            (line[0] == '\n' || (line[0] == '\r' && line[1] == '\n'))) {
            return defaultValue;
        }
        value = digits = 0;
        valid = status == 1;
        /* 最多五位十进制数字；拒绝符号和空格后再检查范围。 */
        for (itemIndex = 0; valid && line[itemIndex] != '\0'; itemIndex++) {
            if (line[itemIndex] == '\n' || line[itemIndex] == '\r') {
                break;
            }
            if (line[itemIndex] < '0' || line[itemIndex] > '9' || digits == 5) {
                valid = 0;
            } else {
                value = value * 10 + line[itemIndex] - '0';
                digits++;
            }
        }
        if (valid && digits > 0 && value >= minimum && value <= maximum) {
            return value;
        }
        printf("Enter a whole number from %d to %d.\n", minimum, maximum);
    }
}

int main(void)
{
    char line[80], command, extra;
    int seed, count, status;
    puts("WAREHOUSE DELIVERY - RBE104TC prototype");
    puts("Type one command and press Enter. Lowercase is also accepted.");
    count = readNumber("Forklifts (1-4; try 1 first): ", 1, MAX_FORKLIFTS, 0);
    if (count < 0) {
        puts("Input ended. Goodbye.");
        return 0;
    }
    /* 只有验收成功才进入游戏；候选耗尽后重新询问编号，而不是使用坏地图。 */
    for (;;) {
        seed = readNumber("Level (1-99999; Enter for 1): ", 1, 99999, (int)DEFAULT_LEVEL);
        if (seed < 0) {
            puts("Input ended. Goodbye.");
            return 0;
        }
        if (initialise(count, (unsigned int)seed)) break;
        puts("Level generation failed: no verified winning route. Choose another level.");
    }
    printf("Level: %d. Repeat the level, forklift count and commands to replay.\n", seed);
    for (;;) {
        display();
        if (getGameStatus() == GAME_LOST) {
            puts("GAME OVER - no lives left. R to restart, Q to quit.");
        } else if (getGameStatus() == GAME_WON) {
            printf("YOU WIN! All packages delivered in %d turns. Score: %d\n", turns, score);
            puts("N for next level, R to restart, Q to quit.");
        } else if (paused) {
            puts("PAUSED - P to continue, R to restart, Q to quit.");
        }
        printf("Move [W/A/S/D], pause [P], restart [R], next after win [N], quit [Q]: ");
        fflush(stdout);
        status = readLine(line, sizeof line);
        if (status == 0) {
            puts("\nInput ended. Goodbye.");
            return 0;
        }
        /* 两个 %c 尝试读取首个和第二个非空白字符；只有恰好一个时才接受。
         * 因此允许命令前后空格，但拒绝 ss 或 w q；无效输入不送入核心。 */
        if (status < 0 || sscanf(line, " %c %c", &command, &extra) != 1) {
            puts("Enter exactly one command. No turn used.");
            continue;
        }
        /* ctype 函数要求 unsigned char 值或 EOF，不能直接传负的有符号 char。 */
        command = (char)tolower((unsigned char)command);
        if (command == 'q') {
            printf("Quit. Score: %d | Delivered: %d/%d\n", score, delivered, totalPackages);
            return 0;
        }
        if (strchr("wasdprn", command) == NULL) {
            puts("Use W A S D, P, R, N or Q. No turn used.");
            continue;
        }
        {
            int previousLives = lives;
            processCommand(command);
            if (lives < previousLives) {
                puts("\n+----------------------------------------+");
                puts("| COLLISION -1 LIFE - BACK TO BASE        |");
                puts("| Carried package kept.                  |");
                puts("+----------------------------------------+");
            }
        }
    }
}
