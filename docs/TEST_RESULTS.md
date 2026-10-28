# 测试记录

## 2026-10-28 合并第五叉车、开始菜单与胜利庆祝（版本 7）

合并基准：`game/` 的 Git HEAD `6a0705c`，开始时工作区干净。同学包为 `ParcelShift_Celebration/Source`（版本 7）。以当前项目为基准，没有覆盖整个 `Source`，未提交、未推送。

### 冲突判断与合并方式

- 共享规则：同学包的 `game.c`/`level.c`/`solver.c`/`audio.c` 基于当前项目展开注释前的较早版本，且缺少当前项目的碰撞掉包裹、回血概率修正、可变尺寸和求解去重细节。保留当前核心，只移植第五叉车：`MAX_FORKLIFTS=5`、生成器从同一洗牌结果取第 5 个出生点、`initialise()` 在 1–4 时仍验收原四种配置、选 5 时额外验收第五种；终端提示改为 1–5。
- 开始页与菜单：当前设置页由同学完整菜单替代。旧 `drawSetup()`、`tests/test_window.c` 删除，原覆盖迁移到新的菜单、窗口流程与胜利流程测试，没有降低规则覆盖。
- 窗口流程：保留当前碰撞提示、音效触发、纹理重建和缩放实现；移植菜单、后台验证、M/Esc 返回菜单、F11、胜利计时与卡片点击。保留窗口化启动和当前展开式手写风格，不照搬同学的默认全屏与压缩写法。
- 音频与素材：保留当前 `audio.c`，替换为 124 BPM 背景音乐与对应生成脚本，新增菜单背景 BMP/PNG。
- 测试：保留现有规则、掉包、回血、巡逻、结构校验、尺寸边界和 `--audit`/`--route`，新增第五叉车、菜单、庆祝、窗口流程和胜利流程覆盖。

### 构建与自动测试

环境：Windows、MinGW GCC 8.1.0（x86_64-posix-seh-rev0）、SDL2 2.32.10、Python 3.14.6。命令从 `game/` 执行，全部使用 C11 `-Wall -Wextra -Wpedantic -Werror`。

| 命令 | 结果 | 耗时 |
| --- | --- | --- |
| `cmd /c "scripts\build_basic.bat basic <nul"` | 通过；`test_game.exe` 全部 PASS，含六关卡 5 叉车验收/回放与 1-4 地图不变；`test_celebration.exe` PASS；重新生成 preview/delivered PPM | 7.61s |
| `cmd /c "scripts\build_basic.bat gui <nul"` | 通过；链接 `menu.c`，复制 `menu_background.bmp` 与六个 WAV，更新 GUI EXE | 1.4s |
| `cmd /c "scripts\build_basic.bat audio <nul"` | 通过：六 WAV 格式、五种音效输出、BGM 循环边界、并发混音与清理 | 0.67s |
| `cmd /c "scripts\build_basic.bat window-test <nul"` | 通过：`test_menu`、`test_window_flow`、`test_victory_flow` 全 PASS，生成 `menu_preview.ppm` | 12.36s |
| `python tests\test_terminal.py -v` | 8/8 OK：暂停/重开/胜负、1–5 叉车、5 叉车通关、999→1000、99999、回血、输入校验、尺寸与包裹数 | 3.37s |
| `.\build\windows\test_game.exe --audit 250` | 250 行、无失败标记；输出存 `build/windows/audit_tests_final.txt` | 39.37s |

`--audit` 沿用原设计只抽 1–4 辆配置；5 辆的验收与回放由 `test_game`、`test_menu`、`test_terminal.py` 覆盖。

文档更新后，Lead 在最终工作区复跑同一组命令，全部通过：basic 7.05s、audio 0.67s、window-test 12.57s、`tests/test_terminal.py` 3.72s（8/8 OK）。

### 1–4 辆叉车地图不变的实证

在临时目录用 `git archive HEAD` 取出合并前源码，分别构建只输出 `generateLevel()`/`validateLevel()` 结果的对比程序（未加入源码），对全部 99999 个关卡的前 4 个内部 seed 比较地图内容、前四个出生点、尺寸、生成 seed 和结构检查结果。两份输出均为 399996 行，SHA-256 同为 `7316D48EE4C053FCB5427D40CA588FB5BBFFA78763E5304465F730D3B58B7491`，逐字节一致。说明第五出生点没有改变 1–4 的地图，也没有让原本通过结构检查的候选被新检查拒绝。对比程序与输出保存在 `build/merge-check/`（`build/` 已忽略）。

