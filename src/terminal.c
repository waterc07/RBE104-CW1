/* RBE104TC learning prototype; AI-assisted. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "game.h"

static void display(void)
{
    int row, col, itemIndex;
    char symbol;
    printf("\n=== WAREHOUSE DELIVERY ===\n");
    printf("Score: %d | Lives: %d | Remaining: %d | Turns: %d\n", score, lives,
           totalPackages - delivered, turns);
    printf("Carrying: %s | Forklifts: %d\n", carrying ? "PACKAGE" : "nothing", forkCount);
    for (row = 0; row < ROWS; row++) {
        for (col = 0; col < COLS; col++) {
            /* 按地图、叉车、机器人顺序覆盖显示，不改写地图内容。 */
            symbol = map[row][col];
            for (itemIndex = 0; itemIndex < forkCount; itemIndex++) {
                if (forklifts[itemIndex].row == row && forklifts[itemIndex].col == col) {
                    symbol = 'F';
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
    puts("R robot | P package | D delivery | F forklift | # wall");
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
    while (inputChar != '\n' && inputChar != EOF) {
        inputChar = getchar();
    }
    return -1;
}

static int readNumber(const char prompt[], int minimum, int maximum)
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
    count = readNumber("Forklifts (1-4; try 1 first): ", 1, MAX_FORKLIFTS);
    if (count < 0) {
        puts("Input ended. Goodbye.");
        return 0;
    }
    seed = readNumber("Random seed (0-99999; e.g. 42): ", 0, 99999);
    if (seed < 0) {
        puts("Input ended. Goodbye.");
        return 0;
    }
    initialise(count, (unsigned int)seed);
    printf("Seed: %d. Repeat this seed and the same commands to replay.\n", seed);
    for (;;) {
        display();
        if (getGameStatus() == GAME_LOST) {
            puts("GAME OVER - all three lives lost. R to restart, Q to quit.");
        } else if (getGameStatus() == GAME_WON) {
            printf("YOU WIN! All packages delivered in %d turns. Score: %d\n", turns, score);
        } else if (paused) {
            puts("PAUSED - P to continue, R to restart, Q to quit.");
        }
        printf("Move [W/A/S/D], pause/continue [P], restart [R], quit [Q]: ");
        fflush(stdout);
        status = readLine(line, sizeof line);
        if (status == 0) {
            puts("\nInput ended. Goodbye.");
            return 0;
        }
        /* Reject multiple commands on one line. */
        if (status < 0 || sscanf(line, " %c %c", &command, &extra) != 1) {
            puts("Enter exactly one command. No turn used.");
            continue;
        }
        command = (char)tolower((unsigned char)command);
        if (command == 'q') {
            printf("Quit. Score: %d | Delivered: %d/%d\n", score, delivered, totalPackages);
            return 0;
        }
        if (strchr("wasdpr", command) == NULL) {
            puts("Use W A S D, P, R or Q. No turn used.");
            continue;
        }
        processCommand(command);
    }
}
