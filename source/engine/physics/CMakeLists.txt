set(lacrima_PHYSICS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LACRIMA_ENGINE_ROOT ${lacrima_PHYSICS_ROOT}/..)

file(GLOB_RECURSE lacrima_PHYSICS_SOURCES ${lacrima_PHYSICS_ROOT}/*.cpp)
file(GLOB_RECURSE lacrima_PHYSICS_HEADERS ${lacrima_PHYSICS_ROOT}/*.h)

add_library(lacrima_physics STATIC ${lacrima_PHYSICS_SOURCES} ${lacrima_PHYSICS_HEADERS})

target_include_directories(lacrima_physics PUBLIC 
    ${lacrima_PHYSICS_ROOT}
    ${lacrima_PHYSICS_ROOT}/include
    ${LACRIMA_ENGINE_ROOT}
)

# We include the root of Jolt so we can #include <Jolt/Jolt.h>
target_include_directories(lacrima_physics PRIVATE
    ${LACRIMA_ENGINE_ROOT}/../thirdparty/jolt
)

target_link_libraries(lacrima_physics PUBLIC
    lacrima_core
)

target_link_libraries(lacrima_physics PRIVATE
    Jolt
    box2d
)

set_target_properties(lacrima_physics PROPERTIES FOLDER "engine")


