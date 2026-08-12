set(DT_RENDERER_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(DT_ENGINE_ROOT ${DT_RENDERER_ROOT}/..)

file(GLOB_RECURSE DT_RENDERER_SOURCES ${DT_RENDERER_ROOT}/*.cpp)
file(GLOB_RECURSE DT_RENDERER_HEADERS ${DT_RENDERER_ROOT}/*.h)

add_library(dt_renderer STATIC ${DT_RENDERER_SOURCES} ${DT_RENDERER_HEADERS})

target_include_directories(dt_renderer PUBLIC ${DT_ENGINE_ROOT})

# Deliberately depends only on dt_core (for Types/Logger/Assert) and pulls
# in runtime/SimulationSnapshot.h as a header dependency via that include
# path - it must NOT depend on dt_runtime as a link target, since that
# would make it possible for a renderer backend to accidentally reach
# SimulationLoop/JobSystem/Application. IRenderer.h intentionally only
# includes runtime/SimulationSnapshot.h (pure data), not any other
# runtime header.
target_link_libraries(dt_renderer PUBLIC
    dt_core
    dt_sdl3
    dt_vulkan
    dt_imgui
    dt_imguizmo
    dt_zlib
)

set_target_properties(dt_renderer PROPERTIES FOLDER "engine")