### 五叉车回放

终端用本次构建的验收路线逐关卡回放（方向键逐字符输入）：

| 关卡 | 叉车 | 回合 | 得分 | 结果 |
| --- | --- | --- | --- | --- |
| 1 | 5 | 50 | 300 | YOU WIN |
| 1000 | 5 | 138 | 400 | YOU WIN |
| 10000 | 5 | 126 | 400 | YOU WIN |
| 30000 | 5 | 178 | 500 | YOU WIN |
| 60000 | 5 | 254 | 500 | YOU WIN |
| 99999 | 5 | 236 | 500 | YOU WIN；N 提示 Last level number，不进入 100000 |

`test_menu.c` 对同一组关卡再次用 5 叉车回放并通过；`test_terminal.py` 的 5 叉车用例覆盖暂停、重开、换关和最后关卡。

### 真实 SDL 窗口检查（非 dummy）

环境：Windows 桌面 2560×1440、显示缩放 150%、SDL2 2.32.10。启动 `build/windows/parcel_shift.exe`，用系统截图、`mouse_event` 鼠标点击和向窗口投递 `WM_KEYDOWN/WM_KEYUP` 的按键进行交互。截图保存在 `build/windows/desktop-check/`。

- 首次启动发现 1280×720 窗口被系统放大到约 1920×1080 并部分移出屏幕，原因是进程未声明 DPI 感知。已在 `SDL_Init` 前设置 `SDL_HINT_WINDOWS_DPI_AWARENESS="permonitorv2"`，重建后窗口客户区为 1280×720，菜单完整可见（`d0_menu.png`）。
- 菜单：背景、标题、难度预设、关卡号 1、机器人 2、档位 1 正常显示；`parcel_shift.exe 99999 2` 预填后显示档位 5、17×31、5 个包裹（`e0_menu_99999.png`）。
- 鼠标：点击 START 进入游戏；点击 RESUME 恢复当前局；庆祝卡片上 R 重开、N 下一关、M 主菜单均生效；菜单 EXIT 退出进程。
- 键盘：D 前进一格（`d2_move.png` TURNS 001）、空格暂停/继续（`d3_pause.png` SHIFT PAUSED）、R 重开（TURNS 000）、M/ESC 返回菜单、F11 进入/退出全屏（`d9_fullscreen.png`、`d10_windowed.png`）、Q 退出。
- 菜单期间状态冻结：M 返回菜单后菜单画布为 1280×720，RESUME 后回到同一回合数的游戏画面（`d5_menu_resume.png`、`d6_resume.png`；虚拟流程测试另做逐字段快照比较）。
- 缩放：用 `MoveWindow` 把窗口改小再恢复，画面等比缩放显示（`d7_resized.png`、`d8_restored.png`）；未测试鼠标拖拽边框。
- 胜利：第 1 关、2 辆叉车按 42 步验收路线通关，显示 SHIFT COMPLETE / ALL DELIVERED / SCORE 300 / LIVES 3 与三个按钮（`d11_win.png`）；点击 R 后回到 TURNS 000（`d12_click_restart.png`）；点击 N 进入第 2 关（`f1_next_level.png`）。
- 最后关卡：第 99999 关、2 辆叉车按 230 步路线通关，卡片显示 FINAL LEVEL CLEAR，无 NEXT；按 N 后仍停留在庆祝画面（`e2_final_win.png`、`e3_after_n.png`）；M 返回菜单后 RESUME 可用，EXIT 退出（`e4_menu_from_win.png`）。
- 本次自动注入键盘使用窗口消息，`keybd_event` 注入未被 SDL 接收；因此以上键盘结果证明窗口事件处理与共享规则，不等同于物理键盘、输入法或桌面焦点的人工验收。

### 未验证项

- 真实桌面物理键盘在菜单种子框输入数字（`SDL_TEXTINPUT`）、Tab/Shift+Tab 切换、鼠标拖拽边框缩放、悬停效果。
- 真实窗口中的失败/碰撞提示显示、5 辆叉车的实际窗口试玩、小屏幕工作区缩小分支。
- 实际音频听感（菜单背景音乐、五种音效、胜利音效只播一次、暂停/重开清音效）；离线音频测试只验证数据。
- Linux、ASan/UBSan、课程指定环境；99999 个关卡的 5 叉车全量搜索。
- dummy 驱动测试覆盖事件与状态，不能替代上述桌面项目。

### 工作区

