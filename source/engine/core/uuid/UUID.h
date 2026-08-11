#pragma once

#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"

#include <array>
#include <compare>
#include <random>
#include <string>

// ---------------------------------------------------------------------------
// UUID.h
//
// 128-bit UUID (v4, random), used for any identity that must remain stable
// across a save/load boundary: Sims, Households, Lots, Relationships. This
// is deliberately a different type from Handle<T> (core/handle/Handle.h) -
// see that header's file comment for the full rationale, summarized here:
// Handle<T> is a fast, process-local, generation-checked reference valid
// only within one running session against one SlotMap<T> instance. UUID is
// a slow-to-generate-but-stable identity that survives serialization. The
// save system stores UUIDs; at load time, a new SlotMap<T> is populated and
// a UUID -> Handle<T> lookup table is rebuilt for that session so in-memory
// references resolve back to fast Handle<T> lookups at runtime, with the
// UUID indirection only paid at load time and at explicit
// serialization/cross-reference points (not on the simulation hot path).
// ---------------------------------------------------------------------------

namespace dt
{
    class UUID
    {
    public:
        UUID() = default;

        static UUID Generate()
        {
            // thread_local generators: UUID::Generate() is called from
            // simulation code running across job-graph worker threads
            // (e.g. spawning a new Sim's UUID during a household creation
            // task), and std::mt19937_64 is not thread-safe to share across
            // threads without external synchronization. A thread_local
            // instance avoids both a mutex on every UUID generation and any
            // cross-thread state sharing bugs.
            thread_local std::mt19937_64 engine{ std::random_device{}() };
            thread_local std::uniform_int_distribution<u64> dist;

            UUID id;
            id.m_high = dist(engine);
            id.m_low = dist(engine);

            // Set RFC 4122 version (4) and variant bits so the resulting
            // value is a spec-conformant UUIDv4 - this matters because
            // UUIDs are also exposed in the JSON diagnostic serialization
            // mirror (core/serialization) as plain UUID strings, and
            // external tooling (e.g. a future web-based save inspector)
            // may parse them with a standard UUID library that validates
            // these bits.
            id.m_high = (id.m_high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
            id.m_low  = (id.m_low  & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

            return id;
        }

        static const UUID& Null()
        {
            static const UUID nullId{};
            return nullId;
        }

        bool IsNull() const { return m_high == 0 && m_low == 0; }

        u64 High() const { return m_high; }
        u64 Low() const { return m_low; }

        auto operator<=>(const UUID&) const = default;

        std::string ToString() const;
        static UUID FromString(const std::string& str);

        struct Hasher
        {
            usize operator()(const UUID& id) const noexcept
            {
                // 64-bit mix (splitmix64 finalizer) rather than a plain XOR
                // of high/low: a plain XOR would map any UUID pair that
                // differs only by having high/low swapped to the same
                // hash, and would also cluster poorly for UUIDs generated
                // in the same burst (sequential mt19937_64 draws sharing
                // structure in one half). This finalizer is a well-known
                // strong bit mixer, cheap (a handful of shifts/xors/muls),
                // and eliminates both failure modes.
                u64 x = id.m_high ^ (id.m_low + 0x9E3779B97f4A7C15ULL + (id.m_high << 6) + (id.m_high >> 2));
                x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
                x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
                x = x ^ (x >> 31);
                return static_cast<usize>(x);
            }
        };

    private:
        u64 m_high = 0;
        u64 m_low = 0;
    };

    template <> struct DT_FieldTypeOf<UUID> { static constexpr FieldType value = FieldType::UUID; };
}
