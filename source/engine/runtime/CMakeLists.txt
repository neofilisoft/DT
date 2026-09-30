set(lacrima_RUNTIME_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LACRIMA_ENGINE_ROOT ${lacrima_RUNTIME_ROOT}/..)

file(GLOB_RECURSE lacrima_RUNTIME_SOURCES ${lacrima_RUNTIME_ROOT}/*.cpp)
file(GLOB_RECURSE lacrima_RUNTIME_HEADERS ${lacrima_RUNTIME_ROOT}/*.h)

# Header-only for now (SimulationLoop/RenderLoop/Application are templates
# of control flow, not data, and currently live entirely in headers) - an
# INTERFACE target still lets it declare its include path and dependencies
# properly for anything that consumes it. Will become a real static library
# the moment the first .cpp lands here (e.g. once Entity/Component
# registration grows non-trivial out-of-line logic).
if(lacrima_RUNTIME_SOURCES)
    add_library(lacrima_runtime STATIC ${lacrima_RUNTIME_SOURCES} ${lacrima_RUNTIME_HEADERS})
    target_include_directories(lacrima_runtime PUBLIC ${LACRIMA_ENGINE_ROOT})
    target_link_libraries(lacrima_runtime PUBLIC lacrima_core lacrima_renderer)
else()
    add_library(lacrima_runtime INTERFACE)
    target_sources(lacrima_runtime INTERFACE ${lacrima_RUNTIME_HEADERS})
    target_include_directories(lacrima_runtime INTERFACE ${LACRIMA_ENGINE_ROOT})
    target_link_libraries(lacrima_runtime INTERFACE lacrima_core lacrima_renderer)
endif()

set_target_properties(lacrima_runtime PROPERTIES FOLDER "engine")


