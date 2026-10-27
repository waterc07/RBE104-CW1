# 维护与交接

运行方法见 [README](../README.md)，已执行的检查见[测试记录](TEST_RESULTS.md)。以下源码路径相对于 `game/`。

## 修改位置

| 内容 | 文件 / 符号 |
| --- | --- |
| 地图、包裹位置 | `src/level.c` 的 `generateLevel`；约定见 [LEVELS.md](LEVELS.md) |
| 各地图包裹数量 | `src/level.c` 的 `levelPackageCount`；生成与结构校验共用，求解段数按 `totalPackages` 计算 |
| 机器人和叉车起点 | `currentLevel.start`、`currentLevel.forkStarts` |
| 拾取、配送、得分 | `src/game.c` 的 `handlePackage` |
| 叉车移动 | `src/game.c` 的 `moveForklifts` |
| 回合顺序、扣命与重生 | `src/game.c` 的 `takeTurn` |
| 随机回血掉落与拾取 | `src/game.c` 的 `restartGame`、`spawnHealth`、`takeTurn`；快照和求解去重同步保留道具状态 |
| 配色、图标、字体和 HUD | `src/scene.c` |
| 固定格子尺寸、画布与窗口大小 | `src/scene.h` 的 `TILE_SIZE`、`src/scene.c` 的 `updateSceneSize`、`src/window.c` 的 `resizeScene` |
| 共享暂停、重开、胜负判断 | `src/game.c` 的 `processCommand`、`getGameStatus` |
| SDL 按键映射、窗口与缩放 | `src/window.c` |
| 终端输入、显示及命令映射 | `src/terminal.c` |
| 规则回归检查 | `tests/test_game.c` |

`terminal.c` 和 `window.c` 都链接 `game.c`，游戏操作统一调用 `processCommand()`，胜负状态读取 `getGameStatus()`。不要在前端复制规则。修改共享接口时同步更新 `game.h`、`scene.h` 及调用处。

## 双前端维护约定（2026-10-23）

- 终端版是课程提交所需入口，SDL 版继续开发，两端保持相同的游戏功能。
- 已实现：共享游戏核心处理状态、规则、回合、暂停、重开和胜负判断，终端与 SDL 分别处理输入和显示。
- 新功能和修复必须覆盖两端；按键、输入方式和画面可不同，规则、得分及胜负结果应一致。暂停、重开等游戏操作也需提供对应入口。
- 修改共享规则后验证两个前端；相同版本、关卡号、叉车数和游戏操作应得到一致的状态结果。记录已验证的行为和待补差异，不将维护目标写成已实现功能。

关卡生成与求解分别位于 `src/level.c` 和 `src/solver.c`。`initialise()` 返回成功/失败；检查成功后才进入游戏，R 使用 `restartGame()` 恢复保存的关卡。

## 修改后的检查

- 改规则：运行基础构建，在 `tests/test_game.c` 中增加对应边界的断言，并运行 `python tests/test_terminal.py` 检查终端命令行为。该测试需要已构建的 `build/windows/terminal.exe`。
- 改显示：运行基础构建生成 PPM，查看实际画面；需要更新 README 配图时再替换 `docs/images/preview.png`。
- 改窗口或按键：编译 GUI，并在实际 SDL 窗口验证。自动测试不经过窗口事件循环。
- 记录编译器、seed、叉车数、输入顺序、预期和实际结果。测试中手动修改状态的场景应明确标注。

## 提交前待办

- 以终端版准备课程提交；如附交 SDL 版，确认附交方式。在课程指定环境验证构建和运行。
- 完成窗口移动、暂停、重开、缩放、通关与失败检查，记录 seed、叉车数及按键路线。
- 补充终端版人工交互试玩；共享核心断言与终端重定向输入的胜负流程已覆盖，范围见测试记录。
- 核对携带包裹重生、无路可走的叉车、最终配送与致命碰撞等细节是否符合教师说明。
- 准备不超过 1500 词的 PDF 报告、源码、构建运行说明、至少三个可复现场景，以及真实贡献和协助声明。
- 核对课程通知中的截止日期与配套评分表；当前目录未包含评分表。

分工由组内认领，贡献声明按实际工作填写。当前目录包含开发产物，不是最终提交包。
