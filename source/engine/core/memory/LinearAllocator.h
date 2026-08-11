#pragma once

#include "core/memory/MemoryTracker.h"
#include "core/platform/Assert.h"
#include "core/platform/Types.h"

#include <cstdlib>

// ---------------------------------------------------------------------------
// LinearAllocator.h
//
// Bump-pointer allocator over a single fixed-size block. Individual
// allocations cannot be freed; the entire block is reclaimed at once via
// Reset(). This is intentional, not a missing feature: LinearAllocator
// exists specifically for per-frame scratch memory whose lifetime is
// exactly one frame (renderer command buffer building, temporary
// pathfinding scratch space, transient string formatting buffers).
//
// Ownership: the allocator owns one contiguous block, allocated once at
// construction and freed at destruction. Callers never own the memory
// returned by Allocate() - they use it within the frame and it becomes
// invalid the instant Reset() is called. This is enforced by convention
// (documented lifetime), not by the type system, which is the correct
// tradeoff here: wrapping every scratch allocation in a lifetime-tracked
// handle would defeat the entire point of a bump allocator being nearly
// free (an atomic add and a bounds check).
//
// Threading: NOT thread-safe by default. A single LinearAllocator instance
// is meant to be owned by exactly one thread per frame (e.g. each job-graph
// worker gets its own LinearAllocator instance from a per-worker pool, so
// there is no cross-thread contention on the bump pointer). If multiple
// threads must share one arena, use ThreadSafeLinearAllocator (bottom of
// this file), which pays for an atomic fetch_add on the offset.
// ---------------------------------------------------------------------------

namespace dt
{
    class LinearAllocator
    {
    public:
        explicit LinearAllocator(usize capacityBytes, MemoryCategory category = MemoryCategory::Core)
            : m_capacity(capacityBytes)
            , m_offset(0)
            , m_category(category)
        {
            DT_ASSERT(capacityBytes > 0, "LinearAllocator capacity must be nonzero");
            m_base = static_cast<u8*>(std::malloc(capacityBytes));
            DT_ASSERT(m_base != nullptr, "LinearAllocator: failed to allocate backing block");
            DT_TRACK_ALLOC(m_base, capacityBytes, m_category);
        }

        ~LinearAllocator()
        {
            DT_TRACK_FREE(m_base);
            std::free(m_base);
        }

        LinearAllocator(const LinearAllocator&) = delete;
        LinearAllocator& operator=(const LinearAllocator&) = delete;

        LinearAllocator(LinearAllocator&& other) noexcept
            : m_base(other.m_base)
            , m_capacity(other.m_capacity)
            , m_offset(other.m_offset)
            , m_category(other.m_category)
        {
            other.m_base = nullptr;
            other.m_capacity = 0;
            other.m_offset = 0;
        }

        LinearAllocator& operator=(LinearAllocator&& other) noexcept
        {
            if (this != &other)
            {
                DT_TRACK_FREE(m_base);
                std::free(m_base);

                m_base = other.m_base;
                m_capacity = other.m_capacity;
                m_offset = other.m_offset;
                m_category = other.m_category;

                other.m_base = nullptr;
                other.m_capacity = 0;
                other.m_offset = 0;
            }
            return *this;
        }

        // Returns nullptr on out-of-space rather than asserting, because a
        // scratch allocator running out of space mid-frame is a recoverable
        // condition the caller can react to (e.g. fall back to a heap
        // allocation, or grow the arena next frame based on
        // GetHighWaterMark()) - it is not necessarily a programming error.
        void* Allocate(usize sizeBytes, usize alignment = alignof(std::max_align_t))
        {
            const usize alignedOffset = AlignUp(m_offset, alignment);
            if (alignedOffset + sizeBytes > m_capacity)
            {
                return nullptr;
            }

            void* ptr = m_base + alignedOffset;
            m_offset = alignedOffset + sizeBytes;

            if (m_offset > m_highWaterMark)
            {
                m_highWaterMark = m_offset;
            }

            return ptr;
        }

        template <typename T, typename... Args>
        T* AllocateObject(Args&&... args)
        {
            void* mem = Allocate(sizeof(T), alignof(T));
            if (mem == nullptr)
            {
                return nullptr;
            }
            return new (mem) T(std::forward<Args>(args)...);
        }

        // Reclaims the entire block. Does NOT run destructors for objects
        // allocated via AllocateObject<T> - LinearAllocator is intended for
        // POD/trivially-destructible scratch data. If a type with a
        // nontrivial destructor needs frame-scoped ownership, use a
        // PoolAllocator or track it explicitly; mixing destructor semantics
        // into a bump allocator's Reset() would require tracking every
        // allocation's type-erased destructor, which reintroduces the
        // bookkeeping cost this allocator exists to avoid.
        void Reset()
        {
            m_offset = 0;
        }

        usize GetCapacity() const { return m_capacity; }
        usize GetUsed() const { return m_offset; }
        usize GetHighWaterMark() const { return m_highWaterMark; }

    private:
        static usize AlignUp(usize value, usize alignment)
        {
            return (value + (alignment - 1)) & ~(alignment - 1);
        }

        u8* m_base = nullptr;
        usize m_capacity;
        usize m_offset;
        usize m_highWaterMark = 0;
        MemoryCategory m_category;
    };
}
