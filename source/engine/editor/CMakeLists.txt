# ---------------------------------------------------------------------------
# dt_editor
# The standalone editor library. Links against dt_renderer (ImGui/Vulkan),
# dt_simulation, and dt_imguizmo (transform gizmos).
# Produces a static library that DTEditor.exe links against.
# ---------------------------------------------------------------------------
set(DT_EDITOR_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(DT_ENGINE_ROOT ${DT_EDITOR_ROOT}/..)

file(GLOB_RECURSE DT_EDITOR_SOURCES ${DT_EDITOR_ROOT}/*.cpp)
file(GLOB_RECURSE DT_EDITOR_HEADERS ${DT_EDITOR_ROOT}/*.h)

add_library(dt_editor STATIC ${DT_EDITOR_SOURCES} ${DT_EDITOR_HEADERS})

target_include_directories(dt_editor PUBLIC ${DT_ENGINE_ROOT})

target_link_libraries(dt_editor PUBLIC
    dt_renderer
    dt_simulation
    dt_core
    dt_imguizmo
)

set_target_properties(dt_editor PROPERTIES FOLDER "engine")
