// Copyright Neofilisoft. All Rights Reserved.
#include "scripting/ScriptingEngine.h"
#include "core/logging/Logger.h"

namespace lacrima::scripting
{
    ScriptingEngine& ScriptingEngine::Get()
    {
        static ScriptingEngine instance;
        return instance;
    }

    bool ScriptingEngine::Initialize()
    {
        if (m_initialized)
        {
            return true;
        }

        LACRIMA_LOG_INFO(LogCategory::Scripting, "ScriptingEngine: Initializing %zu registered module(s)...", m_modules.size());

        bool allSuccess = true;
        for (IScriptModule* module : m_modules)
        {
            if (module)
            {
                if (!module->Initialize())
                {
                    LACRIMA_LOG_WARN(LogCategory::Scripting, "ScriptingEngine: A script module failed to initialize.");
                    allSuccess = false;
                }
            }
        }

        m_initialized = true;
        return allSuccess;
    }

    void ScriptingEngine::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        LACRIMA_LOG_INFO(LogCategory::Scripting, "ScriptingEngine: Shutting down script modules...");
        for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it)
        {
            if (*it)
            {
                (*it)->Shutdown();
            }
        }

        m_modules.clear();
        m_initialized = false;
    }

    void ScriptingEngine::ReloadAll()
    {
        LACRIMA_LOG_INFO(LogCategory::Scripting, "ScriptingEngine: Reloading all script modules...");
        for (IScriptModule* module : m_modules)
        {
            if (module)
            {
                module->Reload();
            }
        }
    }

    void ScriptingEngine::RegisterModule(IScriptModule* module)
    {
        if (!module)
        {
            return;
        }

        for (IScriptModule* existing : m_modules)
        {
            if (existing == module)
            {
                return;
            }
        }

        m_modules.push_back(module);
        if (m_initialized)
        {
            module->Initialize();
        }
    }
}
