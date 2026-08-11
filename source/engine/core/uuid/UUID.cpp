#include "core/uuid/UUID.h"
#include "core/platform/Assert.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace dt
{
    std::string UUID::ToString() const
    {
        // Standard 8-4-4-4-12 hex grouping.
        char buf[37];
        std::snprintf(buf, sizeof(buf),
            "%08llx-%04llx-%04llx-%04llx-%012llx",
            static_cast<unsigned long long>((m_high >> 32) & 0xFFFFFFFFULL),
            static_cast<unsigned long long>((m_high >> 16) & 0xFFFFULL),
            static_cast<unsigned long long>(m_high & 0xFFFFULL),
            static_cast<unsigned long long>((m_low >> 48) & 0xFFFFULL),
            static_cast<unsigned long long>(m_low & 0xFFFFFFFFFFFFULL));
        return std::string(buf);
    }

    UUID UUID::FromString(const std::string& str)
    {
        // Expects exactly the 8-4-4-4-12 hyphenated form produced by
        // ToString(). Malformed input (wrong length, non-hex chars)
        // produces UUID::Null() rather than throwing - the JSON diagnostic
        // loader (core/serialization) treats a null UUID reference as "this
        // field was corrupted or missing" and logs a warning at the call
        // site with full field-path context, which is more useful than an
        // exception unwinding through the deserialization call stack with
        // no indication of which field was the problem.
        if (str.size() != 36 || str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-')
        {
            return UUID::Null();
        }

        auto hexPart = [&](usize start, usize len) -> u64 {
            char buf[17] = {};
            std::memcpy(buf, str.data() + start, len);
            return std::strtoull(buf, nullptr, 16);
        };

        const u64 p1 = hexPart(0, 8);
        const u64 p2 = hexPart(9, 4);
        const u64 p3 = hexPart(14, 4);
        const u64 p4 = hexPart(19, 4);
        const u64 p5 = hexPart(24, 12);

        UUID id;
        id.m_high = (p1 << 32) | (p2 << 16) | p3;
        id.m_low = (p4 << 48) | p5;
        return id;
    }
}
