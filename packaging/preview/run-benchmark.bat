@echo off
setlocal
rem ANNO 1503 rebuild - frame benchmark (preview build @NAME@)
rem Measures the viewer frame pipeline (GPU and CPU) without a window and
rem writes benchmark-results.md next to this file. Takes 1-3 minutes.

if not "%~1"=="" set "ANNO1503_GAME_DIR=%~1"
if "%ANNO1503_GAME_DIR%"=="" (
  echo Path of your ANNO 1503 install folder, e.g. C:\GOG Games\ANNO 1503
  set /p "ANNO1503_GAME_DIR=Game folder: "
)
if not exist "%ANNO1503_GAME_DIR%\Anlagen.dat" (
  echo Anlagen.dat not found in "%ANNO1503_GAME_DIR%".
  pause
  exit /b 1
)

(echo CPU: %PROCESSOR_IDENTIFIER%, %NUMBER_OF_PROCESSORS% threads& echo.) > "%~dp0benchmark-results.md"
"%~dp0windows\bench_frame.exe" "%ANNO1503_GAME_DIR%" "%ANNO1503_GAME_DIR%\Scenes\0200Single\Single_01.sze" --res 1920x1080,2560x1440,3840x2160 --renderer gpu,cpu --chunks async --check --quick >> "%~dp0benchmark-results.md" 2>&1
echo Done: %~dp0benchmark-results.md
pause
