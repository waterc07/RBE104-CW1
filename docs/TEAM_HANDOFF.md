# 维护与交接

运行方法见 [README](../README.md)，已执行的检查见[测试记录](TEST_RESULTS.md)。以下源码路径相对于 `game/`。

## Git 仓库交接范围

- 仓库包含源码、头文件、Windows 构建脚本、测试、六个 WAV、菜单背景 BMP/PNG、图标、预览图、维护文档、[report 素材](REPORT_MATERIALS_ZH.md)及[作业说明原件](assignment.pdf)。不依赖仓库上级目录的文件。
- 接收方需要 GCC；终端集成测试另需 Python 3。SDL 窗口构建和音频、窗口测试另需匹配 GCC 架构的 MinGW SDL2 开发库及 `windres`，目录配置见 README。
- `build/` 和 `deps/` 不纳入 Git。仓库交接的是源码，接收方需构建后运行；现有 EXE、DLL、生成预览和测试日志不随仓库交付，测试命令与结果保留在 TEST_RESULTS。
- `assets/parcel_shift.png` 是图标源图，`assets/menu_background.png` 是菜单背景源图（运行时用同目录 BMP），`game.code-workspace` 是可选编辑器入口；这些素材不参与游戏规则，可保留供维护。测试和音频生成脚本用于复现与修改，不建议从同伴交接仓库中删除。
- 最终报告、组员贡献声明、课程评分表和最新课程通知需组内另行准备。仓库内的素材不是最终报告；作业 PDF 不代替后续通知。
- 新增文件纳入 Git 并提交后再导出。`git archive HEAD` 只包含已提交内容，不包含暂存或工作区修改；直接复制整个目录会混入忽略的构建输出。

## 修改位置

| 内容 | 文件 / 符号 |
| --- | --- |
| 地图、包裹位置 | `src/level.c` 的 `generateLevel`；约定见 [LEVELS.md](LEVELS.md) |
| 各地图包裹数量 | `src/level.c` 的 `levelPackageCount`；生成与结构校验共用，求解段数按 `totalPackages` 计算 |
| 机器人和叉车起点 | `currentLevel.start`、`currentLevel.forkStarts` |
| 拾取、配送、得分 | `src/game.c` 的 `handlePackage` |
| 叉车移动 | `src/game.c` 的 `moveForklifts` |
| 第五叉车与验收数量 | `src/level.h` 的 `MAX_FORKLIFTS`、`src/level.c` 的候选出生点、`src/game.c` 的 `initialise` 与 `checkedLimit`：1–4 仍验收原四种配置，选 5 时额外验收第五种 |
| 回合顺序、扣命与重生 | `src/game.c` 的 `takeTurn` |
| 随机回血掉落与拾取 | `src/game.c` 的 `restartGame`、`spawnHealth`、`takeTurn`；快照和求解去重同步保留道具状态 |
| 配色、图标、字体和 HUD | `src/scene.c` |
| 固定格子尺寸、画布与窗口大小 | `src/scene.h` 的 `TILE_SIZE`、`src/scene.c` 的 `updateSceneSize`、`src/window.c` 的 `resizeScene` |
| 共享暂停、重开、胜负判断 | `src/game.c` 的 `processCommand`、`getGameStatus` |
| SDL 菜单流程、按键映射、窗口、缩放、全屏与音效触发 | `src/window.c`；菜单背景加载、按钮表、预设与档位在 `src/menu.c/.h` |
| SDL 菜单绘制入口 | `src/scene.c` 的 `menuCanvas`、`menuRect`、`menuText` |
| 胜利庆祝动画 | `src/scene.c` 的 `drawVictoryCelebration` 及 `celebrationBot`、`firework`；计时和鼠标点按映射在 `src/window.c` |
| 背景音乐与事件音效 | `src/audio.c/.h`、`assets/*.wav`；生成脚本在 `tools/make_music.py`、`tools/make_sfx.py` |
| 菜单背景 | `assets/menu_background.bmp`（运行时）与 `assets/menu_background.png`（源图） |
| EXE 图标 | `assets/parcel_shift.ico`、`game_icon.rc`；GUI 构建用 `windres` 编译 |
| 终端输入、显示及命令映射 | `src/terminal.c` |
| 规则回归检查 | `tests/test_game.c`（含五叉车验收与回放）；`tests/test_celebration.c` |
| SDL 菜单与窗口流程检查 | `tests/test_menu.c`、`tests/test_window_flow.c`、`tests/test_victory_flow.c`，构建目标 `window-test` |
| WAV 与混音检查 | `tests/test_audio.c`，构建目标 `audio` |

