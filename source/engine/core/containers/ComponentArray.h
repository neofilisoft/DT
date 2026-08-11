#pragma once

#include "core/handle/Handle.h"
#include "core/platform/Assert.h"
#include "core/platform/Types.h"

#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// ComponentArray.h
//
// Sparse-set backed dense storage for one component type T, indexed by
// Entity (Handle<EntityTag>, see runtime/Entity.h once Runtime is built).
// This is the concrete data structure behind the "Sparse Set ECS" decision
// from the architecture discussion: a dense, contiguous std::vector<T> for
// cache-friendly iteration when a system processes every live component of
// type T (e.g. Needs decay running over every NeedsComponent every tick),
// plus a sparse map from Entity to dense-array index for O(1) lookup by
// entity, and a "swap-and-pop" removal that keeps the dense array
// contiguous (no holes) at the cost of the last element's dense index
// changing on removal - which is why the sparse map exists, so external
// Handle<T>-based references to a component never store raw dense indices
// that a later swap-and-pop would invalidate.
//
// This is explicitly bounded to where "data-oriented processing provides
// measurable benefit" per your architecture rule (do NOT force ECS
// everywhere) - it is used for the small set of components that get
// iterated at bulk scale every simulation tick across every Sim in a
// household/lot (Needs, Mood, Position, Autonomy state). One-off or
// rarely-iterated per-object data (e.g. a Lot's static metadata, an
// Inventory's item list) stays as plain OOP members on their owning
// object, per your "primary paradigm is OOP, ECS is the optimization
// layer" instruction - it is not run through ComponentArray<T> just
// because it exists.
// ---------------------------------------------------------------------------

namespace dt
{
    // Opaque tag type identifying "an entity" for the purposes of this
    // container - the concrete Entity type (Runtime module) is just
    // Handle<EntityTag> under the hood, so ComponentArray<T> only needs to
    // know it is keying by *some* Handle<X>, not the concrete Runtime
    // Entity type, keeping Core free of a dependency on Runtime.
    template <typename EntityHandleT, typename T>
    class ComponentArray
    {
    public:
        void Reserve(usize count)
        {
            m_dense.reserve(count);
            m_denseToEntity.reserve(count);
        }

        template <typename... Args>
        T& Add(EntityHandleT entity, Args&&... args)
        {
            DT_ASSERT(m_sparse.find(entity) == m_sparse.end(),
                "ComponentArray::Add called for an entity that already has this component");

            const usize denseIndex = m_dense.size();
            m_dense.emplace_back(std::forward<Args>(args)...);
            m_denseToEntity.push_back(entity);
            m_sparse[entity] = denseIndex;

            return m_dense.back();
        }

        void Remove(EntityHandleT entity)
        {
            auto it = m_sparse.find(entity);
            if (it == m_sparse.end())
            {
                return;
            }

            const usize removedIndex = it->second;
            const usize lastIndex = m_dense.size() - 1;

            if (removedIndex != lastIndex)
            {
                // Swap-and-pop: move the last dense element into the
                // removed slot so m_dense stays contiguous with no holes -
                // this is what keeps bulk iteration over every live
                // component a plain linear scan with no "is this slot
                // live" branch per element, unlike SlotMap<T>'s
                // generation-checked iteration (SlotMap prioritizes stable
                // handle validity across arbitrary reuse patterns;
                // ComponentArray prioritizes maximally cache-friendly bulk
                // iteration, since it's the type specifically chosen for
                // the "iterate every Sim's Needs this tick" hot path).
                m_dense[removedIndex] = std::move(m_dense[lastIndex]);
                const EntityHandleT movedEntity = m_denseToEntity[lastIndex];
                m_denseToEntity[removedIndex] = movedEntity;
                m_sparse[movedEntity] = removedIndex;
            }

            m_dense.pop_back();
            m_denseToEntity.pop_back();
            m_sparse.erase(it);
        }

        bool Has(EntityHandleT entity) const
        {
            return m_sparse.find(entity) != m_sparse.end();
        }

        T* Get(EntityHandleT entity)
        {
            auto it = m_sparse.find(entity);
            return (it != m_sparse.end()) ? &m_dense[it->second] : nullptr;
        }

        const T* Get(EntityHandleT entity) const
        {
            auto it = m_sparse.find(entity);
            return (it != m_sparse.end()) ? &m_dense[it->second] : nullptr;
        }

        // Direct contiguous access for bulk-processing systems. Iterating
        // m_dense directly (via these accessors) rather than through
        // per-entity Get() calls is the entire point of this container -
        // e.g. the Needs decay system iterates Data()/Size() directly with
        // a plain for-loop, touching only live NeedsComponent instances
        // packed contiguously in memory, rather than looking up N entities
        // individually through hash maps.
        T* Data() { return m_dense.data(); }
        const T* Data() const { return m_dense.data(); }
        usize Size() const { return m_dense.size(); }

        EntityHandleT EntityAtDenseIndex(usize denseIndex) const { return m_denseToEntity[denseIndex]; }

        template <typename Func>
        void ForEach(Func&& func)
        {
            for (usize i = 0; i < m_dense.size(); ++i)
            {
                func(m_denseToEntity[i], m_dense[i]);
            }
        }

        template <typename Func>
        void ForEach(Func&& func) const
        {
            for (usize i = 0; i < m_dense.size(); ++i)
            {
                func(m_denseToEntity[i], m_dense[i]);
            }
        }

    private:
        std::vector<T> m_dense;
        std::vector<EntityHandleT> m_denseToEntity;
        std::unordered_map<EntityHandleT, usize, typename EntityHandleT::Hasher> m_sparse;
    };
}
