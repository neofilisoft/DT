set(LACRIMA_CORE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LACRIMA_ENGINE_ROOT ${LACRIMA_CORE_ROOT}/..)

file(GLOB_RECURSE LACRIMA_CORE_SOURCES
    ${LACRIMA_CORE_ROOT}/*.cpp
)
file(GLOB_RECURSE LACRIMA_CORE_HEADERS
    ${LACRIMA_CORE_ROOT}/*.h
)

add_library(lacrima_core STATIC ${LACRIMA_CORE_SOURCES} ${LACRIMA_CORE_HEADERS})

target_include_directories(lacrima_core PUBLIC
    ${LACRIMA_ENGINE_ROOT}
)

target_compile_features(lacrima_core PUBLIC cxx_std_20)

# InputManager (core/input/) uses SDL3 headers for SDL_Keycode,
# SDL_GamepadButton, SDL_FingerID etc. Core is the right home for
# input (not renderer) because Simulation must also be able to query
# action state via Lua without depending on the renderer module.
target_link_libraries(lacrima_core PUBLIC
    lacrima_lua
    lacrima_sol2
    lacrima_sdl3
    lacrima_xxhash
    lacrima_zstd
    lacrima_zlib
)

if(lacrima_PLATFORM_LINUX)
    find_package(Threads REQUIRED)
    target_link_libraries(lacrima_core PUBLIC Threads::Threads)
endif()

set_target_properties(lacrima_core PROPERTIES FOLDER "engine")