未提交、未推送。源码、测试、素材和文档修改保留在 `game/` 工作区；`build/` 与 `deps/` 仍由 `.gitignore` 排除。真实窗口截图和临时对比程序位于 `build/` 下，不属于源码交付。仓库根目录另放有窗口版 `parcel_shift.exe`、`SDL2.dll` 和 `SDL2-LICENSE.txt` 作为可直接运行副本（连同仓库的 `assets/`）；使用 dummy 驱动从根目录启动 3 秒无菜单背景或音频缺失报错。这些二进制未提交，重新编译后需手动复制更新。

## 2026-10-28 仓库交接与相对路径

- README 命令从仓库根目录执行；SDL 默认目录改为 `deps/SDL2/`，仍允许 `SDL2_ROOT` 覆盖，相对配置以仓库根目录解析。构建从 SDL2 目录或其上级复制许可证；`deps/` 与 `build/` 不纳入 Git。
- 将仓库外的作业 PDF 原样复制为 `docs/assignment.pdf`，SHA-256 与原文件一致；修复 README 链接，纳入 report 素材。更新 LEVELS 中过时的 SDL 默认值和生成失败行为说明。
- 用 Git 跟踪文件的工作区版本及两个新增文档文件创建独立副本，路径位于 `build/handoff-check-*/source checkout/`，不复制原构建输出。为模拟接收方安装依赖，将 SDL2 2.32.10 开发库及许可证放入副本的 `deps/SDL2/`，清除 `SDL2_ROOT` 后检查默认相对路径。
- Windows MinGW GCC 8.1.0：副本的 `basic`、`gui`、`audio`、`window-test` 构建目标通过；核心断言、十个尺寸边界关卡的 1/4 辆叉车获胜回放、七项终端集成测试、离线音频与 SDL dummy 事件测试通过。构建输出包含 SDL2 许可证。
- 本次未进行真实窗口键盘交互、音频试听、250 关抽样、Linux 或 ASan/UBSan。副本检查使用本机 GCC、Python 与已安装 SDK，不能代替同伴电脑或课程环境验收。

## 2026-10-28 代码展开与注释整理

- 展开嵌套三目表达式、同一行的多个语句及部分下标自增写法，为控制分支和循环补齐大括号。补充关卡生成、叉车追踪、快照、搜索堆与回溯、SDL 设置页、PCM16 混音和测试夹具的中文注释。音频生成脚本中的匿名函数改为具名函数；未调整规则版本和接口。
- Windows MinGW GCC 8.1.0：基础构建、GUI 构建、离线音频测试、SDL dummy 事件循环测试均通过 C11 严格编译；`python tests/test_terminal.py` 七项通过。基础构建包含核心断言及十个尺寸边界关卡的 1/4 辆叉车获胜回放。
- 修改前后各运行 `test_game.exe --audit 250`，全部关卡生成及获胜回放通过，输出逐行一致。对比文件为 `build/windows/style_before_audit.txt` 和 `style_after_audit.txt`。
- 修改前后的 `preview.ppm`、`delivered.ppm`、`large.ppm`、`health_natural.ppm`、`collision.ppm` SHA-256 相同；本次未新增实际窗口视觉验收。
- 在 `build/style-audio-before/` 和 `build/style-audio-after/` 分别运行提交版本与整理后的音频脚本，生成的六个 WAV 逐字节一致，未覆盖正式 `assets/`。
- 本次未进行实际键盘交互、音频试听、Linux 对照或 ASan/UBSan；现有窗口交互待办仍保留。

## 2026-10-28 提交前复测

本节对应生成/规则版本 6，含碰撞掉包裹、回血概率修复、SDL 开始页、音频和图标。下方按日期保留历史结果，不代表当前版本已重复完成全部检查。

- Windows MinGW GCC 8.1.0：`cmd /c "scripts\build_basic.bat basic <nul"` 和 `cmd /c "scripts\build_basic.bat gui <nul"` 通过 C11 严格编译，已更新正式终端和 GUI EXE。核心断言及十个尺寸边界关卡的 1/4 辆叉车获胜回放通过。
- `python tests/test_terminal.py` 七项通过。
- `cmd /c "scripts\build_basic.bat audio <nul"` 通过：六个 WAV 格式、五种音效输出、背景循环边界、并发混音和清理。
- `cmd /c "scripts\build_basic.bat window-test <nul"` 通过：空开始页、必填校验、输入编辑、60000 关/4 辆叉车初始化、画布扩展、暂停、重开和退出。
- 本次未进行实际窗口交互或音频试听，未重跑 250 关抽样、Linux 对照或 ASan/UBSan。dummy 驱动事件测试不验证物理键盘、桌面焦点、输入法或音频设备；课程指定环境仍待验证。
- `build/` 保留为本机构建与测试输出，由 `.gitignore` 排除，不纳入源码提交。

