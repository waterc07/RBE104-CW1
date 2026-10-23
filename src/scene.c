/* Small software pixel renderer. The SDL window and preview use the same pixels. */
/* 绘制只读取游戏状态；坐标均为固定逻辑画布上的像素位置。 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "game.h"
#include "scene.h"

uint32_t pixels[WIDTH * HEIGHT];
static const uint32_t backgroundColor = 0x13101E, panelColor = 0x1E192C, lineColor = 0x3C3151;
static const uint32_t whiteColor = 0xF2EBFF, mutedColor = 0xA99ABF, purpleColor = 0xBD9BFF;

static void rect(int x, int y, int rectWidth, int rectHeight, uint32_t color)
{
    int itemIndex, pixelRow;
    /* 逐像素裁剪到画布内，越界部分不写入缓冲区。 */
    for (pixelRow = y; pixelRow < y + rectHeight; pixelRow++) {
        for (itemIndex = x; itemIndex < x + rectWidth; itemIndex++) {
            if (itemIndex >= 0 && itemIndex < WIDTH && pixelRow >= 0 && pixelRow < HEIGHT) {
                pixels[pixelRow * WIDTH + itemIndex] = color;
            }
        }
    }
}

/* 5×7 字体：依次存 A-Z、0-9；每行低五位从高到低对应左到右。 */
static const unsigned char font[][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {31, 4, 4, 4, 4, 4, 31},
    {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},     {14, 17, 19, 21, 25, 17, 14},
    {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},   {14, 16, 16, 30, 17, 17, 14},
    {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}};

