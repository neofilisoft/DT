# Lacrima (Game Engine) 

**Lacrima Engine** (formerly Domestic / DT Engine) is a high-performance, modular 2D/3D game and simulation engine developed by Neofilisoft. Built with modern C++20 and a Vulkan backend, Lacrima is engineered around a cache-friendly Sparse-Set Entity Component System (ECS), state snapshotting, deterministic simulation, and an integrated editor tooling suite.

---

![Screenshot](Screenshot%20(29038).png)

## Key Architectural Highlights

### 1. Rendering Architecture (Vulkan Backend)
- **Vulkan 1.4 Core**: Low-overhead command recording, dynamic rendering, and explicit synchronization.
- **Dual 2D / 3D Pipeline Support**:
  - **2D/Isometric**: High-throughput sprite batching, multi-layer depth sorting, and atlas UV mapping.
  - **3D Mesh Pipeline**: Modern GLTF/GLB loading via `tinygltf` with `assimp` fallback, handle-based `MeshRegistry`, staging buffer GPU uploads, and SPIR-V static/skinned mesh shaders.
- **Lighting & Post-Processing**: Directional lighting, point lights, and cascaded shadow map (CSM) infrastructure.

### 2. High-Performance Sparse-Set ECS
- **Data-Oriented Design**: Component arrays stored contiguously in dense memory blocks (`ComponentArray<Entity, Component>`) with O(1) swap-and-pop removals.
- **Cache-Friendly Iteration**: Systems iterate strictly over dense arrays, maximizing CPU L1/L2 cache hit rates.
- **Modular Subsystems**: Spatial partitioning, autonomy AI, need-decay calculations, and LOD systems that execute with O(1) empty overhead when inactive.

### 3. Simulation World & State Isolation (PIE Ready)
- **Snapshotting & Cloning**: `SimulationWorld` supports deep cloning (`Clone()`, `CopyFrom()`) and full state isolation, enabling non-destructive Play-In-Editor (PIE) testing.
- **Save/Load System**: Robust binary serialization coupled with Zstandard (ZSTD) compression (`SaveGameManager`), supporting incremental slots, metadata validation, and migration handlers.

### 4. Physics & Spatial Navigation
- **Jolt Physics**: Modern 3D multi-threaded physics engine integration (`JoltPhysicsSystem`) with collision layers, rigid bodies, and character virtual controllers.
- **Box2D Integration**: Lightweight 2D physics option for planar simulations.
- **Recast / Detour**: Industry-standard NavMesh generation, polygon mesh queries, and point-to-point pathfinding.

### 5. Memory Management & Core Foundation
- **Custom Memory Allocators**: Specialized Linear, Pool, and FreeList allocators with `MemoryTracker` instrumentation.
- **Job System**: Directed Acyclic Graph (DAG) task scheduler for parallel multi-threaded system updates.
- **Reflection System**: Lightweight runtime type metadata (`TypeInfo`, `REFLECT_FIELD`) driving serialization and dynamic editor inspector properties.
- **Virtual File System (VFS)**: Abstraction layer over engine, project, and user directories with zero hardcoded filesystem paths.

### 6. Scripting & Audio
- **Lua Scripting**: High-performance Lua 5.4 integration via Sol2, featuring sandboxed execution, script verification, and atomic hot-reloading.
- **Audio Architecture**: Multi-bus audio mixing (Master, Music, SFX, Ambient, Voice) powered by MiniAudio.

### 7. Integrated Tooling Suite
- **LacrimaEditor**: ImGui Docking UI featuring 3D/2D Viewport, Scene Outliner, Reflective Property Inspector, Content Browser, Console, and Real-time Profiler.
- **LacrimaCooker**: Command-line asset packaging and optimization tool with Zlib/Zstd compression for production builds.

---

## Directory Structure

```
DT/
├── source/
│   └── engine/
│       ├── asset/          # Asset management and cooking pipelines
│       ├── audio/          # MiniAudio bus management and sound instances
│       ├── core/           # Memory, allocators, math, reflection, logging, VFS, save/load
│       │   └── animation/  # Core skeletal animation data structures and math
│       ├── editor/         # LacrimaEditor ImGui application and tooling panels
│       ├── physics/        # Jolt Physics (3D) and Box2D systems
│       ├── renderer/       # Vulkan renderer, shaders, mesh registry, model loaders
│       ├── runtime/        # Engine entry points, application lifecycle, Entity ID
│       ├── scripting/      # Lua bindings, VM lifecycle, and hot-reload policies
│       └── simulation/     # Sparse-Set ECS, spatial systems, navigation, AI autonomy
├── tests/                  # Unit and integration test suites (Core, Simulation, Physics, Renderer)
├── thirdparty/             # Submodules and dependencies (Jolt, Vulkan, Assimp, tinygltf, etc.)
└── CMakeLists.txt          # Root CMake configuration
```

---

## Prerequisites

- **CMake**: Version 3.24 or higher
- **C++ Compiler**: GCC / Clang / MSVC with C++20 support (MSYS2 UCRT64 recommended on Windows)
- **Build Tool**: Ninja (recommended) or Make
- **Vulkan SDK**: Version 1.3 or 1.4 (with glslc / dxc shader compilers)
- **SDL3**: Installed or provided via thirdparty dependencies

---

## Building the Engine

### 1. Configure Build
From the repository root directory:

```bash
# Configure with Editor and Tests enabled using Ninja
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DLACRIMA_BUILD_EDITOR=ON -DLACRIMA_BUILD_TESTS=ON
```

### 2. Compile
```bash
# Compile using all available CPU threads
cmake --build build --config Debug -j4
```

### 3. Run Test Suites
Lacrima includes extensive unit and integration tests covering core subsystems, simulation, physics, and rendering:

```bash
# Core subsystem tests (Allocators, Reflection, Serialization, VFS, Jobs)
./build/tests/core/lacrima_core_tests.exe

# Simulation tests (Sparse-Set ECS, Autonomy, NavMesh, Save/Load, Genetics)
./build/tests/simulation/lacrima_simulation_tests.exe

# Physics tests (Jolt, Box2D)
./build/tests/physics/lacrima_physics_tests.exe

# Renderer tests (Vulkan pipeline smoke tests)
./build/tests/renderer/lacrima_renderer_tests.exe
```

### 4. Launch Tools

- **Launch Editor**:
  ```bash
  ./build/LacrimaEditor.exe
  ```

- **Run Asset Cooker**:
  ```bash
  ./build/LacrimaCooker.exe <input_asset_path>
  ```

---

## Build Configurations

Lacrima supports four distinct build configurations:
- **Debug**: Full debug symbols, assertion checks, memory tracking, and profiler capture.
- **RelWithDebInfo**: Optimized codegen with debug symbols and profiling instrumentation.
- **Release**: Fully optimized for performance with internal QA profiling hooks enabled.
- **Shipping**: Strips memory tracker instrumentation, profiler capture buffers, and debug reflection names down to hashed IDs for minimal footprint and maximum execution speed.

---

## License & Copyright

Copyright (c) 2026 Neofilisoft. All rights reserved.  
This software is proprietary. See [LICENSE.md](LICENSE.md) for licensing terms.
