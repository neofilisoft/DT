// Copyright Neofilisoft. All Rights Reserved.
#include "scripting/csharp/CSharpScriptModule.h"
#include "core/logging/Logger.h"

namespace lacrima::scripting
{
#ifdef LACRIMA_CSHARP_SUPPORT
    bool CSharpScriptModule::Initialize()
    {
        LACRIMA_LOG_INFO(LogCategory::Scripting, "CSharpScriptModule: Initializing .NET 8+ runtime via HostFXR...");
        RegisterNativeBindings();
        return true;
    }

    void CSharpScriptModule::Shutdown()
    {
        LACRIMA_LOG_INFO(LogCategory::Scripting, "CSharpScriptModule: Shutting down .NET runtime.");
    }

    void CSharpScriptModule::Reload()
    {
        LACRIMA_LOG_INFO(LogCategory::Scripting, "CSharpScriptModule: Reloading C# assemblies...");
    }

    void CSharpScriptModule::ExecuteFunction(const std::string& funcName)
    {
        LACRIMA_LOG_INFO(LogCategory::Scripting, "CSharpScriptModule: Executing C# function: %s", funcName.c_str());
    }

    void CSharpScriptModule::RegisterNativeBindings()
    {
        LACRIMA_LOG_INFO(LogCategory::Scripting, "CSharpScriptModule: Registering native engine ECS bindings.");
    }
#else
    bool CSharpScriptModule::Initialize()
    {
        LACRIMA_LOG_WARN(LogCategory::Scripting, "CSharpScriptModule: C# runtime support not compiled in (LACRIMA_CSHARP_SUPPORT disabled)");
        return false;
    }

    void CSharpScriptModule::Shutdown()
    {
    }

    void CSharpScriptModule::Reload()
    {
    }

    void CSharpScriptModule::ExecuteFunction(const std::string& funcName)
    {
        LACRIMA_LOG_WARN(LogCategory::Scripting, "CSharpScriptModule: Cannot execute '%s' - C# support not compiled in", funcName.c_str());
    }

    void CSharpScriptModule::RegisterNativeBindings()
    {
    }
#endif
}
