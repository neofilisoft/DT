#include "scripting/ScriptCoroutine.h"
#include "core/logging/Logger.h"

namespace dt::script
{
    std::optional<ScriptCoroutine> ScriptCoroutine::Create(ScriptEngine& engine, const std::string& functionName)
    {
        if (!engine.HasGlobalFunction(functionName))
        {
            DT_LOG_ERROR(LogCategory::Scripting, "ScriptCoroutine::Create: no Lua function named '{}'", functionName);
            return std::nullopt;
        }

        // sol::thread::create spawns a new coroutine thread that SHARES the
        // same global environment as the parent lua_State - this is the correct
        // way to run a Lua function as a resumable coroutine while still having
        // access to the global functions (Rest_Run, GrabASnack_Run, etc.) and
        // bound API tables (dt_engine.*) that were registered on the main state.
        sol::state& lua = engine.Raw();
        sol::thread runner = sol::thread::create(lua);

        // Look up the function from the MAIN state's globals - the thread's
        // state shares the same global table, so this gets the real function.
        sol::function luaFunc = lua[functionName];
        sol::coroutine co(runner.state(), luaFunc);

        return ScriptCoroutine(std::move(runner), std::move(co));
    }

    ScriptStepResult ScriptCoroutine::Step(Entity actor, Entity target, f32 dt)
    {
        if (m_finished)
        {
            // Stepping an already-finished coroutine is a caller bug
            // (InteractionQueue::StepFront should have popped it already),
            // not a Lua content error - report Failed rather than
            // resuming a dead Lua coroutine, which would itself raise a
            // Lua error ("cannot resume dead coroutine") that we'd then
            // have to specially recognize anyway.
            return ScriptStepResult::Failed;
        }

        sol::protected_function_result result = m_coroutine(actor, target, dt);

        if (!result.valid())
        {
            sol::error err = result;
            DT_LOG_ERROR(LogCategory::Scripting, "ScriptCoroutine::Step: Lua error during resume: {}", err.what());
            m_finished = true;
            return ScriptStepResult::Failed;
        }

        // sol::coroutine's own status tells us whether the Lua function
        // yielded (still suspended, more work next Step()) or returned
        // (dead - this call's return value is the interaction's final
        // result, not an intermediate "continue").
        const bool stillSuspended = (m_coroutine.status() == sol::call_status::yielded);

        if (stillSuspended)
        {
            // Intermediate yield - the yielded value is expected to be
            // "continue" by convention (see file header), but any yield at
            // all is treated as Continue regardless of the yielded string,
            // since the coroutine hasn't finished and there's nothing more
            // useful to do than keep stepping it next tick.
            return ScriptStepResult::Continue;
        }

        m_finished = true;

        // Coroutine returned (dead) - its return value determines the
        // final result.
        if (result.return_count() == 0)
        {
            DT_LOG_ERROR(LogCategory::Scripting, "ScriptCoroutine::Step: run-function returned with no result; expected \"complete\" or \"failed\"");
            return ScriptStepResult::Failed;
        }

        const sol::object returnValue = result.get<sol::object>(0);
        if (returnValue.get_type() != sol::type::string)
        {
            DT_LOG_ERROR(LogCategory::Scripting, "ScriptCoroutine::Step: run-function's final return value was not a string");
            return ScriptStepResult::Failed;
        }

        const std::string status = returnValue.as<std::string>();
        if (status == "complete")
        {
            return ScriptStepResult::Complete;
        }
        if (status == "failed")
        {
            return ScriptStepResult::Failed;
        }

        DT_LOG_ERROR(LogCategory::Scripting, "ScriptCoroutine::Step: unrecognized final return value '{}' (expected \"complete\" or \"failed\")", status);
        return ScriptStepResult::Failed;
    }
}