## 2026-10-28 SDL 开始设置页

- 无参数打开窗口时先显示开始页，种子和叉车数均为空；输入范围分别为 1–99999 和 1–4。Tab / 上下键切换字段，数字键填写，Backspace / Delete 删除，左右键选择叉车数，Enter 校验后初始化。命令行参数只预填对应字段。玩家机器人固定为一个。
- Windows MinGW GCC 8.1.0、SDL2 2.32.10：`cmd /c "scripts\build_basic.bat basic <nul"`、`cmd /c "scripts\build_basic.bat gui <nul"` 严格编译通过；核心断言、十个尺寸边界关卡回放、`python tests/test_terminal.py` 七项通过。正式 GUI EXE 已更新。
- `cmd /c "scripts\build_basic.bat window-test <nul"` 通过。`tests/test_window.c` 使用 dummy 视频/音频驱动，向正式窗口事件循环发送 SDL 按键事件：空输入、种子 0、缺少叉车数及清空后均未初始化；填写 60000、4 辆叉车后进入 17×31 地图并扩展画布；空格暂停、R 重开保留配置、Esc 正常退出。此测试不验证 Windows 输入传递、焦点或输入法。
- 实际启动无参数 GUI，已查看两个字段均为空的开始页，文字和字段无重叠。桌面自动输入 Return、4、2 及点击输入框后重试均未观察到字段或提示变化，未确认实际键盘交互通过。已按本次启动进程 ID 关闭测试窗口。待人工检查开始页填写、必填提示、进入游戏及正常退出；本次未重新验证音频听感或完整胜负流程。

## 2026-10-28 合并同学交付包的音频与图标

- 从 `ParcelShift_Windows_Delivery_Victory/ParcelShift_Windows/Source` 引入音频模块、六个 WAV、PNG/ICO、图标资源声明、音频生成脚本与离线测试。SDL 前端按移动前后状态触发脚步、拾取、碰撞、配送和胜利音效；共享核心、关卡生成、求解、渲染和终端代码未因本次合并改变。保留合并前工作区已有修改。
- GUI 构建链接 `audio.c`，用 `windres` 嵌入图标，将 WAV 复制到 `build/windows/assets/`；增加 `audio` 目标。README 和交接说明同步记录依赖与分发文件。
- Windows MinGW GCC 8.1.0、SDL2 2.32.10：`cmd /c "scripts\build_basic.bat basic <nul"`、`cmd /c "scripts\build_basic.bat gui <nul"`、`cmd /c "scripts\build_basic.bat audio <nul"` 均通过 C11 严格编译。核心断言、十个尺寸边界关卡的 1/4 辆叉车回放、七项终端集成测试通过；正式终端和 GUI EXE 已更新。
- 离线音频测试通过：六个 WAV 格式、五种事件音效非零输出、背景循环边界、并发混音和清理。此测试不打开声音设备，不验证窗口事件或实际听感。
- 正式 GUI 以默认第 1 关、2 辆叉车启动，已查看初始地图、HUD 和标题栏图标。自动输入 D、空格、方向右键后未观察到回合或暂停画面变化；Esc 和窗口关闭点击也未关闭，最后按本次测试进程路径终止。未据此确认移动、暂停、重开或正常退出通过，未确定自动输入无响应的原因。
- 待人工检查：音频设备实际输出、各事件与声音对应、暂停/重开清音效、缩放、完整胜负和正常退出后的音频停止。本次未重跑 250 关抽样或 Linux 检查。

## 2026-10-27 回血掉落概率与显示修复

