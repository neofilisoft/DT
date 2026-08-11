set(DT_SCRIPTING_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(DT_ENGINE_ROOT ${DT_SCRIPTING_ROOT}/..)

file(GLOB_RECURSE DT_SCRIPTING_SOURCES ${DT_SCRIPTING_ROOT}/*.cpp)
file(GLOB_RECURSE DT_SCRIPTING_HEADERS ${DT_SCRIPTING_ROOT}/*.h)

add_library(dt_scripting STATIC ${DT_SCRIPTING_SOURCES} ${DT_SCRIPTING_HEADERS})

target_include_directories(dt_scripting PUBLIC ${DT_ENGINE_ROOT})

# dt_core for FileSystem/Logger; dt_runtime for Entity (ScriptCoroutine
# passes Entity handles through as an opaque Lua usertype - Entity is a
# generic runtime-level identity type, not simulation-specific, so this
# dependency is fine); dt_lua + dt_sol2 for the actual VM.
# ScriptEngine/ScriptCoroutine are the ONLY module allowed to include
# <sol/sol.hpp>, see ScriptEngine.h's file comment on boundary discipline.
# No dependency on dt_simulation - scripting must not know what a
# NeedsComponent or InteractionQueue is (see ScriptStepResult.h's file
# comment for the dependency-direction mistake that was caught and fixed
# during this milestone). simulation/ depends on scripting/, never the
# reverse.
target_link_libraries(dt_scripting PUBLIC dt_core dt_runtime dt_lua dt_sol2)

set_target_properties(dt_scripting PROPERTIES FOLDER "engine")
