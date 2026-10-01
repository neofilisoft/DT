set(lacrima_SCRIPTING_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LACRIMA_ENGINE_ROOT ${lacrima_SCRIPTING_ROOT}/..)

file(GLOB_RECURSE lacrima_SCRIPTING_SOURCES ${lacrima_SCRIPTING_ROOT}/*.cpp)
file(GLOB_RECURSE lacrima_SCRIPTING_HEADERS ${lacrima_SCRIPTING_ROOT}/*.h)

add_library(lacrima_scripting STATIC ${lacrima_SCRIPTING_SOURCES} ${lacrima_SCRIPTING_HEADERS})

target_include_directories(lacrima_scripting PUBLIC ${LACRIMA_ENGINE_ROOT})

# lacrima_core for FileSystem/Logger; lacrima_runtime for Entity (ScriptCoroutine
# passes Entity handles through as an opaque Lua usertype - Entity is a
# generic runtime-level identity type, not simulation-specific, so this
# dependency is fine); lacrima_lua + lacrima_sol2 for the actual VM.
# ScriptEngine/ScriptCoroutine are the ONLY module allowed to include
# <sol/sol.hpp>, see ScriptEngine.h's file comment on boundary discipline.
# No dependency on lacrima_simulation - scripting must not know what a
# NeedsComponent or InteractionQueue is (see ScriptStepResult.h's file
# comment for the dependency-direction mistake that was caught and fixed
# ). simulation/ depends on scripting/, never the
# reverse.
target_link_libraries(lacrima_scripting PUBLIC lacrima_core lacrima_runtime lacrima_lua lacrima_sol2)

set_target_properties(lacrima_scripting PROPERTIES FOLDER "engine")


