/* 用正式回合函数搜索一条获胜路线，不另写叉车或碰撞规则。
 * 搜索临时借用全局游戏状态，退出时恢复；本模块不可并发执行。
 * 固定节点数、深度和方向顺序，使候选验收不依赖机器运行速度。
 * 搜索失败只表示在这些限制下未找到解，不是不可通关的证明。 */
#include <stdlib.h>
#include <string.h>
#include "game.h"

#define SEARCH_NODES 12000
/* 表容量大于最大节点数，线性探测总能遇到空槽。 */
#define HASH_SLOTS 32768
#define SEGMENT_DEPTH 256

typedef struct {
    GameSnapshot state;
    /* parent 为前驱节点下标，根为 -1；command 是前驱到本节点的操作。
     * depth 是当前段已用步数，priority 是堆排序依据，均不代表整局回合数。 */
    int parent, depth, priority;
    char command;
} SearchNode;

static int sameState(const GameSnapshot *a, const GameSnapshot *b)
{
    /* 相同棋面但随机数状态不同，下一步叉车可能不同，必须分别搜索。
     * 回血待生成时回合数决定掉落时间，须参与去重；其他状态下忽略回合数。
     * 分数由已配送数决定，暂停在搜索中不变化；提示文本不影响规则。 */
    return a->bot.row == b->bot.row && a->bot.col == b->bot.col &&
        a->lifeCount == b->lifeCount && a->hasPackage == b->hasPackage &&
        a->deliveredCount == b->deliveredCount && a->randomState == b->randomState &&
        a->health.status == b->health.status && a->health.spawnTurn == b->health.spawnTurn &&
        a->health.position.row == b->health.position.row && a->health.position.col == b->health.position.col &&
        (a->health.status != HEALTH_PENDING || a->turnCount == b->turnCount) &&
        memcmp(a->traffic, b->traffic, sizeof a->traffic) == 0 &&
        memcmp(a->trafficAI, b->trafficAI, sizeof a->trafficAI) == 0 &&
        memcmp(a->board, b->board, sizeof a->board) == 0;
}

static uint32_t stateHash(const GameSnapshot *s)
{
    /* 哈希仅选择起始槽，不代替 sameState。即使不同地图或包裹状态
     * 得到相同哈希，仍通过完整状态比较和线性探测区分。 */
    uint32_t h = s->randomState;
    int i;
    h = (h ^ (uint32_t)(s->bot.row * MAX_COLS + s->bot.col)) * UINT32_C(16777619);
    for (i = 0; i < forkCount; i++)
        h = (h ^ (uint32_t)(s->traffic[i].row * MAX_COLS + s->traffic[i].col)) * UINT32_C(16777619);
    h = (h ^ (uint32_t)s->lifeCount) * UINT32_C(16777619);
    h = (h ^ (uint32_t)s->health.status) * UINT32_C(16777619);
    if (s->health.status == HEALTH_PENDING)
        h = (h ^ (uint32_t)s->turnCount) * UINT32_C(16777619);
    return h;
}

static int before(const SearchNode *nodes, int a, int b)
{
    /* 同优先级时先扩展较深节点，再按创建顺序打破平局；不得用地址排序。 */
    if (nodes[a].priority != nodes[b].priority) return nodes[a].priority < nodes[b].priority;
    if (nodes[a].depth != nodes[b].depth) return nodes[a].depth > nodes[b].depth;
    return a < b;
}

static void push(int *heap, int *size, const SearchNode *nodes, int node)
{
    /* 最小堆存节点下标，不移动包含整张地图的 SearchNode。
     * 新下标先放到末尾，沿父节点 (index-1)/2 上浮。 */
    int index = (*size)++;
    while (index > 0 && before(nodes, node, heap[(index - 1) / 2])) {
        heap[index] = heap[(index - 1) / 2];
        index = (index - 1) / 2;
    }
    heap[index] = node;
}

static int pop(int *heap, int *size, const SearchNode *nodes)
{
    /* 取出堆顶，用最后一个元素填补，并沿较优的子节点向下调整。
     * 调用方仅在 size>0 时调用；弹出最后一个元素时不再写回根。 */
    int result = heap[0], last = heap[--*size], index = 0, child;
    while (index * 2 + 1 < *size) {
        child = index * 2 + 1;
        if (child + 1 < *size && before(nodes, heap[child + 1], heap[child])) child++;
        if (!before(nodes, heap[child], last)) break;
        heap[index] = heap[child]; index = child;
    }
    if (*size > 0) heap[index] = last;
    return result;
}

/* 从全部剩余包裹或配送点反向做多源 BFS，得到每格到最近目标的距离。
 * -1 表示不可达；只考虑墙，不考虑叉车，作为搜索方向的估计值。
 * 实际是否碰撞仍由 takeTurn 判断，不能根据这张距离表直接判定通关。 */
static void goalDistances(int distance[MAX_ROWS][MAX_COLS], int toDelivery)
{
    Position queue[MAX_ROWS * MAX_COLS];
    int r, c, i, head = 0, tail = 0;
    const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
    for (r = 0; r < MAX_ROWS; r++)
        for (c = 0; c < MAX_COLS; c++) distance[r][c] = -1;
    for (r = 0; r < mapRows; r++)
        for (c = 0; c < mapCols; c++)
            if (map[r][c] == (toDelivery ? 'D' : 'P')) {
                distance[r][c] = 0; queue[tail++] = (Position){r, c};
            }
    while (head < tail) {
        Position p = queue[head++];
        for (i = 0; i < 4; i++) {
            r = p.row + dr[i]; c = p.col + dc[i];
            if (r >= 0 && r < mapRows && c >= 0 && c < mapCols && map[r][c] != '#' && distance[r][c] < 0) {
                distance[r][c] = distance[p.row][p.col] + 1;
                queue[tail++] = (Position){r, c};
            }
        }
    }
}