static void text(int x, int y, const char *content, int scale, uint32_t color)
{
    int itemIndex, row, col, glyphIndex;
    for (itemIndex = 0; content[itemIndex]; itemIndex++) {
        unsigned char glyphChar = (unsigned char)toupper((unsigned char)content[itemIndex]);
        /* 小写转大写；表外字符只绘制下方列出的标点，其他字符留空位。 */
        glyphIndex = glyphChar >= 'A' && glyphChar <= 'Z' ? glyphChar - 'A' : glyphChar >= '0' && glyphChar <= '9' ? 26 + glyphChar - '0' : -1;
        if (glyphIndex >= 0) {
            for (row = 0; row < 7; row++) {
                for (col = 0; col < 5; col++) {
                    if (font[glyphIndex][row] & (1 << (4 - col))) {
                        rect(x + itemIndex * 6 * scale + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
        } else if (glyphChar == '-') {
            rect(x + itemIndex * 6 * scale, y + 3 * scale, 5 * scale, scale, color);
        } else if (glyphChar == '/') {
            for (row = 0; row < 7; row++) {
                rect(x + itemIndex * 6 * scale + (4 - row * 4 / 6) * scale, y + row * scale, scale, scale,
                     color);
            }
        } else if (glyphChar == ':') {
            rect(x + itemIndex * 6 * scale + scale, y + 2 * scale, scale, scale, color);
            rect(x + itemIndex * 6 * scale + scale, y + 5 * scale, scale, scale, color);
        } else if (glyphChar == '.') {
            rect(x + itemIndex * 6 * scale + scale, y + 6 * scale, scale, scale, color);
        } else if (glyphChar == '+') {
            rect(x + itemIndex * 6 * scale, y + 3 * scale, 5 * scale, scale, color);
            rect(x + itemIndex * 6 * scale + 2 * scale, y + scale, scale, 5 * scale, color);
        }
    }
}

/* 12×12 图标：'1'..'5' 索引调色板，'.' 跳过绘制以保留背景。 */
static const char *bot[12] = {
    ".....22.....",
    ".....12.....",
    "...111111...",
    "..12222221..",
    ".1123333211.",
    ".1123535211.",
    ".1123333211.",
    "..12222221..",
    "...144441...",
    "..11444411..",
    "..11....11..",
    "..33....33.."
};
static const char *box[12] = {
    "............",
    "...111111...",
    "..12233221..",
    ".1222332221.",
    ".1444334441.",
    ".1444334441.",
    ".1444334441.",
    ".1444334441.",
    ".1444444441.",
    ".1444444441.",
    "..11111111..",
    "............"
};
static const char *fork[12] = {
    "..11....11..",
    "..11....11..",
    "..11111111..",
    "...222222...",
    "..12333321..",
    "..12355321..",
    ".1123333211.",
    ".1122222211.",
    "..14444441..",
    ".3314444133.",
    ".33......33.",
    "............"
};
static const char *heart[12] = {
    "............",
    "..111..111..",
    ".1222112221.",
    ".1252222221.",
    ".1222222221.",
    "..12222221..",
    "...122221...",
    "....1221....",
    ".....11.....",
    "............",
    "............",
    "............"
};
static const char *flag[12] = {
    "..11111111..",
    "..12323231..",
    "..13232321..",
    "..12323231..",
    "..13232321..",
    "..11111111..",
    "..11........",
    "..11........",
    "..11........",
    "..11........",
    ".111111.....",
    "............"
};

static void sprite(int x, int y, const char **shape, int scale, int kind)
{
    /* kind 顺序：机器人、包裹、叉车、爱心、旗帜；scale 是整数放大倍数。 */
    static const uint32_t palettes[5][6] = {{0, 0x7254A5, 0xD9C3FF, 0x292039, 0xAC84E9, 0xC3FFF0},
                                            {0, 0x946442, 0xF5D5A1, 0xFFF0D2, 0xD79B64, 0xFFF0D2},
                                            {0, 0xA45577, 0xF4A4BD, 0x44263E, 0xBD6B95, 0xFFE6B3},
                                            {0, 0x8E609F, 0xD9ABF1, 0, 0, 0xFFF1FF},
                                            {0, 0x6E9E93, 0xB6E0CD, 0x324D48, 0, 0}};
    int row, col, paletteIndex;
    for (row = 0; row < 12; row++) {
        for (col = 0; col < 12; col++) {
            paletteIndex = shape[row][col] - '0';
            if (paletteIndex >= 1 && paletteIndex <= 5) {
                rect(x + col * scale, y + row * scale, scale, scale, palettes[kind][paletteIndex]);
            }
        }
    }
}

static void panel(int x, int y, int rectWidth, int rectHeight)
{
    rect(x, y, rectWidth, rectHeight, lineColor);
    rect(x + 1, y + 1, rectWidth - 2, rectHeight - 2, panelColor);
}

void drawScene(unsigned int seed, int paused)
{
    int row, col, itemIndex, x, y;
    char textBuffer[90];
    /* 标题和顶部状态卡片。每帧先清空缓冲区，避免残留上一帧图标。 */
    rect(0, 0, WIDTH, HEIGHT, backgroundColor);
    rect(40, 36, 8, 36, purpleColor);
    text(62, 34, "PARCEL SHIFT", 5, whiteColor);
    text(64, 83, "A LITTLE ROBOT. A LONG WAY HOME.", 1, mutedColor);
    text(974, 42, "NIGHT SHIFT / 01", 2, purpleColor);
    text(974, 69, "WAREHOUSE DELIVERY", 1, mutedColor);

    panel(40, 120, 260, 82);
    text(58, 136, "SCORE", 1, mutedColor);
    snprintf(textBuffer, sizeof textBuffer, "%04d", score);
    text(58, 160, textBuffer, 4, whiteColor);
    text(184, 177, "+100 / DROP", 1, mutedColor);
    panel(312, 120, 280, 82);
    text(330, 136, "LIVES", 1, mutedColor);
    for (itemIndex = 0; itemIndex < 3; itemIndex++) {
        if (itemIndex < lives) {
            sprite(330 + itemIndex * 42, 152, heart, 3, 3);
        } else {
            rect(334 + itemIndex * 42, 160, 24, 18, lineColor);
        }
    }
    snprintf(textBuffer, sizeof textBuffer, "%d / 3", lives);
    text(496, 169, textBuffer, 2, mutedColor);
    panel(604, 120, 272, 82);
    text(622, 136, "DELIVERED", 1, mutedColor);
    snprintf(textBuffer, sizeof textBuffer, "%02d / %02d", delivered, totalPackages);
    text(622, 164, textBuffer, 3, whiteColor);

    /* 地图左上角 (40, 242)，每格 44×44；先画格子，再叠加移动对象。 */
    text(40, 219, "SECTOR A / WAREHOUSE FLOOR", 1, mutedColor);
    text(716, 219, "ONE MOVE. ONE TURN.", 1, mutedColor);
    for (row = 0; row < ROWS; row++) {
        for (col = 0; col < COLS; col++) {
            x = 40 + col * 44;
            y = 242 + row * 44;
            rect(x, y, 44, 44, 0x292235);
            rect(x + 1, y + 1, 42, 42, (row + col) % 2 ? 0x231D31 : 0x251F33);
            if (map[row][col] == '#') {
                rect(x + 2, y + 4, 40, 38, 0x181421);
                rect(x + 2, y + 2, 40, 33, 0x443653);
                rect(x + 2, y + 2, 40, 3, 0x66517F);
                rect(x + 6, y + 10, 32, 2, 0x554267);
                rect(x + 6, y + 25, 32, 2, 0x342A44);
                rect(x + 6, y + 6, 2, 24, 0x514062);
            } else if (map[row][col] == 'D') {
                rect(x + 3, y + 3, 38, 38, 0x31463F);
                sprite(x + 4, y + 4, flag, 3, 4);
            } else if (map[row][col] == 'P') {
                sprite(x + 4, y + 4, box, 3, 1);
            }
        }
    }
    for (itemIndex = 0; itemIndex < forkCount; itemIndex++) {
        sprite(44 + forklifts[itemIndex].col * 44, 246 + forklifts[itemIndex].row * 44, fork, 3, 2);
    }
    sprite(44 + robot.col * 44, 246 + robot.row * 44, bot, 3, 0);
    if (carrying) {
        sprite(67 + robot.col * 44, 266 + robot.row * 44, box, 1, 1);
    }

    /* 右侧面板：任务进度、携带状态、回合、种子和图例。 */
    panel(906, 120, 334, 518);
    text(928, 141, "YOUR DELIVERY RUN", 2, whiteColor);
    rect(928, 173, 290, 1, lineColor);
    sprite(928, 190, box, 3, 1);
    text(980, 194, "PACKAGES LEFT", 1, mutedColor);
    snprintf(textBuffer, sizeof textBuffer, "%02d", totalPackages - delivered);
    text(980, 215, textBuffer, 4, whiteColor);
    text(928, 264, "CARRYING", 1, mutedColor);
    text(928, 285, carrying ? "ONE PACKAGE" : "EMPTY", 3, carrying ? 0xF5D5A1 : purpleColor);
    text(928, 323, carrying ? "RETURN TO THE GREEN FLAG" : "FIND A GOLDEN PACKAGE", 1, mutedColor);
    rect(928, 350, 290, 1, lineColor);
    snprintf(textBuffer, sizeof textBuffer, "TURNS   %03d", turns);
    text(928, 374, textBuffer, 2, whiteColor);
    snprintf(textBuffer, sizeof textBuffer, "SEED    %05u", seed);
    text(928, 409, textBuffer, 2, mutedColor);
    snprintf(textBuffer, sizeof textBuffer, "TRAFFIC %d", forkCount);
    text(928, 444, textBuffer, 2, mutedColor);
    rect(928, 481, 290, 1, lineColor);
    sprite(925, 500, bot, 2, 0);
    text(962, 507, "YOU", 1, mutedColor);
    sprite(1070, 500, fork, 2, 2);
    text(1106, 507, "AVOID", 1, mutedColor);
    sprite(925, 541, box, 2, 1);
    text(962, 548, "COLLECT", 1, mutedColor);
    sprite(1070, 541, flag, 2, 4);
    text(1106, 548, "DELIVER", 1, mutedColor);
    text(928, 605, "SMALL STEPS. SAFE DELIVERIES.", 1, purpleColor);

    /* 底部操作说明和最近一次行动的消息。 */
    rect(40, 659, 1200, 1, lineColor);
    text(40, 677, "CONTROLS", 1, mutedColor);
    for (itemIndex = 0; itemIndex < 4; itemIndex++) {
        const char *keys[] = {"W", "A", "S", "D"};
        const char *labels[] = {"UP", "LEFT", "DOWN", "RIGHT"};
        x = 40 + itemIndex * 127;
        panel(x, 700, 34, 34);
        text(x + 11, 709, keys[itemIndex], 2, purpleColor);
        text(x + 45, 713, labels[itemIndex], 1, mutedColor);
    }
    text(575, 709, "SPACE PAUSE   R RESTART   Q QUIT", 1, mutedColor);
    text(40, 766, message, 1, purpleColor);
    text(1090, 766, "RBE104TC / C", 1, mutedColor);
    /* 最后覆盖状态提示；生命耗尽优先于全部送达。 */
    if (paused || getGameStatus() != GAME_PLAYING) {
        panel(216, 362, 484, 118);
        text(246, 390,
             paused       ? "SHIFT PAUSED"
             : getGameStatus() == GAME_LOST ? "SHIFT ENDED"
                          : "ALL DELIVERED",
             3, whiteColor);
        text(246, 442, paused ? "SPACE TO CONTINUE" : "PRESS R TO START AGAIN", 1, purpleColor);
    }
}

int savePreview(const char *fileName)
{
    FILE *outputFile = fopen(fileName, "wb");
    int itemIndex;
    if (!outputFile) {
        return 0;
    }
    /* P6 在头部后直接写 R/G/B 字节，不包含 alpha，也不受主机字节序影响。 */
    fprintf(outputFile, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (itemIndex = 0; itemIndex < WIDTH * HEIGHT; itemIndex++) {
        fputc((int)(pixels[itemIndex] >> 16) & 255, outputFile);
        fputc((int)(pixels[itemIndex] >> 8) & 255, outputFile);
        fputc((int)pixels[itemIndex] & 255, outputFile);
    }
    return fclose(outputFile) == 0;
}
