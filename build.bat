@echo off
setlocal enabledelayedexpansion
REM Local build for AimAssist - no GitHub needed.
REM Requires: Visual Studio 2022 (Desktop development with C++), CMake and Git on PATH.
REM Run from "x64 Native Tools Command Prompt for VS 2022" or any prompt after vcvars64.

cd /d "%~dp0"

if not defined VCPKG_ROOT (
  set "VCPKG_ROOT=%~dp0.vcpkg"
  if not exist "!VCPKG_ROOT!\vcpkg.exe" (
    echo Installing vcpkg into !VCPKG_ROOT! ...
    git clone https://github.com/microsoft/vcpkg "!VCPKG_ROOT!" || exit /b 1
    call "!VCPKG_ROOT!\bootstrap-vcpkg.bat" -disableMetrics || exit /b 1
  )
)

REM Pin the Color-Glass registry baseline to its latest commit
if exist vcpkg-configuration.json.template (
  copy /y vcpkg-configuration.json.template vcpkg-configuration.json >nul
) else (
  copy /y vcpkg-configuration.json vcpkg-configuration.json.template >nul
)
for /f %%h in ('git ls-remote https://gitlab.com/colorglass/vcpkg-colorglass HEAD') do (
  if not defined BASELINE set "BASELINE=%%h"
)
echo Registry baseline: %BASELINE%
powershell -NoProfile -Command "(Get-Content vcpkg-configuration.json) -replace '__BASELINE__','%BASELINE%' | Set-Content vcpkg-configuration.json"

cmake --preset release || exit /b 1
cmake --build --preset release || exit /b 1

for /r build %%f in (AimAssist.dll) do (
  if exist "%%f" copy /y "%%f" package\SKSE\Plugins\AimAssist.dll >nul
)
if not exist package\SKSE\Plugins\AimAssist.dll (
  echo AimAssist.dll not found after build
  exit /b 1
)

powershell -NoProfile -Command "Compress-Archive -Force -Path package\* -DestinationPath AimAssist-1.0.0.zip"
echo.
echo Done: %~dp0AimAssist-1.0.0.zip  (drag this into Vortex)