- 旧实现直接取关卡号派生的 LCG 第一次输出最高位，导致相邻编号的有/无掉落结果集中；复现第 1–20 关和第 42 关全部不掉落。加入 32 位种子混合后再决定概率和回合；每关仍约 50% 概率、至多一个，重开不重抽。版本升到 6。
- C 断言检查前 20 关出现/不出现两类结果（有掉落数量限定 5–15），前 1000 关有/无掉落数量均在 400–600 内，并验证重开快照一致。正常初始化第 4 关、1 辆叉车，输入 `dadadadadad`，第 11 个有效回合出现道具；未注入道具或回合状态，重开后重复输入得到相同快照。
- 已查看正常掉落导出的 `build/windows/health_natural.png`：地图爱心、`HEALTH: ON MAP` 和底部出现提示可见。两个前端均显示无掉落、待掉落、地图上、已拾取四种状态；碰撞提示优先于当回合的掉落提示。
- Windows C11 严格基础构建及正式 GUI 构建通过；核心断言、十个尺寸边界关卡回放与七项终端集成测试通过。终端新增第 4 关自然掉落及第 1 关无掉落状态检查。正式 `parcel_shift.exe` 已更新；本次未进行 SDL 窗口交互、250 关抽样或 Linux 检查。

## 2026-10-27 碰撞时掉落携带包裹

- 共享核心在重生前把携带包裹放到碰撞格并清空携带状态，终端和 SDL 共用。落点已有包裹时保存重叠数量，逐个拾取；快照恢复和求解去重包含此数量。生成/规则版本升到 5，README、LEVELS 和交接待办已同步。
- Windows GCC 严格基础构建通过。新增手动夹具（关卡编号 1042）覆盖：主动走入叉车、叉车撞上刚拾取包裹的玩家、原地掉落及再次拾取、同格两个包裹分别配送、快照恢复、重开、致命碰撞和空手碰撞。夹具不代表该编号的正常生成地图。
- 现有十个尺寸边界关卡以 1、4 辆叉车回放获胜；初始化仍检查四种叉车数量。六项终端集成测试通过。已检查手动夹具生成的 collision.png，碰撞提示文字位于弹窗内。
- 原 parcel_shift.exe 正在运行，GUI 构建因文件占用而无法覆盖。使用相同 C11 严格编译参数将新版输出为 build/windows/parcel_shift_updated.exe，编译通过；运行中的旧窗口仍使用旧规则。关闭旧窗口后可运行新版，或重新执行 GUI 构建覆盖标准入口。
- 本次未进行 SDL 窗口交互、250 关抽样、Linux 对照或 ASan/UBSan。

## 2026-10-27 按地图尺寸增加包裹

- 生成版本 4：9×15 为 3 个包裹，9×19 和 11×23 为 4 个，13×27 和 17×31 为 5 个。求解段数上限按 `2 * totalPackages` 计算；地图结构检查要求对应数量，得分和胜负继续由共享核心处理。
- Windows MinGW GCC 8.1.0；基础构建和 GUI 严格编译通过，两个正式 EXE 已更新。C 测试检查十个尺寸边界的包裹数、缺少包裹时拒绝、1/4 辆叉车获胜回放、全部配送前不判胜、最终分数为包裹总数乘 100，以及重开恢复。初始化内部仍验证全部四种叉车数量。
- `python tests/test_terminal.py` 六项通过。新增用例在关卡 1、1000、10000、30000、60000、1 辆叉车下回放求解路线，检查初始数量、全数配送后的 300/400/500 分及 R 重开恢复；999→1000 检查新关有 4 个包裹。
- 本次未重跑 250 关抽样、Linux 对照、ASan/UBSan 或 SDL 窗口交互；下方旧版路线、指纹和包裹数量仅属于当时版本。

## 2026-10-27 大地图显示尺寸

- 地图格子固定为 44×44、角色和地图道具为 36×36 逻辑像素。9×15、9×19 使用 1280×800 画布；11×23 为 1456×888，13×27 为 1632×976，17×31 为 1808×1152。右侧面板向右移动，底栏向下移动；弹窗按扩展后的区域居中。
- 基础构建和 GUI 的 C11 严格编译通过；现有 C 断言和十个尺寸边界关卡的路线回放通过。已检查第 1 关初始预览及第 99999 关、4 辆叉车的结算预览，地图、面板和底栏未重叠。
- 正式 SDL 构建以 `parcel_shift.exe 99999 2` 启动，窗口按大地图尺寸扩展。截图显示第 53 回合、3 条命、已配送 1 个且正在携带包裹，完整地图与底栏可见。这些回合由用户操作，未作为自动输入测试结果。检测到用户输入后未继续发送按键。
- 未完成窗口内 N 跨尺寸换关、手动缩放、暂停、重开及完整胜负的自动交互验收；小屏幕工作区的缩小分支尚未实测。游戏规则未修改。

## 2026-10-27 随机回血道具

