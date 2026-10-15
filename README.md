# Aim Assist (Skyrim SE/AE)

Controller aim assist for Skyrim, inspired by Oblivion Remastered. An SKSE plugin that hooks the right-stick look handler, so mouse and keyboard are never affected.

- **Friction:** camera turns slower while a hostile target is inside the aim cone.
- **Magnetism:** the crosshair is nudged toward the target while you move the stick.
- Defaults: bows, crossbows and spells only, hostile targets only, weapon drawn, line of sight required.

## Requirements
- Skyrim Special Edition, current 1.6.x version
- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (the all-in-one 1.6 version)

## In-game settings (MCM)
Journal → System → Mod Configuration → Aim Assist. Needs [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604) and [MCM Helper](https://www.nexusmods.com/skyrimspecialedition/mods/53000). Changes apply within about a second, no restart. Anything not in the menu (aim height, max distance, invert yaw/pitch) is still in `AimAssist.ini`; MCM values override the INI.

## Build without GitHub
Install Visual Studio 2022 (Desktop development with C++), CMake and Git, then double-click `build.bat` from an x64 Native Tools prompt. It sets up vcpkg, builds, and writes `AimAssist-1.0.0.zip` ready for Vortex.

## Build with GitHub
Push this folder to a GitHub repo. The **Build** workflow compiles on a Windows runner and uploads `AimAssist-Vortex` as an artifact (`AimAssist-1.0.0.zip`). To build locally you need Visual Studio 2022, CMake and vcpkg (`VCPKG_ROOT` set); replace `__BASELINE__` in `vcpkg-configuration.json` with a commit hash of https://gitlab.com/colorglass/vcpkg-colorglass, then run `cmake --preset release` and `cmake --build --preset release`.

## Install with Vortex
1. Download the artifact and unzip it once (GitHub wraps artifacts in a zip).
2. In Vortex, drag `AimAssist-1.0.0.zip` onto the mods page, then install and enable it.
3. Launch through `skse64_loader.exe`.

## Tuning
Edit `Data/SKSE/Plugins/AimAssist.ini` (in Vortex: right-click the mod, open in file manager). Restart the game after changes.

- Pull goes the wrong way: toggle `InvertYaw` / `InvertPitch`.
- Too sticky: lower `Friction Strength`. Too weak: raise `Magnetism Strength`.
- Want it for melee too: `BowsAndSpellsOnly = false`.

Log file: `Documents/My Games/Skyrim Special Edition/SKSE/AimAssist.log`.

## Status
Untested first build. The hook targets `LookHandler::ProcessThumbstick`; if the log doesn't show "Look handler hooked", or the game crashes at launch, send me the log.
