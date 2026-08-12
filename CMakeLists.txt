cmake_minimum_required(VERSION 3.24)

project(DTEngine LANGUAGES CXX C)

# ---------------------------------------------------------------------------
# Global configuration
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Shipping is a fourth configuration on top of the standard three: it strips
# MemoryTracker instrumentation, the Profiler capture buffers, and all
# REFLECT_* debug-name strings down to hashed IDs only. Debug/RelWithDebInfo
# keep full instrumentation; Release keeps instrumentation but optimizes
# codegen, since internal QA builds still need profiling.
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Shipping" CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS_SHIPPING "${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_EXE_LINKER_FLAGS_SHIPPING "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set(CMAKE_SHARED_LINKER_FLAGS_SHIPPING "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")

option(DT_BUILD_TESTS "Build DTEngine unit/integration tests" ON)
option(DT_BUILD_EDITOR "Build the standalone DTEditor application" ON)

# ---------------------------------------------------------------------------
# Platform detection (compile-time defines consumed by core/platform)
# ---------------------------------------------------------------------------

if(WIN32)
    add_compile_definitions(DT_PLATFORM_WINDOWS=1)
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(DT_PLATFORM_LINUX=1)
else()
    message(FATAL_ERROR "DTEngine: unsupported platform. Windows and Linux are the only supported targets.")
endif()

# Config-specific defines. DT_SHIPPING strips all dev-only instrumentation
# at compile time (see core/config/BuildConfig.h for the consuming code).
add_compile_definitions(
    $<$<CONFIG:Debug>:DT_DEBUG=1>
    $<$<CONFIG:Release>:DT_RELEASE=1>
    $<$<CONFIG:RelWithDebInfo>:DT_RELEASE=1>
    $<$<CONFIG:Shipping>:DT_SHIPPING=1>
)

# ---------------------------------------------------------------------------
# Third party (git submodules, vendored under thirdparty/)
# ---------------------------------------------------------------------------

add_subdirectory(thirdparty)

# ---------------------------------------------------------------------------
# Engine modules
# ---------------------------------------------------------------------------

add_subdirectory(source/engine/core)
add_subdirectory(source/engine/audio)
add_subdirectory(source/engine/renderer)
add_subdirectory(source/engine/runtime)
add_subdirectory(source/engine/scripting)
add_subdirectory(source/engine/simulation)

# ---------------------------------------------------------------------------
# Game
# ---------------------------------------------------------------------------

add_subdirectory(source/game)

# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------
add_executable(DTCooker source/tools/cooker/main.cpp)
target_link_libraries(DTCooker PRIVATE dt_tinygltf dt_stb dt_zlib)
set_target_properties(DTCooker PROPERTIES FOLDER "tools")

if(DT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/core)
    add_subdirectory(tests/simulation)
    add_subdirectory(tests/renderer)
endif()
