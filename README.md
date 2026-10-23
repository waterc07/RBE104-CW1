# Parcel Shift

RBE104TC 仓库配送游戏，提供终端版和 SDL2 像素窗口版。机器人拾取包裹并送回配送点，同时避开叉车。

终端版用于满足课程提交要求，SDL 版继续保留。后续开发要求两端保持相同的游戏功能；输入方式和显示形式可以不同。两端共用 `game.c` 的状态、规则、回合处理、暂停、重开和胜负判断；终端与 SDL 分别负责输入和显示。

![窗口版预览](docs/images/preview.png)

## 编译与运行

以下命令在 Windows PowerShell 中执行，需要 GCC 在 `PATH` 中。

```powershell
cd "D:\Users\Admin\Desktop\Y2S1\RBE104 course work\game"
.\scripts\build_basic.bat
.\build\windows\terminal.exe
```

默认构建与 `basic` 参数相同：编译终端版、预览和测试程序，运行测试，并生成 `build/windows/preview.ppm`、`delivered.ppm`。脚本结束时按任意键返回。

终端版先输入叉车数（1–4），再输入 seed（0–99999）。每次输入一个命令后按回车：W/A/S/D 移动，P 暂停/继续，R 使用原 seed 和叉车数重开，Q 退出；大小写均可。胜负后停留在结算状态，可用 R 重开或 Q 退出。

窗口版：

```powershell
.\scripts\build_basic.bat gui
.\build\windows\parcel_shift.exe 42 2
```

参数依次为 seed 和叉车数，省略时使用 42 和 2。关闭游戏后再重新编译。

| 窗口操作 | 按键 |
| --- | --- |
| 移动一格 | WASD 或方向键，长按不连发 |
| 暂停 / 继续 | 空格 |
| 使用原 seed 重开 | R |
| 退出 | Q、Esc 或关闭窗口 |

GUI 构建需要与 GCC 架构匹配的 MinGW SDL2 开发库。脚本读取 `SDL2_ROOT`，未设置时使用本机路径：

```text
D:\Coding\env\SDL2\SDL2-2.32.10\x86_64-w64-mingw32
```

换电脑后，设置为实际安装目录，例如：

```powershell
$env:SDL2_ROOT = 'C:\Libraries\SDL2\x86_64-w64-mingw32'
.\scripts\build_basic.bat gui
```

该目录应包含 `include/SDL2/SDL.h`、`lib/` 和 `bin/SDL2.dll`。脚本将 DLL 复制到 EXE 旁，并在上级目录存在 `LICENSE.txt` 时复制为 `SDL2-LICENSE.txt`。分发窗口版时保留 DLL 和许可证。窗口版不依赖 SDL_image、SDL_ttf 或外部字体、图片。

## 游戏规则

- 有效移动后，每辆叉车行动一次；撞墙和无效命令不消耗回合。
- 每次携带一个包裹，送回配送点得 100 分，全部送达后获胜。
- 初始三条命，碰撞每回合最多扣一命。机器人和叉车回到各自起点，携带包裹保留。
- 最后配送与致命碰撞同回合发生时判负；叉车无合法方向时等待。
- 相同 seed 和操作在同一 C 库中可复现；不同 C 库的 `rand()` 轨迹可能不同。

## 文件入口

| 路径 | 用途 |
| --- | --- |
| `src/game.c`、`src/game.h` | 终端、窗口、预览和测试共用的地图、状态、规则与游戏操作 |
| `src/scene.c`、`src/scene.h` | 1280×800 像素缓冲区、图标、字体和 HUD |
| `src/window.c` | SDL2 窗口与键盘事件入口 |
| `src/terminal.c` | 终端输入和文本显示入口，调用共享游戏核心 |
| `tests/test_terminal.py` | 终端命令、暂停、重开、胜负后操作和输入校验集成测试 |
| `tests/test_game.c` | 规则断言、绘制调用与 PPM 写入检查 |
| `tools/preview.c` | 不依赖 SDL 的初始画面导出入口 |
| `scripts/build_basic.bat` | Windows 构建入口 |
| [维护与交接](docs/TEAM_HANDOFF.md) | 修改位置、同步要求和提交待办 |
| [测试记录](docs/TEST_RESULTS.md) | 复现命令、结果与未验证项 |

不要把全部 `*.c` 一起编译：窗口、终端、预览和测试分别定义 `main()`。修改规则时只改 `game.c`；两端通过 `processCommand()` 提交游戏操作，通过 `getGameStatus()` 读取胜负状态。退出由各前端处理。

终端集成测试需额外安装 Python 3；基础构建不依赖 Python：

```powershell
python tests/test_terminal.py
```

构建输出统一放在 `build/windows/`。上方 PNG 是保存的预览，构建不会自动更新它。

## 课程提交限制

根目录的[作业 PDF](../RBE104TC_Group_Coursework_Warehouse_Delivery_Robot.pdf)要求 standard C、terminal-based 游戏。提交保留不依赖 SDL 的终端版；如需附交 SDL 窗口版，按教师要求处理。终端版编译需要同时提供 `src/game.c`、`src/game.h` 和 `src/terminal.c`，不能再单独编译 `terminal.c`。显示层中的位运算、像素缓冲区和 SDL 事件需要成员自行理解。

报告最多 1500 词，需附源码、构建运行说明、至少三个可复现场景和真实贡献声明。PDF 中截止日期为待定，实际日期以课程通知为准。本原型使用了 AI 辅助，协助披露按课程政策填写。