- Windows MinGW GCC 8.1.0；`cmd /c "scripts\build_basic.bat basic <nul"`、`cmd /c "scripts\build_basic.bat gui <nul"` 严格编译通过，正式终端和 SDL EXE 已更新。`python tests/test_terminal.py` 五项通过，`git diff --check` 无空白错误。
- 新增 C 断言使用手动状态夹具：检查有/无掉落、5–20 回合范围、重开复现、撞墙和暂停不推进掉落、满血保留、回复一命、拾取后不补刷、致命碰撞不回血、快照恢复和无空地时延后生成。编号 1–1000 只检查重开设置，不代表这些关卡已生成或通关。
- 夹具关卡号 1042、叉车数 0，手动设为第 4 回合且第 5 回合掉落，输入 D 后导出 `build/windows/health.ppm`；已查看转换的 `health.png`，爱心和图例可见。此图使用测试状态，不代表正常开局截图。
- 现有十个尺寸边界关卡以 1、4 辆叉车回放获胜；初始化仍检查全部四种数量。版本升到 3，未重跑 250 关抽样、Linux 对照或 ASan/UBSan。
- 本次未进行 SDL 窗口内的拾取、键盘、暂停、重开、缩放和完整胜负交互验收；共享核心断言与渲染预览不替代这些检查。

## 2026-10-27 安全区、追踪与碰撞提示

- Windows MinGW GCC 8.1.0；基础构建、GUI 的 C11 严格编译和五项终端集成测试通过。
- C 断言覆盖配送点附近禁止叉车进入、按路径距离发现、六回合追踪、四回合冷却、返回安全区停止追踪、直行巡逻、无路等待、AI 快照恢复和重开清零。旧的“叉车撞入 D”夹具已改为验证安全配送；生命耗尽优先级仍单独断言。
- 十个尺寸边界关卡分别以 1、4 辆叉车回放获胜；初始化内部仍检查全部四种数量。250 关确定性抽样全部生成并回放获胜，输出为 `build/windows/traffic_audit_windows.txt`。本次未重跑 Linux 对照或 ASan/UBSan。
- 已检查初始、安全区、追踪图标和碰撞提示的渲染输出。`chasing.ppm`、`collision.ppm` 使用手动设置状态的夹具；不代表正常游戏截图。
- 实际 SDL 正式构建：第 1 关、2 辆叉车，以方向键执行 `wwsaaawwdwwwaawadadswadssaaswwdwssswdwdwssawwaswawdsd`（字母表示对应方向），第 33 个有效回合碰撞，生命从 3 降到 2，中央提示可见；提示消失后 Down 成功推进至第 34 回合。
- 本次自动注入 WASD 与空格未产生预期响应，未据此确认这些按键通过。SDL 提示期间输入屏蔽、精确停留时长、重开、缩放和完整胜负尚未完成窗口交互验收。终端提示等待下一条输入，不使用计时暂停。
- 生成/规则版本升到 2，旧版本路线和指纹不再作为当前基线。下方记录保留各自版本的结果。

## 2026-10-27 默认第 1 关与源码注释

- SDL 无关卡参数及无 SDL 预览使用 `DEFAULT_LEVEL = 1`；叉车默认数量仍为 2。终端关卡输入为空行时使用 1，EOF 仍退出。
- 补充生成、BFS、快照、候选验收、搜索堆、哈希去重、路线回溯、输入处理和显示缩放的中文注释；未调整生成算法与移动规则。
- 基础构建、GUI 严格编译、现有 C 测试和五项终端集成测试通过；数值输入测试包含空行进入第 1 关。已重新生成默认关卡预览。
- 本次未进行 SDL 实际窗口交互检查；上一节的 250 关跨平台抽样未重复执行。

## 2026-10-27 确定性关卡与可变尺寸

环境：Windows MinGW GCC 8.1.0、Python 3.14；WSL Ubuntu-24.04 GCC，Linux 检查启用 ASan/UBSan。

```powershell
cmd /c "scripts\build_basic.bat basic <nul"
cmd /c "scripts\build_basic.bat gui <nul"
python tests/test_terminal.py
.\build\windows\test_game.exe --audit 250
```

