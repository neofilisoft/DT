# ---------------------------------------------------------------------------
# Third party dependencies.
#
# Policy: everything here is vendored as source, never a system-installed
# package. This guarantees every developer builds against byte-identical
# dependency versions with zero package manager setup.
# `git submodule update --init --recursive` (or xcopy from Desktop/ThirdParty)
# is the entire setup step.
#
# Layout expected under thirdparty/:
#   thirdparty/lua/        -> Lua 5.4.8+ source
#   thirdparty/sol2/       -> ThePhD/sol2 (header-only)
#   thirdparty/sdl/        -> libsdl-org/SDL, branch release-3.x
#   thirdparty/imgui/      -> ocornut/imgui, docking branch
#   thirdparty/imguizmo/   -> CedricGuillemet/ImGuizmo
# ---------------------------------------------------------------------------

set(DT_THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Lua 5.4.8+
# Built as a plain C static library. Compiled as C, not C++, and isolated in
# its own translation units so Lua's setjmp/longjmp-based error handling
# never interacts with C++ exception unwinding across the boundary. All
# engine-side error handling from Lua goes through sol2's protected_function
# call wrappers, which convert Lua errors into C++ exceptions at the
# boundary in a controlled, single location.
# ---------------------------------------------------------------------------
if(EXISTS ${DT_THIRDPARTY_DIR}/lua/CMakeLists.txt)
    add_subdirectory(lua)
else()
    message(WARNING
        "thirdparty/lua submodule not present. Run: "
        "git submodule update --init thirdparty/lua"
    )
endif()

# ---------------------------------------------------------------------------
# sol2 (header-only)
# ---------------------------------------------------------------------------
if(EXISTS ${DT_THIRDPARTY_DIR}/sol2/include/sol/sol.hpp)
    add_library(dt_sol2 INTERFACE)
    target_include_directories(dt_sol2 INTERFACE ${DT_THIRDPARTY_DIR}/sol2/include)
    target_compile_features(dt_sol2 INTERFACE cxx_std_20)
    # sol2 exceptions must be enabled; this is how Lua runtime errors surface
    # to C++ call sites (e.g. a bad Lua interaction script indexing nil).
    target_compile_definitions(dt_sol2 INTERFACE
        SOL_ALL_SAFETIES_ON=1
        SOL_USING_CXX_LUA=0
    )
else()
    message(WARNING
        "thirdparty/sol2 submodule not present. Run: "
        "git submodule update --init thirdparty/sol2"
    )
endif()

# ---------------------------------------------------------------------------
# SDL3
# Provides: platform window, Vulkan surface creation, event pump, input.
# SDL3's own CMakeLists creates the SDL3::SDL3 target. We wrap it with an
# alias dt_sdl3 to stay consistent with this project's dt_* naming.
# ---------------------------------------------------------------------------
if(EXISTS ${DT_THIRDPARTY_DIR}/sdl/CMakeLists.txt)
    # Disable SDL subsystems the engine does not use, keeping build times
    # and binary size minimal.
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON  CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF  CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    # Audio/Haptic/Joystick/Sensor are owned by engine subsystems
    # (miniaudio for audio). Disable SDL's own implementations.
    set(SDL_AUDIO OFF CACHE BOOL "" FORCE)

    add_subdirectory(sdl EXCLUDE_FROM_ALL)

    # Alias so engine code links dt_sdl3, not SDL3::SDL3 directly.
    add_library(dt_sdl3 INTERFACE)
    target_link_libraries(dt_sdl3 INTERFACE SDL3::SDL3-static)
    set_target_properties(SDL3-static PROPERTIES FOLDER "thirdparty/SDL3")
else()
    message(WARNING
        "thirdparty/sdl not present. "
        "Copy Desktop/ThirdParty/SDL3 -> thirdparty/sdl"
    )
    # Stub so dependent targets still parse even if SDL3 is absent.
    add_library(dt_sdl3 INTERFACE)
endif()

# ---------------------------------------------------------------------------
# Vulkan (system-installed Vulkan SDK - the one exception to the
# "vendored source only" policy because Vulkan loaders are inherently
# platform/driver infrastructure, not user-space source code we can vendor).
# Requires Vulkan SDK installed (https://vulkan.lunarg.com/).
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED)

