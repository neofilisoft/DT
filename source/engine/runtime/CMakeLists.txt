set(DT_RUNTIME_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(DT_ENGINE_ROOT ${DT_RUNTIME_ROOT}/..)

file(GLOB_RECURSE DT_RUNTIME_SOURCES ${DT_RUNTIME_ROOT}/*.cpp)
file(GLOB_RECURSE DT_RUNTIME_HEADERS ${DT_RUNTIME_ROOT}/*.h)

# Header-only for now (SimulationLoop/RenderLoop/Application are templates
# of control flow, not data, and currently live entirely in headers) - an
# INTERFACE target still lets it declare its include path and dependencies
# properly for anything that consumes it. Will become a real static library
# the moment the first .cpp lands here (e.g. once Entity/Component
# registration grows non-trivial out-of-line logic).
if(DT_RUNTIME_SOURCES)
    add_library(dt_runtime STATIC ${DT_RUNTIME_SOURCES} ${DT_RUNTIME_HEADERS})
    target_include_directories(dt_runtime PUBLIC ${DT_ENGINE_ROOT})
    target_link_libraries(dt_runtime PUBLIC dt_core dt_renderer)
else()
    add_library(dt_runtime INTERFACE)
    target_sources(dt_runtime INTERFACE ${DT_RUNTIME_HEADERS})
    target_include_directories(dt_runtime INTERFACE ${DT_ENGINE_ROOT})
    target_link_libraries(dt_runtime INTERFACE dt_core dt_renderer)
endif()

set_target_properties(dt_runtime PROPERTIES FOLDER "engine")
