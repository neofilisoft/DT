set(DT_CORE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(DT_ENGINE_ROOT ${DT_CORE_ROOT}/..)

file(GLOB_RECURSE DT_CORE_SOURCES
    ${DT_CORE_ROOT}/*.cpp
)
file(GLOB_RECURSE DT_CORE_HEADERS
    ${DT_CORE_ROOT}/*.h
)

add_library(dt_core STATIC ${DT_CORE_SOURCES} ${DT_CORE_HEADERS})

target_include_directories(dt_core PUBLIC
    ${DT_ENGINE_ROOT}
)

target_compile_features(dt_core PUBLIC cxx_std_20)

# InputManager (core/input/) uses SDL3 headers for SDL_Keycode,
# SDL_GamepadButton, SDL_FingerID etc. Core is the right home for
# input (not renderer) because Simulation must also be able to query
# action state via Lua without depending on the renderer module.
target_link_libraries(dt_core PUBLIC
    dt_lua
    dt_sol2
    dt_sdl3
)

if(DT_PLATFORM_LINUX)
    find_package(Threads REQUIRED)
    target_link_libraries(dt_core PUBLIC Threads::Threads)
endif()

set_target_properties(dt_core PROPERTIES FOLDER "engine")
