// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include <string>
#include <vector>

namespace lacrima::scripting
{
    // Common interface for seamless Lua and C# execution (Flax-like Interop)
    class IScriptModule
    {
    public:
        virtual ~IScriptModule() = default;
        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;
        virtual void Reload() = 0;
        virtual void ExecuteFunction(const std::string& funcName) = 0;
    };

    class ScriptingEngine
    {
    public:
        static ScriptingEngine& Get();
        
        bool Initialize();
        void Shutdown();
        void ReloadAll();
        
        // Interoperability Configuration: Lua isolated by default, cross-calling enabled via specific bridge flags
        bool enableInteroperability = false;

        // Modules will be registered here (e.g., LuaScriptModule, CSharpScriptModule)
        void RegisterModule(IScriptModule* module);

    private:
        std::vector<IScriptModule*> m_modules;
        bool m_initialized = false;
    };
}