add_library(dt_vulkan INTERFACE)
target_link_libraries(dt_vulkan INTERFACE Vulkan::Vulkan)

# ---------------------------------------------------------------------------
# Dear ImGui
# Builds the core ImGui library plus the SDL3 platform backend and the
# Vulkan renderer backend. Only these two backends are compiled - the
# backends/ directory contains backends for many other platforms/APIs that
# we simply do not include.
# ---------------------------------------------------------------------------
if(EXISTS ${DT_THIRDPARTY_DIR}/imgui/imgui.h)
    set(IMGUI_DIR ${DT_THIRDPARTY_DIR}/imgui)

    add_library(dt_imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        # SDL3 platform backend
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        # Vulkan renderer backend
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    )

    target_include_directories(dt_imgui PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )

    target_link_libraries(dt_imgui PUBLIC
        dt_sdl3
        dt_vulkan
    )

    target_compile_features(dt_imgui PUBLIC cxx_std_17)

    # ImGui's Vulkan backend requires this to use dynamic loader
    # (avoids needing to pass function pointers manually).
    target_compile_definitions(dt_imgui PUBLIC
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES=0
    )

    set_target_properties(dt_imgui PROPERTIES FOLDER "thirdparty/ImGui")
else()
    message(WARNING
        "thirdparty/imgui not present. "
        "Copy Desktop/ThirdParty/ImGui -> thirdparty/imgui"
    )
    add_library(dt_imgui INTERFACE)
endif()

# ---------------------------------------------------------------------------
# ImGuizmo
# Header + source widget library layered on top of ImGui. Used by the
# editor (Phase 5) for transform gizmos in the scene/lot editor.
# Included here (not in editor/) because it may also be useful in
# in-game debug overlays that render before the full editor is built.
# ---------------------------------------------------------------------------
if(EXISTS ${DT_THIRDPARTY_DIR}/imguizmo/ImGuizmo.h)
    set(IMGUIZMO_DIR ${DT_THIRDPARTY_DIR}/imguizmo)

    add_library(dt_imguizmo STATIC
        ${IMGUIZMO_DIR}/ImGuizmo.cpp
    )

    target_include_directories(dt_imguizmo PUBLIC ${IMGUIZMO_DIR})
    target_link_libraries(dt_imguizmo PUBLIC dt_imgui)
    target_compile_features(dt_imguizmo PUBLIC cxx_std_17)
    set_target_properties(dt_imguizmo PROPERTIES FOLDER "thirdparty/ImGuizmo")
else()
    message(WARNING
        "thirdparty/imguizmo not present. "
        "Copy Desktop/ThirdParty/ImGuizmo -> thirdparty/imguizmo"
    )
    add_library(dt_imguizmo INTERFACE)
endif()

# ---------------------------------------------------------------------------
# stb (header-only)
# ---------------------------------------------------------------------------
if(EXISTS ${DT_THIRDPARTY_DIR}/stb/stb_image.h)
    add_library(dt_stb INTERFACE)
    target_include_directories(dt_stb INTERFACE ${DT_THIRDPARTY_DIR})
else()
    add_library(dt_stb INTERFACE)
endif()

# ---------------------------------------------------------------------------
# tinygltf (header-only)
# ---------------------------------------------------------------------------
if(EXISTS ${DT_THIRDPARTY_DIR}/tinygltf/tiny_gltf.h)
    add_library(dt_tinygltf INTERFACE)
    target_include_directories(dt_tinygltf INTERFACE ${DT_THIRDPARTY_DIR} ${DT_THIRDPARTY_DIR}/stb)
else()
    add_library(dt_tinygltf INTERFACE)
endif()

# ---------------------------------------------------------------------------
# miniaudio
# ---------------------------------------------------------------------------
if(EXISTS ${DT_THIRDPARTY_DIR}/miniaudio/miniaudio.h)
    add_library(dt_miniaudio STATIC
        ${DT_THIRDPARTY_DIR}/miniaudio/miniaudio.c
    )
    target_include_directories(dt_miniaudio PUBLIC ${DT_THIRDPARTY_DIR}/miniaudio)
else()
    add_library(dt_miniaudio INTERFACE)
endif()

