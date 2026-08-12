# DT Engine

DT Engine (Domaintic Engine) is an in-house proprietary game engine developed by Neofilisoft. It is designed to be highly performant, utilizing a Sparse-Set Entity Component System (ECS) alongside modern graphics APIs (Vulkan) for simulation-heavy games.

## Key Features
- **Vulkan Rendering:** High-performance rendering backend.
- **Sparse-Set ECS:** Data-oriented design for cache-friendly bulk processing.
- **Recast/Detour Navigation:** Robust pathfinding and NavMesh generation.
- **Lua Scripting:** Flexible interaction and gameplay scripting.
- **Asset Cooker:** Integrated asset pipeline with Zlib compression (`DTCooker`).
- **ImGui Editor:** A lightweight, built-in editor for scene inspection and profiling (`DTEditor`).

## Building the Engine

### Prerequisites
- CMake (3.20+)
- MSYS2 (UCRT64 environment) / GCC
- Vulkan SDK
- SDL3

### Build Instructions
From the root directory, you can configure and build the project using CMake:

```bash
# Generate build files (Ensure Editor is enabled)
cmake -S . -B build -DDT_BUILD_EDITOR=ON

# Build the project (use -j to speed up compilation)
cmake --build build --config Debug -j4
```
*(Alternatively, you can use the provided `dtbuild.bat` script at the root directory).*

## Tools

### DTEditor
The standalone engine editor. It provides a Scene Outliner, Property Inspector, Content Browser, and Log Console for inspecting the simulation world and tweaking properties.
**Run:** `build/DTEditor.exe`

### DTCooker
The command-line asset cooking tool. It converts raw assets (like `.glb`, `.png`) into optimized, Zlib-compressed `.asset` files for the engine to load quickly.
**Run:** `build/DTCooker.exe <input_file>`

## License
This software is proprietary. See [LICENSE.md](LICENSE.md) for the End-User License Agreement.

---
2026 Copyright (c) Neofilisoft. All rights reserved.
