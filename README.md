# AshPaw Client

AshPaw Client is a cross-platform C++ client for a top-down multiplayer animal roleplay game. This repository now includes the foundation, local playable slice, authoritative multiplayer loop, interaction flow, chat, persistence-facing settings, and a developer tooling layer for playtest iteration.

## Current State

- `ashpaw_engine` static library for reusable engine-side systems
- `ashpaw_client` executable for app flow and game wiring
- `ashpaw_tests` for pure-logic and subsystem seam tests
- SDL + OpenGL rendering bootstrap
- Tiled-style JSON map loading seam
- ENet client with the documented binary server contract
- Shared protocol library seam via `ashpaw_protocol`
- Chat, interaction, reconnect persistence, and onboarding/options UI
- Debug rendering toggles for collision, interaction range, and world bounds
- Lightweight install/package support via CPack

## Prerequisites

- CMake 3.24+
- A C++20 compiler
- Git available during CMake configure so `FetchContent` can retrieve dependencies

Platform notes:

- Linux: on Fedora, install OpenGL/X11 development packages before configuring the client:

```bash
sudo dnf install mesa-libGL-devel mesa-libEGL-devel libX11-devel libXext-devel libXcursor-devel libXi-devel libXinerama-devel libXrandr-devel libXrender-devel
```

- macOS: Xcode command line tools are recommended
- Windows: Visual Studio 2022 or a recent MSVC toolchain is recommended

## Configure and Build

```bash
cmake -S . -B build
cmake --build build
```

Run the client:

```bash
./build/ashpaw_client
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

Create a simple package:

```bash
cmake --build build --target package
```

Install locally:

```bash
cmake --install build --prefix ./dist
```

Installed layout:

- `bin/ashpaw_client`
- `share/ashpaw_client/assets`
- `share/ashpaw_client/docs`

## Layout

- `engine/` reusable engine-side subsystems
- `client/` game-specific app flow and wiring
- `assets/` local config and placeholder map content
- `tests/` pure logic and subsystem seam tests
- `docs/` planning and guidance notes

## Playtest Notes

- Start the real AshPaw server that matches the docs in `server-side docs/`
- Start the client with `./build/ashpaw_client`
- Use `WASD` to move, `E` to interact, and `Enter` to chat
- Use `F1` to toggle the developer overlay
- The options panel stores local preferences, while the server remains authoritative for display name and restored position

## Next Steps

- Replace placeholder map visuals with real tile rendering from exported Tiled data
- Introduce texture-backed sprite rendering and stronger visual polish
- Expand packaging and platform-specific release validation for Linux, macOS, and Windows
