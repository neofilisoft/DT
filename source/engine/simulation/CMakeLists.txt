set(LACRIMA_SIMULATION_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LACRIMA_ENGINE_ROOT ${LACRIMA_SIMULATION_ROOT}/..)

file(GLOB_RECURSE LACRIMA_SIMULATION_SOURCES ${LACRIMA_SIMULATION_ROOT}/*.cpp)
file(GLOB_RECURSE LACRIMA_SIMULATION_HEADERS ${LACRIMA_SIMULATION_ROOT}/*.h)

add_library(lacrima_simulation STATIC ${LACRIMA_SIMULATION_SOURCES} ${LACRIMA_SIMULATION_HEADERS})

target_include_directories(lacrima_simulation PUBLIC ${LACRIMA_ENGINE_ROOT})

target_link_libraries(lacrima_simulation PUBLIC lacrima_core lacrima_runtime lacrima_scripting lacrima_recast lacrima_lifesim lacrima_physics)

set_target_properties(lacrima_simulation PROPERTIES FOLDER "engine")