`terminal.c` 和 `window.c` 都链接 `game.c`，游戏操作统一调用 `processCommand()`，胜负状态读取 `getGameStatus()`。不要在前端复制规则。修改共享接口时同步更新 `game.h`、`scene.h` 及调用处。`menu.c` 只保存待提交的配置，真正开始新局仍调用 `initialise()`；菜单的后台验证线程期间主线程只绘制菜单，不读取局内状态。

## 双前端维护约定（2026-10-23）

- 终端版是课程提交所需入口，SDL 版继续开发，两端保持相同的游戏功能。
- 已实现：共享游戏核心处理状态、规则、回合、暂停、重开和胜负判断，终端与 SDL 分别处理输入和显示。SDL 开始菜单负责关卡号、2–5 辆机器人、地图档位和难度预设；终端仍按行输入叉车数（1–5）和关卡号。
- 新功能和修复必须覆盖两端；按键、输入方式和画面可不同，规则、得分及胜负结果应一致。暂停、重开等游戏操作也需提供对应入口。SDL 的返回菜单/继续是前端功能，不改变共享规则。
- 第五叉车沿用同一生成器的洗牌结果：前四个出生点不变，选择 5 时在 `initialise` 中额外验收第 5 种配置；1–4 的地图与旧版一致。
- 修改共享规则后验证两个前端；相同版本、关卡号、叉车数和游戏操作应得到一致的状态结果。记录已验证的行为和待补差异，不将维护目标写成已实现功能。

关卡生成与求解分别位于 `src/level.c` 和 `src/solver.c`。`initialise()` 返回成功/失败；检查成功后才进入游戏，R 使用 `restartGame()` 恢复保存的关卡。

## 修改后的检查

- 改规则：运行基础构建，在 `tests/test_game.c` 中增加对应边界的断言，并运行 `python tests/test_terminal.py` 检查终端命令行为。该测试需要已构建的 `build/windows/terminal.exe`。改叉车数量或验收条件时同时检查 `tests/test_game.c` 的五叉车用例与 `tests/test_celebration.c`。
- 改显示：运行基础构建生成 PPM，查看实际画面；需要更新 README 配图时再替换 `docs/images/preview.png`。
- 改菜单或庆祝动画：运行 `scripts\build_basic.bat window-test` 检查菜单点击、预设、种子编辑、继续、全屏、胜利卡片与下一关；再在实际窗口查看背景、缩放和鼠标命中。
- 改音频：运行 `scripts\build_basic.bat audio` 检查 WAV 和混音，再在实际 SDL 窗口试听移动、拾取、碰撞、配送、通关、暂停、重开和退出。离线测试不检查声音设备或事件触发。分发时保留 EXE 旁的六个 WAV 和菜单背景 BMP。
- 改窗口或按键：编译 GUI，运行 `scripts\build_basic.bat window-test`，再在实际 SDL 窗口验证。该测试使用 dummy 驱动向正式事件循环发送事件，不检查物理键盘、桌面焦点、输入法或实际声音。
- 记录编译器、seed、叉车数、输入顺序、预期和实际结果。测试中手动修改状态的场景应明确标注。

## 提交前待办

- 以终端版准备课程提交；如附交 SDL 版，确认附交方式。在课程指定环境验证构建和运行。
- 实际窗口已检查菜单显示、鼠标开始/继续/重开/下一关/退出、键盘移动与暂停、返回菜单、程序化缩放、F11、胜利卡片和 99999 禁入下一关；仍待人工确认物理键盘向菜单种子框输入数字、Tab 切换、鼠标拖拽缩放、失败/碰撞提示的实际显示和正常音频听感。
- 补充终端版人工交互试玩；共享核心断言与终端重定向输入的胜负流程已覆盖，范围见测试记录。
- 核对碰撞掉落包裹、无路可走的叉车、最终配送与致命碰撞等细节是否符合教师说明。
- 准备不超过 1500 词的 PDF 报告、源码、构建运行说明、至少三个可复现场景，以及真实贡献和协助声明。
- 核对课程通知中的截止日期与配套评分表；当前目录未包含评分表。

分工由组内认领，贡献声明按实际工作填写。当前目录包含开发产物，不是最终提交包。