- 基础构建及 GUI 均使用 C11、`-Wall -Wextra -Wpedantic -Werror`。
- C 测试覆盖既有配送、携带限制、碰撞扣命、暂停、重开和胜负优先级；原固定地图改为显式测试夹具，不再代表关卡 42。
- 结构检查反例覆盖开放外墙、非法字符、尺寸错误、包裹数量错误、出生重叠和孤立包裹；无路可走时求解失败并恢复原状态。
- 关卡 1、999、1000、9999、10000、29999、30000、59999、60000、99999 均生成成功。检查尺寸，分别回放 1 和 4 辆叉车的完整获胜路线；初始化内部另验证全部四种数量。
- 关卡 10000、29999、30000、60000 使用第一次内部重试；相同关卡在不同叉车数量下的地图和生成 seed 相同。
- 求解前后完整状态不变；重开恢复地图、对象、生命、包裹、回合和随机状态；切换其他关卡后返回也得到相同初始状态。
- 五项终端集成测试通过：包含大小写、超长行、多命令、非法数值、配置各阶段 EOF、暂停、重开、完整胜负、四种叉车数通关、999→1000 换尺寸和最后关卡编号。终端失败路线为 `random.Random(42)` 从 `wasd` 生成 5000 个命令，关卡 42、4 辆叉车。
- 固定抽样 250 个关卡全部生成并回放获胜；Windows 与 Linux 输出的地图/出生点/路线指纹、生成 seed、获胜回合和生命逐行一致。Linux C 测试与抽样均通过 ASan/UBSan。抽样规则见 [LEVELS.md](LEVELS.md)，输出为 `build/windows/level_audit_windows.txt`、`level_audit_linux.txt`。
- 已查看 9×15 初始画面和 17×31 结算画面；地图、图标、状态及操作栏位于预定区域。README 图片已更新。
- 新 SDL 构建实际启动并显示关卡 42、2 辆叉车、9×15 地图。期间检测到用户操作，自动输入已停止；本次暂停、重开、缩放、完整胜负及 N 换关的窗口交互验收未完成，不能由终端测试代替。

未穷举全部 99999 个关卡，未验证内存分配失败及耗尽全部候选的前端流程；搜索上限失败只表示未确认可通关。课程指定环境仍需单独验证。以下为历史记录，旧固定地图路线和 `rand()` 结果不适用于生成版本 1。

## 2026-10-26 SDL 输入法拦截按键

原版窗口 seed 42、2 辆叉车，自动输入 S、D 后角色与回合数不变。临时事件日志收到 `SDL_TEXTEDITING`，未收到对应 `SDL_KEYDOWN`；界面出现输入法组合文本。

`window.c` 在窗口创建后及收到 `SDL_WINDOWEVENT_FOCUS_GAINED` 时调用 `SDL_StopTextInput()`。游戏不需要文本输入，移动仍通过 `processCommand()` 处理。

- MinGW GCC 8.1.0，`cmd /c "scripts\build_basic.bat gui <nul"` 严格编译通过，已更新 `build/windows/parcel_shift.exe`。
- 正式构建 seed 42、2 辆叉车，自动输入 Down 后机器人从 `(1,1)` 移到 `(2,1)`，回合显示 001。用户在新窗口手动按 S、D 后确认角色可以移动。
- 基础构建和六组 C 测试通过；`python tests/test_terminal.py` 四项通过。
- 本次未完成 SDL 暂停、重开、缩放和完整胜负流程检查。

## 2026-10-23 共享核心重构后

环境：Windows、MinGW GCC 8.1.0（x86_64-posix-seh-rev0）、Python 3.14.6。以下命令从 `game/` 执行；`<nul` 用于跳过批处理末尾的按键等待。

```powershell
cmd /c "scripts\build_basic.bat basic <nul"
cmd /c "scripts\build_basic.bat gui <nul"
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Isrc src/game.c src/scene.c tests/test_game.c -o build/windows/test_game.exe
Push-Location build/windows
.\test_game.exe
Pop-Location
python tests/test_terminal.py
```

结果：基础构建、SDL 严格编译、独立严格测试编译通过；C 测试六组输出通过，终端集成测试四项通过。终端构建现在链接 `game.c`，不链接 SDL。基础构建不要求 Python；终端集成测试单独运行。

