@echo off
setlocal
rem ANNO 1503 rebuild - island viewer (preview build @NAME@)
rem Needs an installed ANNO 1503 (GOG "A.D." 2.0.0.5). The folder must contain Anlagen.dat.

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

set "SCENE=%~2"
if "%SCENE%"=="" set "SCENE=%ANNO1503_GAME_DIR%\Scenes\0200Single\Single_01.sze"

"%~dp0windows\anno1503.exe" --view "%SCENE%" --game-dir "%ANNO1503_GAME_DIR%"
if errorlevel 1 pause
