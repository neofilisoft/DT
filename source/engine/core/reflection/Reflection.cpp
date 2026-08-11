#include "core/reflection/Reflection.h"
#include "core/logging/Logger.h"
#include "core/platform/Assert.h"

namespace dt
{
    TypeRegistry& TypeRegistry::Get()
    {
        static TypeRegistry instance;
        return instance;
    }

    void TypeRegistry::Register(const TypeInfo* info)
    {
        // Static-init-order: this runs as part of dt::TypeInfoRegistrar<T>
        // instances constructing at static-init time, potentially across
        // multiple translation units with no defined relative order
        // between TUs (only within a single TU is order defined). Register
        // is intentionally trivial (a vector push_back into a
        // function-local static, i.e. safely lazily constructed regardless
        // of init order - see Get() above using a function-local static
        // instance rather than a namespace-scope global, which sidesteps
        // the C++ static initialization order fiasco entirely).
        for (const TypeInfo* existing : m_types)
        {
            if (existing->nameHash == info->nameHash)
            {
                DT_LOG_ERROR(LogCategory::Reflection,
                    "Duplicate TypeInfo registration for type '{}' (hash collision or duplicate REFLECT_BEGIN)",
                    std::string(info->name));
                return;
            }
        }
        m_types.push_back(info);
    }

    const TypeInfo* TypeRegistry::Find(u64 nameHash) const
    {
        for (const TypeInfo* info : m_types)
        {
            if (info->nameHash == nameHash)
            {
                return info;
            }
        }
        return nullptr;
    }

    const TypeInfo* TypeRegistry::Find(std::string_view name) const
    {
        return Find(FnvHash(name));
    }
}
