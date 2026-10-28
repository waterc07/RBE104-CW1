/* Small software pixel renderer. The SDL window and preview use the same pixels. */
/* 绘制只读取游戏状态；地图使用固定格子，面板随画布边界移动。 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "game.h"
#include "scene.h"

uint32_t pixels[MAX_SCENE_WIDTH * MAX_SCENE_HEIGHT];
int sceneWidth = 1280, sceneHeight = 800;
static int offsetX, offsetY;

void updateSceneSize(void)
{
    int extraWidth = mapCols * TILE_SIZE - 836;
    int extraHeight = mapRows * TILE_SIZE - 396;
    sceneWidth = 1280 + (extraWidth > 0 ? extraWidth : 0);
    sceneHeight = 800 + (extraHeight > 0 ? extraHeight : 0);
}
static const uint32_t backgroundColor = 0x13101E, panelColor = 0x1E192C, lineColor = 0x3C3151;
static const uint32_t whiteColor = 0xF2EBFF, mutedColor = 0xA99ABF, purpleColor = 0xBD9BFF;

static void rect(int x, int y, int rectWidth, int rectHeight, uint32_t color)
{
    int itemIndex, pixelRow;
    x += offsetX; y += offsetY;
    /* 逐像素裁剪到画布内，越界部分不写入缓冲区。 */
    for (pixelRow = y; pixelRow < y + rectHeight; pixelRow++) {
        for (itemIndex = x; itemIndex < x + rectWidth; itemIndex++) {
            if (itemIndex >= 0 && itemIndex < sceneWidth && pixelRow >= 0 && pixelRow < sceneHeight) {
                pixels[pixelRow * sceneWidth + itemIndex] = color;
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
    /* 每个字形占 5 列，额外留 1 列间距，因此字符起点间隔为 6*scale。
     * 字体是 ASCII 点阵，不能直接显示 UTF-8 中文；中文说明放在源码与文档中。 */
    int itemIndex, row, col, glyphIndex;
    for (itemIndex = 0; content[itemIndex]; itemIndex++) {
        unsigned char glyphChar = (unsigned char)toupper((unsigned char)content[itemIndex]);
        /* 小写转大写；表外字符只绘制下方列出的标点，其他字符留空位。 */
        glyphIndex = glyphChar >= 'A' && glyphChar <= 'Z' ? glyphChar - 'A' : glyphChar >= '0' && glyphChar <= '9' ? 26 + glyphChar - '0' : -1;
        if (glyphIndex >= 0) {
            for (row = 0; row < 7; row++) {
                for (col = 0; col < 5; col++) {
                    /* 每行的低五位从左到右对应 bit4..bit0，置位才绘制色块。 */
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
    int tile = TILE_SIZE, originX, originY, iconScale, iconOffset;
    int extraWidth, extraHeight;
    updateSceneSize();
    extraWidth = sceneWidth - 1280; extraHeight = sceneHeight - 800;
    offsetX = offsetY = 0;
    originX = 40 + (836 + extraWidth - tile * mapCols) / 2;
    originY = 242 + (396 + extraHeight - tile * mapRows) / 2;
    /* 内嵌图标为 12×12，使用整数倍率避免像素边缘模糊；offset 负责居中。 */
    iconScale = (tile - 4) / 12;
    if (iconScale < 1) iconScale = 1;
    iconOffset = (tile - 12 * iconScale) / 2;
    char textBuffer[90];
    /* 标题和顶部状态卡片。每帧先清空缓冲区，避免残留上一帧图标。 */
    rect(0, 0, sceneWidth, sceneHeight, backgroundColor);
    rect(40, 36, 8, 36, purpleColor);
    text(62, 34, "PARCEL SHIFT", 5, whiteColor);
    text(64, 83, "A LITTLE ROBOT. A LONG WAY HOME.", 1, mutedColor);
    snprintf(textBuffer, sizeof textBuffer, "LEVEL %05u", seed);
    text(974 + extraWidth, 42, textBuffer, 2, purpleColor);
    text(974 + extraWidth, 69, "WAREHOUSE DELIVERY", 1, mutedColor);

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

    snprintf(textBuffer, sizeof textBuffer, "WAREHOUSE %d X %d", mapRows, mapCols);
    text(40, 219, textBuffer, 1, mutedColor);
    text(716 + extraWidth, 219, "ONE MOVE. ONE TURN.", 1, mutedColor);
    for (row = 0; row < mapRows; row++) {
        for (col = 0; col < mapCols; col++) {
            x = originX + col * tile;
            y = originY + row * tile;
            rect(x, y, tile, tile, 0x292235);
            rect(x + 1, y + 1, tile - 2, tile - 2, (row + col) % 2 ? 0x231D31 : 0x251F33);
            if (map[row][col] != '#' && isSafeZone(row, col))
                rect(x + 1, y + 1, tile - 2, tile - 2, 0x284238);
            if (map[row][col] == '#') {
                rect(x + 2, y + 2, tile - 4, tile - 4, 0x443653);
                rect(x + 2, y + 2, tile - 4, 2, 0x66517F);
            } else if (map[row][col] == 'D') {
                rect(x + 2, y + 2, tile - 4, tile - 4, 0x31463F);
                sprite(x + iconOffset, y + iconOffset, flag, iconScale, 4);
            } else if (map[row][col] == 'P') {
                sprite(x + iconOffset, y + iconOffset, box, iconScale, 1);
            }
            if (healthPickup.status == HEALTH_ACTIVE && healthPickup.position.row == row &&
                healthPickup.position.col == col)
                sprite(x + iconOffset, y + iconOffset, heart, iconScale, 3);
        }
    }
    /* 先画地图，再叠加叉车、机器人及携带图标；显示覆盖不改写地图数据。 */
    for (itemIndex = 0; itemIndex < forkCount; itemIndex++) {
        if (forkliftAI[itemIndex].chaseLeft > 0) {
            x = originX + forklifts[itemIndex].col * tile;
            y = originY + forklifts[itemIndex].row * tile;
            rect(x + 2, y + 2, tile - 4, tile - 4, 0x813C4D);
            rect(x + tile - 6, y + 3, 3, 6, 0xFFE0A3);
            rect(x + tile - 6, y + 11, 3, 3, 0xFFE0A3);
        }
        sprite(originX + forklifts[itemIndex].col * tile + iconOffset,
               originY + forklifts[itemIndex].row * tile + iconOffset, fork, iconScale, 2);
    }
    sprite(originX + robot.col * tile + iconOffset, originY + robot.row * tile + iconOffset, bot, iconScale, 0);
    if (carrying) {
        sprite(originX + robot.col * tile + tile - 13, originY + robot.row * tile + tile - 13, box, 1, 1);
    }

    /* 右侧面板：任务进度、携带状态、回合、种子和图例。 */
    offsetX = extraWidth;
    panel(906, 120, 334, 518 + extraHeight);
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
    snprintf(textBuffer, sizeof textBuffer, "LEVEL   %05u", seed);
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
    sprite(925, 575, heart, 1, 3);
    text(946, 578, "HEART +1 LIFE / MAX 3", 1, mutedColor);
    text(928, 594, "RED MARK - CHASING", 1, 0xFFAB9C);
    text(928, 608, "GREEN TILES - SAFE ZONE", 1, purpleColor);
    text(928, 624, healthPickup.status == HEALTH_NONE ? "HEALTH: NONE THIS LEVEL" :
         healthPickup.status == HEALTH_PENDING ? "HEALTH: WAITING" :
         healthPickup.status == HEALTH_ACTIVE ? "HEALTH: ON MAP" : "HEALTH: COLLECTED", 1, purpleColor);

    /* 底部操作说明和最近一次行动的消息。 */
    offsetX = 0; offsetY = extraHeight;
    rect(40, 659, 1200 + extraWidth, 1, lineColor);
    text(40, 677, "CONTROLS", 1, mutedColor);
    for (itemIndex = 0; itemIndex < 4; itemIndex++) {
        const char *keys[] = {"W", "A", "S", "D"};
        const char *labels[] = {"UP", "LEFT", "DOWN", "RIGHT"};
        x = 40 + itemIndex * 127;
        panel(x, 700, 34, 34);
        text(x + 11, 709, keys[itemIndex], 2, purpleColor);
        text(x + 45, 713, labels[itemIndex], 1, mutedColor);
    }
    text(575, 709, "SPACE PAUSE  R RESTART  N NEXT  Q QUIT", 1, mutedColor);
    text(40, 766, message, 1, purpleColor);
    text(1090 + extraWidth, 766, "RBE104TC / C", 1, mutedColor);
    /* 最后覆盖状态提示；生命耗尽优先于全部送达。 */
    if (paused || getGameStatus() != GAME_PLAYING) {
        offsetX = extraWidth / 2; offsetY = extraHeight / 2;
        panel(216, 362, 484, 118);
        text(246, 390,
             paused       ? "SHIFT PAUSED"
             : getGameStatus() == GAME_LOST ? "SHIFT ENDED"
                          : "ALL DELIVERED",
             3, whiteColor);
        text(246, 442, paused ? "SPACE TO CONTINUE" : getGameStatus() == GAME_WON ? "N NEXT LEVEL / R RESTART" : "PRESS R TO START AGAIN", 1, purpleColor);
    }
    offsetX = offsetY = 0;
}

void drawSetup(const char *seed, int count, int field, const char *error)
{
    char buffer[32];
    sceneWidth = 1280; sceneHeight = 800;
    offsetX = offsetY = 0;
    rect(0, 0, sceneWidth, sceneHeight, backgroundColor);
    text(352, 100, "PARCEL SHIFT", 8, whiteColor);
    text(424, 186, "CHOOSE YOUR DELIVERY RUN", 3, mutedColor);
    panel(300, 250, 680, 330);
    text(340, 284, "SEED / LEVEL   1-99999", 2, mutedColor);
    rect(328, 320, 624, 64, field == 0 ? lineColor : backgroundColor);
    text(352, 336, seed[0] ? seed : "-", 4, field == 0 ? purpleColor : whiteColor);
    text(340, 414, "FORKLIFTS   1-4", 2, mutedColor);
    rect(328, 450, 624, 64, field == 1 ? lineColor : backgroundColor);
    if (count > 0) snprintf(buffer, sizeof buffer, "%d", count);
    else snprintf(buffer, sizeof buffer, "-");
    text(352, 466, buffer, 4, field == 1 ? purpleColor : whiteColor);
    text(412, 478, "LEFT / RIGHT OR TYPE 1-4", 2, mutedColor);
    text(340, 544, "ONE PLAYER ROBOT - AVOID THE FORKLIFTS", 1, mutedColor);
    text(340, 610, "TAB / UP / DOWN: SELECT FIELD", 2, mutedColor);
    text(340, 648, "SEED: TYPE DIGITS / BACKSPACE / DELETE", 2, mutedColor);
    text(340, 694, "ENTER: START    ESC: QUIT", 3, purpleColor);
    text(340, 754, error, 2, 0xFFAB9C);
}

void drawCollisionNotice(void)
{
    offsetX = (sceneWidth - 1280) / 2; offsetY = (sceneHeight - 800) / 2;
    panel(360, 310, 560, 180);
    text(400, 340, "COLLISION", 4, 0xFFAB9C);
    text(400, 392, "-1 LIFE", 3, whiteColor);
    text(400, 435, lives > 0 ? "IF CARRYING: PACKAGE DROPPED" : "NO LIVES LEFT", 2, purpleColor);
    text(400, 465, "CONTINUING IN 1.5 SECONDS", 1, mutedColor);
    offsetX = offsetY = 0;
}

int savePreview(const char *fileName)
{
    FILE *outputFile = fopen(fileName, "wb");
    int itemIndex;
    if (!outputFile) {
        return 0;
    }
    /* P6 在头部后直接写 R/G/B 字节，不包含 alpha，也不受主机字节序影响。
     * 用 wb 避免 Windows 文本模式改写像素中的换行字节。 */
    fprintf(outputFile, "P6\n%d %d\n255\n", sceneWidth, sceneHeight);
    for (itemIndex = 0; itemIndex < sceneWidth * sceneHeight; itemIndex++) {
        fputc((int)(pixels[itemIndex] >> 16) & 255, outputFile);
        fputc((int)(pixels[itemIndex] >> 8) & 255, outputFile);
        fputc((int)pixels[itemIndex] & 255, outputFile);
    }
    return fclose(outputFile) == 0;
}
