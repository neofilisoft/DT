#pragma once

#include "core/platform/Types.h"

#include <sol/sol.hpp>

#include <optional>
#include <string>

// ---------------------------------------------------------------------------
// ScriptEngine.h
//
// Owns one Lua VM (sol2::state) and is the ONLY place in the engine that
// includes <sol/sol.hpp> or touches a sol:: type in its public API. Every
// other module (simulation/, game/) that needs Lua interop goes through
// ScriptEngine's or ScriptCoroutine's wrapper API instead of touching sol2
// directly - this mirrors the same boundary discipline as
// "Simulation must never include a Vulkan header": sol2 is an
// implementation detail of how DTEngine talks to Lua, not something
// gameplay/simulation code should need to know exists. If the scripting
// backend ever changed (unlikely, but the principle matters), only this
// module's internals would need to change.
//
// Threading: a single Lua VM (lua_State) is NOT thread-safe for concurrent
// calls - Lua's own C API requires all calls into one lua_State to be
// serialized. Per the architecture's TaskGraph design, interaction
// check/run functions are invoked from SimulationWorld's Autonomy and
// InteractionResolve nodes; both nodes still execute on a single worker
// at a time per entity (TaskGraph nodes run in parallel with each other,
// but the per-entity ForEachValid loop inside one node is presently
// single-threaded C++ iteration, not itself parallelized across entities -
// see SimulationWorld.cpp). This means all Lua calls in the current
// pipeline are already serialized by construction. If per-entity work
// inside a node is ever parallelized in a future milestone, ScriptEngine
// will need either a lock around Lua calls or (better, avoiding lock
// contention) one lua_State per worker thread - that is an explicit,
// documented future concern, not silently assumed safe here.
// ---------------------------------------------------------------------------

namespace dt::script
{
    class ScriptEngine
    {
    public:
        ScriptEngine();

        // Opens the standard Lua libraries this engine actually wants
        // exposed to gameplay scripts. Deliberately NOT open_libraries(all)
        // - `os` and `io` give a Lua script direct filesystem/process
        // access, which gameplay/interaction scripts (untrusted relative to
        // engine code, potentially content-mod-authored) must not have.
        // Everything gameplay content needs (string, table, math) is
        // opened; filesystem access for content loading goes through
        // FileSystem (core/filesystem), never raw Lua io.
        void OpenStandardLibraries();

        bool LoadFile(const std::string& path);
        bool LoadString(const std::string& source, const std::string& chunkName);

        // Calls a global Lua function by name with the given arguments,
        // returning std::nullopt if the function doesn't exist or the call
        // raised a Lua error (logged via DT_LOG_ERROR at the call site, see
        // .cpp - callers get a clean optional rather than needing to
        // understand sol2's error-reporting types).
        template <typename ReturnT, typename... Args>
        std::optional<ReturnT> CallGlobalFunction(const std::string& functionName, Args&&... args)
        {
            sol::protected_function func = m_lua[functionName];
            if (!func.valid())
            {
                return std::nullopt;
            }

            sol::protected_function_result result = func(std::forward<Args>(args)...);
            if (!result.valid())
            {
                LogCallError(functionName, result);
                return std::nullopt;
            }

            return result.get<ReturnT>();
        }

        bool HasGlobalFunction(const std::string& functionName) const;

        // Escape hatch for binding registration (usertypes, free
        // functions) - used by modules that own domain data (e.g.
        // SimulationWorld registering Entity/need accessors) to bind their
        // own types into this VM. Kept as a single named accessor rather
        // than a public sol::state member so every binding-registration
        // call site is visibly opting into direct sol2 usage, not
        // accidentally leaking sol2 types through an implicit conversion.
        sol::state& Raw() { return m_lua; }

    private:
        void LogCallError(const std::string& functionName, const sol::protected_function_result& result);

        sol::state m_lua;
    };
}