static int findSegment(SearchNode *nodes, int *heap, int *table, char *route, int *length)
{
    int distance[MAX_ROWS][MAX_COLS], count = 1, heapSize = 0, current, i, index, slot;
    int toDelivery = carrying, previousDelivered = delivered, initialLives = lives;
    /* 不携带时找任意一个包裹，携带时找一次成功配送；一段只完成一个目标。
     * 固定方向顺序也属于确定性的一部分，修改后可能改变选中的获胜路线。 */
    const char commands[] = "wsad";
    char reverse[SEGMENT_DEPTH];
    memset(table, 0, HASH_SLOTS * sizeof *table);
    goalDistances(distance, toDelivery);
    if (distance[robot.row][robot.col] < 0) return 0;
    captureGame(&nodes[0].state);
    nodes[0].parent = -1; nodes[0].depth = 0;
    nodes[0].priority = distance[robot.row][robot.col];
    /* table 用 0 表示空槽，其他值为节点下标+1，避免与根下标 0 冲突。 */
    table[stateHash(&nodes[0].state) % HASH_SLOTS] = 1;
    push(heap, &heapSize, nodes, 0);
    while (heapSize > 0) {
        current = pop(heap, &heapSize, nodes);
        if (nodes[current].depth >= SEGMENT_DEPTH) continue;
        for (i = 0; i < 4; i++) {
            GameSnapshot candidate;
            /* 每个分支都从同一前驱快照出发，包括恢复随机状态。
             * 撞墙等无效命令不扩展；碰撞后仍有生命的分支允许继续搜索。 */
            restoreGame(&nodes[current].state);
            if (!takeTurn(commands[i]) || getGameStatus() == GAME_LOST) continue;
            if ((toDelivery && delivered > previousDelivered) || (!toDelivery && carrying)) {
                int n = 0;
                /* 当前操作尚未存成节点：先记录它，再沿 parent 回溯，
                 * 最后反转追加到整局路线。返回时全局状态停在本段目标处，
                 * 下一段直接以该状态为起点，不必重新执行这段路线。 */
                reverse[n++] = commands[i];
                for (index = current; nodes[index].parent >= 0; index = nodes[index].parent)
                    reverse[n++] = nodes[index].command;
                if (*length + n > MAX_SOLUTION) return 0;
                while (n > 0) route[(*length)++] = reverse[--n];
                route[*length] = '\0';
                return 1;
            }
            captureGame(&candidate);
            slot = (int)(stateHash(&candidate) % HASH_SLOTS);
            while (table[slot] && !sameState(&candidate, &nodes[table[slot] - 1].state))
                slot = (slot + 1) % HASH_SLOTS;
            /* 这里只保留首次发现的状态，不保证最短路线或搜索完备性。
             * 有界搜索失败会由上层重试候选，成功则还要正式回放验证。 */
            if (table[slot]) continue;
            if (count == SEARCH_NODES) return 0;
            nodes[count].state = candidate;
            nodes[count].parent = current; nodes[count].command = commands[i];
            nodes[count].depth = nodes[current].depth + 1;
            /* 步数+静态距离优先接近目标；每扣一命增加 100 的代价，
             * 倾向保命但不禁止利用碰撞重生。此值不改变正式游戏分数。 */
            nodes[count].priority = nodes[count].depth + distance[robot.row][robot.col] +
                (initialLives - lives) * 100;
            table[slot] = count + 1;
            push(heap, &heapSize, nodes, count++);
        }
    }
    return 0;
}

int findWinningRoute(char route[MAX_SOLUTION + 1])
{
    /* 搜索节点较大，放在堆上；三块缓冲区在各段搜索之间复用。
     * 任意 malloc 失败都返回未验证，释放已成功分配的部分。 */
    SearchNode *nodes = malloc(SEARCH_NODES * sizeof *nodes);
    int *heap = malloc(SEARCH_NODES * sizeof *heap);
    int *table = malloc(HASH_SLOTS * sizeof *table);
    int length = 0, segments = 0, result = 0;
    GameSnapshot initial;
    char initialMessage[sizeof message];
    /* 状态快照不含 message，因此另存提示文本；求解不能推进玩家的游戏。 */
    captureGame(&initial);
    memcpy(initialMessage, message, sizeof message);
    route[0] = '\0';
    if (nodes && heap && table) {
        /* 每个包裹最多需要一段拾取和一段配送。
         * 每段选定后不回溯之前的段，因此可能拒绝实际有解的候选。 */
        while (getGameStatus() == GAME_PLAYING && segments++ < totalPackages * 2)
            if (!findSegment(nodes, heap, table, route, &length)) break;
        result = getGameStatus() == GAME_WON;
    }
    /* 成败都恢复现场；失败时清空路线，防止上层误用未完成的前缀。 */
    if (!result) route[0] = '\0';
    restoreGame(&initial);
    memcpy(message, initialMessage, sizeof message);
    free(table); free(heap); free(nodes);
    return result;
}
