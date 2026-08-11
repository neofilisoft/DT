set(DT_SIMULATION_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(DT_ENGINE_ROOT ${DT_SIMULATION_ROOT}/..)

file(GLOB_RECURSE DT_SIMULATION_SOURCES ${DT_SIMULATION_ROOT}/*.cpp)
file(GLOB_RECURSE DT_SIMULATION_HEADERS ${DT_SIMULATION_ROOT}/*.h)

add_library(dt_simulation STATIC ${DT_SIMULATION_SOURCES} ${DT_SIMULATION_HEADERS})

target_include_directories(dt_simulation PUBLIC ${DT_ENGINE_ROOT})

# dt_runtime for Entity (Handle<EntityTag>) - Simulation modules key their
# ComponentArray<Entity, T> instances against Runtime's Entity type (see
# Entity.h's own file comment on why Core stays generic and Runtime is
# where the concrete alias lives). dt_scripting for real Lua execution of
# InteractionDef's check/run functions (see scripting/ScriptEngine.h,
# scripting/ScriptCoroutine.h). No dependency on dt_renderer - Simulation
# must never see a renderer header, per the sim/render separation rule.
target_link_libraries(dt_simulation PUBLIC dt_core dt_runtime dt_scripting)

set_target_properties(dt_simulation PROPERTIES FOLDER "engine")
