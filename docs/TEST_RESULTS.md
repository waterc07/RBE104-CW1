# 测试记录

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
