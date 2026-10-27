@echo off
setlocal
cd /d "%~dp0.."
where gcc >nul 2>nul
if errorlevel 1 (
  echo GCC not found. Install or configure the course C compiler first.
  goto failed
)
if not exist "build\windows" mkdir "build\windows"
if errorlevel 1 goto failed
if "%~1"=="gui" goto gui
if "%~1"=="basic" goto basic
if "%~1"=="" goto basic
echo Usage: scripts\build_basic.bat [basic^|gui]
exit /b 1
:basic
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Isrc src\level.c src\solver.c src\game.c src\terminal.c -o build\windows\terminal.exe
if errorlevel 1 goto failed
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Isrc src\level.c src\solver.c src\game.c src\scene.c tools\preview.c -o build\windows\preview.exe
if errorlevel 1 goto failed
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Isrc src\level.c src\solver.c src\game.c src\scene.c tests\test_game.c -o build\windows\test_game.exe
if errorlevel 1 goto failed
cd /d "build\windows"
test_game.exe
if errorlevel 1 goto failed
preview.exe
if errorlevel 1 goto failed
echo Built successfully. Run build\windows\terminal.exe from the game folder to play.
goto done
:gui
if not defined SDL2_ROOT set "SDL2_ROOT=D:\Coding\env\SDL2\SDL2-2.32.10\x86_64-w64-mingw32"
if not exist "%SDL2_ROOT%\include\SDL2\SDL.h" (
  echo SDL2 headers missing. Set SDL2_ROOT to the matching MinGW SDL2 directory.
  goto failed
)
if not exist "%SDL2_ROOT%\bin\SDL2.dll" (
  echo SDL2.dll missing from SDL2_ROOT\bin.
  goto failed
)
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -I"%SDL2_ROOT%\include\SDL2" src\level.c src\solver.c src\game.c src\scene.c src\window.c -L"%SDL2_ROOT%\lib" -lmingw32 -lSDL2main -lSDL2 -mwindows -o build\windows\parcel_shift.exe
if errorlevel 1 goto failed
copy /y "%SDL2_ROOT%\bin\SDL2.dll" "build\windows\SDL2.dll" >nul
if errorlevel 1 goto failed
if exist "%SDL2_ROOT%\..\LICENSE.txt" (
  copy /y "%SDL2_ROOT%\..\LICENSE.txt" "build\windows\SDL2-LICENSE.txt" >nul
  if errorlevel 1 goto failed
)
echo Built successfully. Run build\windows\parcel_shift.exe 1 2 from the game folder to play.
:done
pause
exit /b 0
:failed
echo Build or tests failed. Save the error output for your group.
pause
exit /b 1
