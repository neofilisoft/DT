// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "scripting/ScriptingEngine.h"

namespace lacrima::scripting
{
    // Interop layer for .NET 8+ (HostFXR Native Hosting)
    class CSharpScriptModule : public IScriptModule
    {
    public:
        bool Initialize() override;
        void Shutdown() override;
        void Reload() override;
        void ExecuteFunction(const std::string& funcName) override;
        
        // Called from C# to access native Engine ECS
        static void RegisterNativeBindings();
    };
}

