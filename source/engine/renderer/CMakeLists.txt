set(LACRIMA_RENDERER_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LACRIMA_ENGINE_ROOT ${LACRIMA_RENDERER_ROOT}/..)

file(GLOB_RECURSE LACRIMA_RENDERER_SOURCES ${LACRIMA_RENDERER_ROOT}/*.cpp)
file(GLOB_RECURSE LACRIMA_RENDERER_HEADERS ${LACRIMA_RENDERER_ROOT}/*.h)

add_library(lacrima_renderer STATIC ${LACRIMA_RENDERER_SOURCES} ${LACRIMA_RENDERER_HEADERS})

target_include_directories(lacrima_renderer PUBLIC ${LACRIMA_ENGINE_ROOT})

# Deliberately depends only on lacrima_core (for Types/Logger/Assert) and pulls
# in runtime/SimulationSnapshot.h as a header dependency via that include
# path - it must NOT depend on lacrima_runtime as a link target, since that
# would make it possible for a renderer backend to accidentally reach
# SimulationLoop/JobSystem/Application. IRenderer.h intentionally only
# includes runtime/SimulationSnapshot.h (pure data), not any other
# runtime header.
target_link_libraries(lacrima_renderer PUBLIC
    lacrima_core
    lacrima_sdl3
    lacrima_vulkan
    lacrima_imgui
    lacrima_imguizmo
    lacrima_zlib
    lacrima_assimp
    lacrima_tinygltf
)

set_target_properties(lacrima_renderer PROPERTIES FOLDER "engine")


