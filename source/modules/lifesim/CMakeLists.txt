set(LACRIMA_LIFESIM_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LACRIMA_ENGINE_ROOT ${LACRIMA_LIFESIM_ROOT}/../../engine)

file(GLOB_RECURSE LACRIMA_LIFESIM_SOURCES ${LACRIMA_LIFESIM_ROOT}/*.cpp)
file(GLOB_RECURSE LACRIMA_LIFESIM_HEADERS ${LACRIMA_LIFESIM_ROOT}/*.h)

add_library(lacrima_lifesim STATIC ${LACRIMA_LIFESIM_SOURCES} ${LACRIMA_LIFESIM_HEADERS})

target_include_directories(lacrima_lifesim PUBLIC ${LACRIMA_ENGINE_ROOT})

target_link_libraries(lacrima_lifesim PUBLIC lacrima_simulation)

set_target_properties(lacrima_lifesim PROPERTIES FOLDER "modules")
