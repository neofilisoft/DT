#include "scripting/ScriptEngine.h"
#include "core/filesystem/FileSystem.h"
#include "core/logging/Logger.h"

namespace dt::script
{
    ScriptEngine::ScriptEngine()
    {
        OpenStandardLibraries();
    }

    void ScriptEngine::OpenStandardLibraries()
    {
        m_lua.open_libraries(
            sol::lib::base,
            sol::lib::coroutine,   // required - interaction run-functions execute as coroutines, see ScriptCoroutine.h
            sol::lib::string,
            sol::lib::table,
            sol::lib::math,
            sol::lib::utf8
            // Deliberately excluded: sol::lib::os, sol::lib::io (see file
            // header - gameplay scripts must not have raw filesystem/
            // process access), sol::lib::package (no dynamic
            // require()-based module loading from arbitrary paths; content
            // loading goes through FileSystem/ScriptEngine::LoadFile only).
        );
    }

    bool ScriptEngine::LoadFile(const std::string& path)
    {
        auto contents = FileSystem::Get().ReadEntireFile(path);
        if (!contents.has_value())
        {
            DT_LOG_ERROR(LogCategory::Scripting, "ScriptEngine::LoadFile: could not read '{}'", path);
            return false;
        }

        std::string source(reinterpret_cast<const char*>(contents->data()), contents->size());
        return LoadString(source, path);
    }

    bool ScriptEngine::LoadString(const std::string& source, const std::string& chunkName)
    {
        sol::protected_function_result result = m_lua.script(source, sol::script_pass_on_error, chunkName);

        if (!result.valid())
        {
            sol::error err = result;
            DT_LOG_ERROR(LogCategory::Scripting, "ScriptEngine::LoadString failed for chunk '{}': {}", chunkName, err.what());
            return false;
        }

        return true;
    }

    bool ScriptEngine::HasGlobalFunction(const std::string& functionName) const
    {
        sol::object obj = m_lua[functionName];
        return obj.valid() && obj.get_type() == sol::type::function;
    }

    void ScriptEngine::LogCallError(const std::string& functionName, const sol::protected_function_result& result)
    {
        sol::error err = result;
        DT_LOG_ERROR(LogCategory::Scripting, "Lua call to '{}' failed: {}", functionName, err.what());
    }
}
