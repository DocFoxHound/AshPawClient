# AshPaw Client

AshPaw Client is a cross-platform C++ client for a top-down multiplayer animal roleplay game. This repository currently implements the project foundation plus an initial local playable slice: startup, configuration, logging, SDL/OpenGL bootstrap, a test map, one controllable player entity, camera follow, collision, and an ImGui developer overlay.

## Current State

- `ashpaw_engine` static library for reusable engine-side systems
- `ashpaw_client` executable for app flow and game wiring
- `ashpaw_tests` for pure-logic and subsystem seam tests
- `ashpaw_handshake_server` for local Phase 3 connection/handshake testing
- SDL + OpenGL rendering bootstrap
- Tiled-style JSON map loading seam
- ENet client connection seam
- Shared protocol library seam via `ashpaw_protocol`

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

If you want to build only the handshake responder while OpenGL packages are still missing:

```bash
cmake -S . -B build-server -DASHPAW_BUILD_CLIENT=OFF
cmake --build build-server --target ashpaw_handshake_server
```

Run the client:

```bash
./build/ashpaw_client
```

Run the minimal handshake responder:

```bash
./build/ashpaw_handshake_server
```

Optional flags:

```bash
./build/ashpaw_handshake_server --host 0.0.0.0 --port 7777 --reserved-name taken
```

The temporary Phase 3 handshake contract is:

- client sends `join:<player_name>`
- server replies with `join_accepted`
- or server replies with `join_rejected:<reason>`

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

## Layout

- `engine/` reusable engine-side subsystems
- `client/` game-specific app flow and wiring
- `assets/` local config and placeholder map content
- `tests/` pure logic and subsystem seam tests
- `docs/` planning and guidance notes

## Next Steps

- Replace placeholder map visuals with real tile rendering from exported Tiled data
- Introduce texture-backed sprite rendering
- Wire the shared protocol library once the server repo exposes it
- Move from local-only playback into handshake and authoritative multiplayer flow
