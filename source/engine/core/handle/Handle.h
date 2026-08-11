#pragma once

#include "core/platform/Assert.h"
#include "core/platform/Types.h"

#include <vector>

// ---------------------------------------------------------------------------
// Handle.h
//
// Generational handle: {index, generation} pair used everywhere the engine
// needs to reference an object without holding a raw pointer to it. This is
// the ownership backbone of the whole engine - see the rationale recap
// below since this type shows up in nearly every other subsystem's public
// API surface (ECS entities, asset references, navigation agents, Editor
// Undo/Redo targets).
//
// Why not shared_ptr: shared_ptr gives you shared ownership + automatic
// lifetime extension, which is precisely the wrong contract for gameplay
// object references. A Sim's "target" reference to another Sim must NOT
// keep that Sim alive - if the target Sim's household is deleted, the
// target must actually be destroyed, and every dangling reference to it
// (other Sims' relationship graphs, active Interaction queues, the
// Navigation reservation system) must detect staleness on next access
// rather than accidentally being kept alive by a stray shared_ptr copy.
// Handle<T> gives you exactly that: the generation counter increments on
// slot reuse, so a stale Handle<T> captured before a Free() is provably
// detectable (IsValid() returns false) without needing weak_ptr's
// lock()-and-check dance at every call site, and without any refcounting
// overhead on the hot path.
//
// Why not a raw index alone: reusing slot indices (which SlotMap<T> does,
// to keep the backing array dense and cache-friendly) means a plain
// integer index becomes ambiguous the moment the original object at that
// index is freed and a new object is allocated into the same slot. The
// generation counter disambiguates "this is the same logical object I
// originally referenced" from "this is a different object that happens to
// occupy the same slot now".
//
// Serialization: Handle<T> is trivially serializable (two u32s), but a raw
// {index, generation} pair is only meaningful within the SlotMap instance
// that produced it and only for the lifetime of one process run - it is
// NOT stable across a save/load boundary, because slot reuse patterns will
// differ between sessions. Cross-session persistent identity uses UUID
// (core/uuid) instead; SlotMap<T>::Rehydrate (see serialization notes in
// SlotMap.h) is the mechanism that reconstructs Handle<T> values that are
// consistent for one loaded session from UUIDs stored in a save file.
// ---------------------------------------------------------------------------

namespace dt
{
    template <typename T>
    struct Handle
    {
        u32 index = kInvalidIndex;
        u32 generation = 0;

        static constexpr u32 kInvalidIndex = 0xFFFFFFFFu;

        constexpr Handle() = default;
        constexpr Handle(u32 inIndex, u32 inGeneration) : index(inIndex), generation(inGeneration) {}

        constexpr bool IsNull() const { return index == kInvalidIndex; }

        constexpr bool operator==(const Handle& other) const
        {
            return index == other.index && generation == other.generation;
        }
        constexpr bool operator!=(const Handle& other) const { return !(*this == other); }

        struct Hasher
        {
            usize operator()(const Handle& h) const noexcept
            {
                return (static_cast<usize>(h.index) << 32) | static_cast<usize>(h.generation);
            }
        };
    };

    // ---------------------------------------------------------------------
    // SlotMap<T>
    //
    // Backing store for Handle<T>. Dense array of T plus a parallel
    // generation array and a freelist of reusable indices. O(1) Create,
    // O(1) Destroy, O(1) validated lookup (Get returns nullptr for a stale
    // handle rather than undefined behavior).
    //
    // Memory layout: `m_generations` and `m_slots` are separate arrays
    // (struct-of-arrays), not interleaved, because IsValid() checks
    // (called far more often than actual data access, e.g. every AI
    // utility-scoring pass validating a target Handle before touching it)
    // only need to touch m_generations - keeping it separate means that
    // hot validation path doesn't drag sizeof(T) worth of unrelated data
    // through cache on every check.
    // ---------------------------------------------------------------------
    template <typename T>
    class SlotMap
    {
    public:
        SlotMap() = default;
        explicit SlotMap(usize reserveCount) { Reserve(reserveCount); }

        void Reserve(usize count)
        {
            m_slots.reserve(count);
            m_generations.reserve(count);
        }

        template <typename... Args>
        Handle<T> Create(Args&&... args)
        {
            if (!m_freeList.empty())
            {
                const u32 index = m_freeList.back();
                m_freeList.pop_back();
                m_slots[index] = T(std::forward<Args>(args)...);
                m_alive[index] = true;
                return Handle<T>(index, m_generations[index]);
            }

            const u32 index = static_cast<u32>(m_slots.size());
            m_slots.emplace_back(std::forward<Args>(args)...);
            m_generations.push_back(0);
            m_alive.push_back(true);
            return Handle<T>(index, 0);
        }

        bool Destroy(Handle<T> handle)
        {
            if (!IsValid(handle))
            {
                return false;
            }

            m_slots[handle.index] = T{};
            m_alive[handle.index] = false;
            // Wrap deliberately, not asserted against: a slot being reused
            // 2^32 times in one process lifetime is not a realistic
            // scenario for this engine's target content scale (a Sims-2
            // scale life sim's household/lot/object counts are nowhere
            // near enough churn to hit 4 billion reuses of a single slot
            // in one session), so wraparound is accepted rather than
            // guarded against with extra branching on every Destroy call.
            m_generations[handle.index] = m_generations[handle.index] + 1;
            m_freeList.push_back(handle.index);
            return true;
        }

        bool IsValid(Handle<T> handle) const
        {
            if (handle.IsNull())
            {
                return false;
            }
            if (handle.index >= m_generations.size())
            {
                return false;
            }
            return m_alive[handle.index] && m_generations[handle.index] == handle.generation;
        }

        T* Get(Handle<T> handle)
        {
            return IsValid(handle) ? &m_slots[handle.index] : nullptr;
        }

        const T* Get(Handle<T> handle) const
        {
            return IsValid(handle) ? &m_slots[handle.index] : nullptr;
        }

        usize LiveCount() const { return m_slots.size() - m_freeList.size(); }
        usize Capacity() const { return m_slots.size(); }

        // Iteration deliberately does NOT skip freed slots for the caller -
        // exposed as raw dense-array iteration for systems (e.g. ECS
        // component array processing) that want to walk every live slot
        // contiguously. Callers needing "only live objects" use
        // ForEachValid below.
        template <typename Func>
        void ForEachValid(Func&& func)
        {
            for (usize i = 0; i < m_slots.size(); ++i)
            {
                if (m_alive[i])
                {
                    const Handle<T> h(static_cast<u32>(i), m_generations[i]);
                    func(h, m_slots[i]);
                }
            }
        }

    private:
        std::vector<T> m_slots;
        std::vector<u32> m_generations;
        std::vector<bool> m_alive;
        std::vector<u32> m_freeList;
    };
}