| 检查 | 结果 |
| --- | --- |
| 原有规则断言 | 配送、撞墙与无效输入、碰撞重生、叉车撞入机器人、单包裹限制通过 |
| 暂停和继续 | 暂停时 `processCommand` 和直接 `takeTurn` 都拒绝移动；继续后执行配送路线，地图、机器人、叉车、回合、得分和后续随机数与无暂停路线一致 |
| 原 seed 重开 | 游戏中、暂停中、胜利后、失败后均可重开；重置得分、生命、包裹、回合和暂停，保留 seed 与叉车数 |
| 胜负保护 | 结束后移动和暂停不改变结算状态；最终配送同回合被叉车致命撞击时判负。C 测试通过直接设置状态构造这两个边界 |
| 终端输入 | 大写命令、P 暂停/继续、R 重开、Q 退出、多命令行、超长行、数值越界和 EOF 通过 |
| 终端完整胜利 | seed 42、1 辆叉车，Python `random.Random(42)` 连续从 `wasd` 选 3000 个命令；第 1843 个命令进入胜利，1044 回合、300 分、剩余 1 条命；后续移动不改变结果，R 可重开 |
| 终端完整失败 | 同一输入生成方式、seed 42、4 辆叉车；第 792 个命令进入失败，452 回合、100 分、生命为 0；后续移动和 P 不改变结果，R 可重开 |
| 渲染 | 生成初始与配送 PPM；将初始 PPM 转为 `build/windows/preview_check.png` 后查看，地图、HUD、操作栏显示正常。未替换 README 保存的 PNG |
| SDL 实际窗口 | 新构建启动并显示 seed 42、2 辆叉车初始画面，检查后关闭；自动输入空格、字母和方向键时未观察到画面状态变化，按键流程未确认通过 |

胜负路线依赖本次 C 库的 `rand()` 序列，不能据此保证其他 C 库得到相同回合数。终端完整流程使用重定向输入，不等同于人工试玩。最初的失败用例使用 `da` 往返，未触发碰撞，已改为上述固定生成方式的输入；未为此修改游戏规则。

仍待检查：SDL 窗口移动、暂停、重开、缩放、完整胜负和结算后重开的实际交互；终端人工试玩；课程指定环境运行。下面是重构前的历史记录。

## 2026-10-23 文档整理时复测

环境：Windows PowerShell、MinGW GCC；全部命令在 `game/` 下执行。未修改 C 源码、头文件或构建脚本。

```powershell
.\scripts\build_basic.bat basic
.\scripts\build_basic.bat gui
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Isrc src/game.c src/scene.c tests/test_game.c -o build/windows/test_game.exe
Push-Location .\build\windows
.\test_game.exe
Pop-Location
```

结果：基础构建、GUI 严格编译、独立严格测试编译均通过；现有五组断言通过。基础构建生成 `build/windows/preview.ppm` 与 `delivered.ppm`。

| 测试场景 | 设置与操作 | 实际结果 |
| --- | --- | --- |
| 配送 | seed 42、1 辆叉车，执行 `sssdawww` | 100 分、已配送 1、3 条命、未携带；绘制并保存 PPM 成功 |
| 撞墙与无效命令 | 初始化后执行 `w`、`x` | 返回 0，回合数保持 0 |
| 碰撞与重生 | 每次将叉车放在 `(1,2)`，执行 `d`，重复三次 | 生命依次为 2、1、0，机器人回到 `(1,1)` |
| 叉车撞入机器人 | 将叉车放在 `(1,3)`、将 `(1,4)` 改为墙，执行 `d` | 剩余 2 条命，机器人回到起点列 |
| 单包裹限制 | 设置已携带、机器人位于 `(4,1)`，执行 `d` | 仍携带一个包裹，`(4,2)` 的包裹未被移除 |

坐标为数组的零起始行、列。后三个场景直接设置测试状态，不是完整试玩路线。绘制函数执行和图片写入成功，不代表 HUD 内容及布局已逐项验收。

## 终端入口复测

```powershell
@('1','42','w','x','s','s','s','d','a','w','w','w','q') | .\build\windows\terminal.exe
```

结果：撞墙 `w`、无效输入 `x` 后回合保持 0；配送后输出 `Score: 100 | Lives: 3 | Remaining: 2 | Turns: 8`；Q 退出输出 `Quit. Score: 100 | Delivered: 1/3`，退出码为 0。这是重定向输入检查，未覆盖完整交互试玩。

## 先前窗口检查与未验证项

整理前记录：2026-10-23 原生 SDL2 窗口启动并显示 seed 42、2 辆叉车的初始画面。用户按 Esc 中止后续输入检查。本次仅重新编译 GUI，未重新启动窗口。

仍未完成：窗口移动、暂停、重开、缩放、完整通关与失败；终端版完整通关和输入边界；课程指定环境运行。自动测试中的三次扣命不等于窗口失败流程已通过。

SDL2 开发库和 DLL 已安装，旧日志中“需要 SDL 开发头文件和桌面”的提示不能用来判断本机依赖是否缺失。窗口行为需另行实测。
