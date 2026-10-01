# ---------------------------------------------------------------------------
# lacrima_editor
# The standalone editor library. Links against lacrima_renderer (ImGui/Vulkan),
# lacrima_simulation, and lacrima_imguizmo (transform gizmos).
# Produces a static library that DTEditor.exe links against.
# ---------------------------------------------------------------------------
set(lacrima_EDITOR_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LACRIMA_ENGINE_ROOT ${lacrima_EDITOR_ROOT}/..)

file(GLOB_RECURSE lacrima_EDITOR_SOURCES ${lacrima_EDITOR_ROOT}/*.cpp)
file(GLOB_RECURSE lacrima_EDITOR_HEADERS ${lacrima_EDITOR_ROOT}/*.h)

file(GLOB_RECURSE THIRDPARTY_SOURCES ${lacrima_EDITOR_ROOT}/thirdparty/*.cpp)
file(GLOB_RECURSE THIRDPARTY_HEADERS ${lacrima_EDITOR_ROOT}/thirdparty/*.h)

if(THIRDPARTY_SOURCES)
    list(REMOVE_ITEM lacrima_EDITOR_SOURCES ${THIRDPARTY_SOURCES})
endif()
if(THIRDPARTY_HEADERS)
    list(REMOVE_ITEM lacrima_EDITOR_HEADERS ${THIRDPARTY_HEADERS})
endif()

add_library(lacrima_editor STATIC ${lacrima_EDITOR_SOURCES} ${lacrima_EDITOR_HEADERS})

target_include_directories(lacrima_editor PUBLIC ${LACRIMA_ENGINE_ROOT})
target_include_directories(lacrima_editor PRIVATE thirdparty/json/include)

target_link_libraries(lacrima_editor PUBLIC
    lacrima_renderer
    lacrima_simulation
    lacrima_core
    lacrima_imguizmo
)

if(WIN32)
    target_link_libraries(lacrima_editor PUBLIC comdlg32 shell32)
endif()

set_target_properties(lacrima_editor PROPERTIES FOLDER "engine")



