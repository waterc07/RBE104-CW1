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
    x += offsetX;
    y += offsetY;
    /* 逐像素裁剪到画布内，越界部分不写入缓冲区。 */
    for (pixelRow = y; pixelRow < y + rectHeight; pixelRow++) {
        for (itemIndex = x; itemIndex < x + rectWidth; itemIndex++) {
            if (itemIndex >= 0 && itemIndex < sceneWidth && pixelRow >= 0 &&
                pixelRow < sceneHeight) {
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
        glyphIndex = -1;
        if (glyphChar >= 'A' && glyphChar <= 'Z') {
            glyphIndex = glyphChar - 'A';
        } else if (glyphChar >= '0' && glyphChar <= '9') {
            glyphIndex = 26 + glyphChar - '0';
        }
        if (glyphIndex >= 0) {
            for (row = 0; row < 7; row++) {
                for (col = 0; col < 5; col++) {
                    /* 每行的低五位从左到右对应 bit4..bit0，置位才绘制色块。 */
                    if (font[glyphIndex][row] & (1 << (4 - col))) {
                        rect(x + itemIndex * 6 * scale + col * scale, y + row * scale, scale, scale,
                             color);
                    }
                }
            }
        } else if (glyphChar == '-') {
            rect(x + itemIndex * 6 * scale, y + 3 * scale, 5 * scale, scale, color);
        } else if (glyphChar == '/') {
            for (row = 0; row < 7; row++) {
                rect(x + itemIndex * 6 * scale + (4 - row * 4 / 6) * scale, y + row * scale, scale,
                     scale, color);
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

/* 菜单与游戏共用同一块像素缓冲区。菜单画布固定 1280×720；游戏开始后
 * drawScene() 会按地图重新设置画布，窗口据此重建纹理。 */
void menuCanvas(const uint32_t *background)
{
    int i;
    sceneWidth = 1280;
    sceneHeight = 720;
    offsetX = offsetY = 0;
    if (background) {
        /* 背景已转成 0x00RRGGBB，可直接整块复制。 */
        memcpy(pixels, background, 1280 * 720 * sizeof *pixels);
    } else {
        for (i = 0; i < 1280 * 720; i++) {
            pixels[i] = 0x170D29;
        }
    }
}

void menuRect(int x, int y, int width, int height, uint32_t color)
{
    rect(x, y, width, height, color);
}

void menuText(int x, int y, const char *content, int scale, uint32_t color)
{
    text(x, y, content, scale, color);
}

/* 12×12 图标：'1'..'5' 索引调色板，'.' 跳过绘制以保留背景。 */static const char *bot[12] = {
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
    const char *healthText;
    const char *statusTitle;
    const char *statusHint;
    updateSceneSize();
    extraWidth = sceneWidth - 1280;
    extraHeight = sceneHeight - 800;
    offsetX = offsetY = 0;
    /* 836×396 是初始地图区域。扩展后的剩余空间均分到两侧，
     * originX/originY 是地图左上角，后续再按行列乘格子尺寸定位。 */
    originX = 40 + (836 + extraWidth - tile * mapCols) / 2;
    originY = 242 + (396 + extraHeight - tile * mapRows) / 2;
    /* 内嵌图标为 12×12，使用整数倍率避免像素边缘模糊；offset 负责居中。 */
    iconScale = (tile - 4) / 12;
    if (iconScale < 1) {
        iconScale = 1;
    }
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
            if (map[row][col] != '#' && isSafeZone(row, col)) {
                rect(x + 1, y + 1, tile - 2, tile - 2, 0x284238);
            }
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
                healthPickup.position.col == col) {
                sprite(x + iconOffset, y + iconOffset, heart, iconScale, 3);
            }
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
    sprite(originX + robot.col * tile + iconOffset, originY + robot.row * tile + iconOffset, bot,
           iconScale, 0);
    if (carrying) {
        sprite(originX + robot.col * tile + tile - 13, originY + robot.row * tile + tile - 13, box,
               1, 1);
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
    if (healthPickup.status == HEALTH_NONE) {
        healthText = "HEALTH: NONE THIS LEVEL";
    } else if (healthPickup.status == HEALTH_PENDING) {
        healthText = "HEALTH: WAITING";
    } else if (healthPickup.status == HEALTH_ACTIVE) {
        healthText = "HEALTH: ON MAP";
    } else {
        healthText = "HEALTH: COLLECTED";
    }
    text(928, 624, healthText, 1, purpleColor);

    /* 底部操作说明和最近一次行动的消息。 */
    offsetX = 0;
    offsetY = extraHeight;
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
    text(575, 709, "SPACE PAUSE  R RESTART  N NEXT  M MENU  Q QUIT", 1, mutedColor);
    text(575, 729, "ESC MENU  F11 FULLSCREEN", 1, mutedColor);
    text(40, 766, message, 1, purpleColor);
    text(1090 + extraWidth, 766, "RBE104TC / C", 1, mutedColor);
    /* 最后覆盖暂停或失败提示；获胜画面由 drawVictoryCelebration() 绘制。 */
    if (paused || getGameStatus() == GAME_LOST) {
        offsetX = extraWidth / 2;
        offsetY = extraHeight / 2;
        panel(216, 362, 484, 118);
        if (paused) {
            statusTitle = "SHIFT PAUSED";
            statusHint = "SPACE TO CONTINUE";
        } else {
            statusTitle = "SHIFT ENDED";
            statusHint = "PRESS R TO START AGAIN";
        }
        text(246, 390, statusTitle, 3, whiteColor);
        text(246, 442, statusHint, 1, purpleColor);
    }
    offsetX = offsetY = 0;
}

void drawCollisionNotice(void)
{
    offsetX = (sceneWidth - 1280) / 2;
    offsetY = (sceneHeight - 800) / 2;
    panel(360, 310, 560, 180);
    text(400, 340, "COLLISION", 4, 0xFFAB9C);
    text(400, 392, "-1 LIFE", 3, whiteColor);
    text(400, 435, lives > 0 ? "IF CARRYING: PACKAGE DROPPED" : "NO LIVES LEFT", 2, purpleColor);
    text(400, 465, "CONTINUING IN 1.5 SECONDS", 1, mutedColor);
    offsetX = offsetY = 0;
}

/* 用矩形积木画抱箱机器人。坐标以 6 像素为单位，表内顺序就是绘制顺序：
 * 先头身和面部，再箱子与手臂，最后眼睛。程序内绘制，不加载外部图片。 */
static void celebrationBot(int x, int y, int wink)
{
    typedef struct {
        int x, y, w, h;
        uint32_t color;
    } Part;
    static const Part parts[] = {
        {15, 0, 2, 5, 0x8060B5},   {14, 0, 4, 2, 0xBBFFE6},   {7, 4, 20, 15, 0x493261},
        {8, 4, 18, 2, 0xF0DFFF},   {7, 6, 20, 11, 0xBA91EC},   {8, 17, 18, 2, 0x8762B5},
        {4, 8, 3, 8, 0x50376F},    {4, 9, 2, 6, 0x8CEBD7},    {27, 8, 3, 8, 0x50376F},
        {28, 9, 2, 6, 0x8CEBD7},   {9, 7, 16, 10, 0x21172F},  {10, 7, 14, 1, 0x59436E},
        {11, 10, 3, 3, 0xB2FFE8},  {11, 10, 1, 1, 0xF0FFF9},  {15, 14, 5, 1, 0xE5A4F2},
        {16, 15, 3, 1, 0xE5A4F2},  {10, 15, 2, 1, 0x9D6BBA},  {22, 15, 2, 1, 0x9D6BBA},
        {9, 19, 16, 8, 0x8C64BC},  {11, 20, 12, 6, 0xD2B4F4}, {9, 26, 5, 5, 0xB59BD7},
        {20, 26, 5, 5, 0xB59BD7},  {8, 30, 7, 2, 0x47335F},   {19, 30, 7, 2, 0x47335F},
        {9, 30, 5, 1, 0xA8F4DB},   {20, 30, 5, 1, 0xA8F4DB},  {7, 18, 20, 11, 0x82512F},
        {8, 18, 18, 2, 0xFFE39A},  {8, 20, 18, 8, 0xE4A64D},  {9, 21, 16, 6, 0xF5BC62},
        {15, 18, 4, 3, 0xC4894C},  {16, 21, 2, 5, 0xD0924B},  {23, 24, 2, 3, 0xAC743E},
        {22, 25, 4, 1, 0xAC743E},  {5, 20, 4, 5, 0x4E356B},   {5, 20, 3, 4, 0xD9BCF6},
        {25, 20, 4, 5, 0x4E356B},  {26, 20, 3, 4, 0xD9BCF6}};
    size_t i;
    for (i = 0; i < sizeof parts / sizeof parts[0]; i++) {
        rect(x + parts[i].x * 6, y + parts[i].y * 6, parts[i].w * 6, parts[i].h * 6,
             parts[i].color);
    }
    if (wink) {
        /* 跳跃时眨眼：把眼睛画成弧形亮线，落地后恢复完整眼睛。 */
        rect(x + 21 * 6, y + 10 * 6, 2 * 6, 6, 0xB2FFE8);
        rect(x + 20 * 6, y + 11 * 6, 2 * 6, 6, 0xB2FFE8);
        rect(x + 21 * 6, y + 12 * 6, 2 * 6, 6, 0xB2FFE8);
    } else {
        rect(x + 21 * 6, y + 10 * 6, 3 * 6, 3 * 6, 0xB2FFE8);
    }
}

/* 按比例压暗单个颜色；烟花余烬和尾迹逐级变暗时使用，不做整屏混合。 */
static uint32_t fadeColor(uint32_t color, int amount)
{
    return (((color >> 16) & 255) * amount / 255 << 16) |
           (((color >> 8) & 255) * amount / 255 << 8) | ((color & 255) * amount / 255);
}

/* 一组烟花的循环动画。delay 让三组先后出现；elapsed 取模使动画无限循环，
 * 不调用游戏的随机数，因此庆祝不会改变下一回合的叉车行为。 */
static void firework(int x, int y, uint32_t elapsed, uint32_t delay, uint32_t color)
{
    /* 16 个固定方向用千分比整数表示，避免浮点运算和平台差异。 */
    static const int dx[16] = {1000, 924, 707, 383, 0, -383, -707, -924,
                               -1000, -924, -707, -383, 0, 383, 707, 924};
    static const int dy[16] = {0, 383, 707, 924, 1000, 924, 707, 383,
                               0, -383, -707, -924, -1000, -924, -707, -383};
    int age, i, j;
    if (elapsed < delay) {
        return;
    }
    age = (int)((elapsed - delay) % 2100);
    if (age < 260) {
        /* 升空阶段：火点从低处升到爆点，并带一小段尾迹。 */
        int rise = 90 - age * 90 / 260;
        rect(x, y + rise, 4, 9, color);
        rect(x + 1, y + rise + 12, 2, 8, fadeColor(color, 120));
        return;
    }
    age -= 260;
    if (age > 1100) {
        return;
    }
    /* 爆开阶段：不同方向速度略有差别，t*t 近似重力下坠；
     * 同一方向画三层，越旧越暗越大，形成尾迹。 */
    for (i = 0; i < 16; i++) {
        for (j = 2; j >= 0; j--) {
            int t = age - j * 35, radius, fall, size;
            if (t < 0) {
                continue;
            }
            radius = t * (i % 2 ? 88 : 110) / 1000;
            fall = t * t / 18000;
            size = j ? 3 : 5;
            rect(x + dx[i] * radius / 1000, y + dy[i] * radius / 1000 + fall, size, size,
                 fadeColor(color, (255 - age * 180 / 1100) / (j + 1)));
        }
    }
}

void drawVictoryCelebration(uint32_t elapsed)
{
    int i, phase = (int)(elapsed % 1600), lift = 0, shadow;
    char stats[80];
    /* 只有共享核心确认获胜时才覆盖画面；重开或失败后调用本函数不绘制。 */
    if (getGameStatus() != GAME_WON) {
        return;
    }
    offsetX = offsetY = 0;
    /* 保留压暗的地图作为背景，庆祝卡片按当前画布居中，兼容所有地图尺寸。 */
    for (i = 0; i < sceneWidth * sceneHeight; i++) {
        pixels[i] = (pixels[i] & 0xFCFCFC) >> 2;
    }
    offsetX = (sceneWidth - 880) / 2;
    offsetY = (sceneHeight - 600) / 2;
    rect(0, 0, 880, 600, 0x9971CB);
    rect(3, 3, 874, 594, 0x1B112D);
    rect(20, 20, 840, 4, 0x39274F);
    text(356, 38, "SHIFT COMPLETE", 2, 0xBBA3D5);
    text(284, 72, "ALL DELIVERED", 4, 0xF5E8FF);
    snprintf(stats, sizeof stats, "LEVEL %05u / SCORE %d / LIVES %d", currentLevel.number, score,
             lives);
    text((880 - (int)strlen(stats) * 12) / 2, 117, stats, 2, 0xA7EBD3);
    /* 背景星点缓慢闪烁；相位只用 elapsed，与游戏回合无关。 */
    for (i = 0; i < 22; i++) {
        int starX = 45 + (i * 137) % 790;
        int starY = 160 + (i * 61) % 290;
        rect(starX, starY, 2, 2,
             ((elapsed / 240 + (uint32_t)i) % 5 == 0) ? 0xBDA0E2 : 0x513965);
    }
    firework(177, 250, elapsed, 0, 0xC89BFF);
    firework(705, 226, elapsed, 400, 0xFFD484);
    firework(440, 202, elapsed, 900, 0x8DE8D1);
    rect(224, 478, 432, 2, 0x4B315F);
    /* 整数抛物线：phase 在 0..1050 间往返，中点达到最高跳跃高度。 */
    if (phase < 1050) {
        lift = 4 * 86 * phase * (1050 - phase) / (1050 * 1050);
    }
    shadow = 142 - lift / 2;
    rect(440 - shadow / 2, 465, shadow, 8, 0x100B1B);
    rect(440 - shadow / 2 + 12, 473, shadow - 24, 3, 0x2E1D3F);
    celebrationBot(338, 274 - lift, phase < 1050);
    /* 落地瞬间的尘土只持续 120ms；不做整屏闪光，避免遮住结算文字。 */
    if (phase >= 1050 && phase < 1170) {
        int dust = (phase - 1050) / 6;
        rect(340 - dust, 458 - dust / 3, 6, 4, 0xB18BCC);
        rect(536 + dust, 458 - dust / 3, 6, 4, 0xB18BCC);
    }
    text(272, 497, "EVERY PARCEL HOME. NICE WORK.", 2, 0xD1B6EB);
    /* 三个操作入口与 window.c 的鼠标命中区域使用同一组坐标。 */
    panel(64, 539, 240, 38);
    panel(320, 539, 240, 38);
    panel(576, 539, 240, 38);
    text(88, 551, currentLevel.number < 99999 ? "N  NEXT DELIVERY" : "FINAL LEVEL CLEAR", 2,
         0xC6A0FF);
    text(356, 551, "R  PLAY AGAIN", 2, 0xE6D6FA);
    text(618, 551, "M  MAIN MENU", 2, 0xE6D6FA);
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
