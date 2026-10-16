# Deadly Premonition: The Director's Cut — FreeCam & HUD Hide ASI Mod

[![Release](https://img.shields.io/badge/Release-v1.2.1-blue.svg)](#)
[![Game](https://img.shields.io/badge/Game-Deadly%20Premonition%20The%20Director's%20Cut-crimson.svg)](#)
[![Architecture](https://img.shields.io/badge/Architecture-x86%20(32--bit)-purple.svg)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Dear ImGui](https://img.shields.io/badge/UI-Dear%20ImGui%20C%2B%2B-orange.svg)](#)

A standalone, feature-complete **6-DOF Free Camera (FreeCam)** and **HUD / Radar Hide** ASI modification for **Deadly Premonition: The Director's Cut** (PC). Built with **Dear ImGui C++** (Direct3D 9 backend) and zero external runtime dependencies (`-nostdlib` / process heap).

Engineered to operate seamlessly alongside companion fixes like **ZachFix** (`F10`) and **MapFix** (`M`) with zero input conflicts, camera freezes, or render pass corruption.

---

## 📸 Features

- 🎥 **6-DOF Free Flying Camera (`F5`):**
  - Full 360-degree pitch, yaw, and roll-free camera flight.
  - Corrected for Deadly Premonition's mirrored projection matrix (`proj._11 * -1.0f`), guaranteeing natural **`A` = Strafe Left**, **`D` = Strafe Right**, and mouse right = turn right.
  - Multi-tiered flight speeds: Normal (`MoveSpeed`), Fast Boost (`Shift` or Gamepad `L3`, 3x speed), and Precision Slow (`Ctrl` / `Alt`, 0.25x speed).
  - Smooth, unrestricted 360° mouse look with automated screen-center delta tracking and transition snap prevention.
  - Dynamic **XInput Gamepad Support** (Left Stick fly/strafe, Right Stick look, Triggers/Bumpers elevate/descend, L3 turbo boost).
  - Custom Field of View (FOV) slider (30° to 120°).

- 👁️ **2D HUD & Radar Suppression (`F4`):**
  - Instantly toggle all 2D in-game HUD graphics: Health/Stamina bars, mini-map, compass ring, player arrow, digital clock, weather icon, and ammo counters.
  - Suppresses in-vehicle navigation radar (`CARNAVI.XLY`) and cockpit UI.
  - Pure visual suppression that **never** freezes, pauses, or affects the gameplay camera or simulation loop.
  - Full-screen Overworld Map (`M`) and Pause Menu remain 100% accessible and undamaged.

- ⚡ **York Teleportation:**
  - One-click **"Teleport York to Camera"** button in the mod menu.
  - Resolves York's live actor entity via the engine's internal Actor Manager (`0x00BD7670`) and Actor Resolver (`0x006C5FD0`), moving world position coordinates (`+0x58`) and collision bounds (`+0x6C`).
  - One-click **"Snap Camera to York"** button to instantly re-center the FreeCam behind the player.

- 🖥️ **In-Game Dear ImGui Menu (`F9`):**
  - Dark-themed Dear ImGui C++ overlay (`StyleColorsDark`) with 13px ProggyClean typography.
  - 5 organized tabs: **Camera**, **HUD**, **Gamepad**, **Diagnostics**, and **About**.
  - Interactive sliders, checkboxes, tooltips, draggable titlebar with collapse arrow, and bottom-right resize grip.
  - Clean input isolation: while the menu is open, in-game camera motion and key presses are blocked (hold Right Mouse Button to fly while the menu is open).
  - Compatible with ZachFix: Opening ZachFix's menu (`F10`) frees the cursor and never spins the camera.

---

## ⌨️ Default Controls

| Key / Input | Action |
| :--- | :--- |
| **F9** | Toggle FreeCam Settings Menu (Dear ImGui) |
| **F5** | Toggle FreeCam flight mode on / off |
| **F4** | Toggle HUD & Radar visibility on / off |
| **W / S** | Fly Forward / Backward |
| **A / D** | Strafe Left / Right |
| **E / Space** | Elevate Upward (`+Y`) |
| **Q / C** | Descend Downward (`-Y`) |
| **Mouse Move** | 360° Free Look (Yaw / Pitch) |
| **Shift** (Hold) | Speed Boost (3x multiplier) |
| **Ctrl / Alt** (Hold) | Precision Crawl (0.25x multiplier) |
| **Right Mouse** (Hold) | Look / Fly freely while the `F9` settings menu is open |

### 🎮 Gamepad Controls (XInput)

| Control | Action |
| :--- | :--- |
| **Left Thumbstick** | Fly Forward / Backward & Strafe Left / Right |
| **Right Thumbstick** | Rotate Camera (Yaw & Pitch look) |
| **Right Trigger / RB** | Ascend Upward |
| **Left Trigger / LB** | Descend Downward |
| **L3 (Click Left Stick)** | Hold for Speed Boost |

---

## 📦 Installation

1. Make sure you have **Deadly Premonition: The Director's Cut** installed.
2. Ensure you have an ASI Loader installed (e.g. `dinput8.dll` from [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) or included with [ZachFix](https://github.com/Kaldaien/ZachFix)).
3. Copy **`FreeCam.asi`** and **`FreeCam.ini`** into your game's `scripts/` directory:
   ```
   <Deadly Premonition Root>/
   ├── DP.exe
   ├── dinput8.dll
   └── scripts/
       ├── FreeCam.asi
       ├── FreeCam.ini
       ├── ZachFix.asi   (optional, recommended)
       └── MapFix.asi    (optional, recommended)
   ```
4. Launch the game and press **`F9`** to open the menu, or **`F5`** to take flight!

---

## ⚙️ Configuration (`FreeCam.ini`)

All settings are hot-reloadable and configurable in `scripts/FreeCam.ini` or via the **Save to INI** button in the in-game menu:

```ini
[General]
ToggleKey=0x78       ; VK_F9 (Open/close ImGui settings menu)
FreeCamKey=0x74      ; VK_F5 (Direct FreeCam flight toggle)
HudHideKey=0x73      ; VK_F4 (Direct HUD & Radar hide toggle)
FreezePlayer=1       ; Freeze York's physical movement during FreeCam flight

[Camera]
MoveSpeed=15.0       ; Base flight speed (world units / sec)
FastMultiplier=3.0   ; Speed multiplier when holding Shift / L3
SlowMultiplier=0.25  ; Speed multiplier when holding Ctrl / Alt
MouseSensitivity=0.003
InvertPitch=0        ; 0 = Normal, 1 = Inverted vertical look
InvertYaw=0          ; 0 = Normal, 1 = Inverted horizontal look
DefaultFOV=65.0      ; Camera FOV in degrees (0.0 to retain engine FOV)

[HUD]
StartHidden=0        ; 1 = Start game with HUD hidden

[UI]
MenuX=50
MenuY=50
MenuWidth=620
MenuHeight=520
```

---

## 🛠️ Building from Source

### Prerequisites
- Windows 7/8/10/11
- .NET Framework 4.x (for compiling `build_freecam.cs` via built-in `csc.exe`)
- [Zig](https://ziglang.org/) compiler (0.13.x) targeting `x86-windows` (used for Clang C++ frontend and `lld-link` with zero external CRT).

### Build Command
Run the build script from the repository root:
```cmd
build.bat
```
The compiled output will be generated in `FreeCam/build/FreeCam.asi` and automatically deployed to `scripts/FreeCam.asi`.

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
Dear ImGui is licensed under the MIT License, Copyright (c) 2014-2026 Omar Cornut.
