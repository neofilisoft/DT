#pragma once

#include "core/memory/MemoryTracker.h"
#include "core/platform/Assert.h"
#include "core/platform/Types.h"

#include <atomic>
#include <cstdlib>

// ---------------------------------------------------------------------------
// PoolAllocator.h
//
// Fixed-size-block allocator over a single contiguous backing array.
// Free blocks are threaded into an intrusive singly-linked freelist stored
// in the unused blocks themselves (no separate bookkeeping array). This is
// the allocator behind Handle<T> slot maps (core/handle) and ECS dense
// component arrays: both need stable-address, fixed-size-element storage
// with O(1) alloc/free and no fragmentation, which a general-purpose
// allocator does not guarantee.
//
// Threading: the freelist head uses a lock-free CAS loop
// (std::atomic<Node*>::compare_exchange_weak). This one *is* built
// lock-free, unlike MemoryTracker, because PoolAllocator sits on the actual
// hot path: ECS component construction happens inside job-graph worker
// tasks running across N threads every simulation tick, so allocate/free
// contention here is real and would show up in a profiler. The ABA problem
// is avoided because freed blocks are never returned to a general heap and
// reused for a different allocator/purpose - a freed block only re-enters
// this same pool's freelist, so a stale CAS operand pointing at a
// "recycled for something else" block cannot occur here.
// ---------------------------------------------------------------------------

namespace dt
{
    class PoolAllocator
    {
    public:
        PoolAllocator(usize blockSize, usize blockCount, MemoryCategory category = MemoryCategory::Core)
            : m_blockSize(blockSize < sizeof(Node) ? sizeof(Node) : blockSize)
            , m_blockCount(blockCount)
            , m_category(category)
        {
            DT_ASSERT(blockSize > 0, "PoolAllocator block size must be nonzero");
            DT_ASSERT(blockCount > 0, "PoolAllocator block count must be nonzero");

            const usize totalBytes = m_blockSize * m_blockCount;
            m_base = static_cast<u8*>(std::malloc(totalBytes));
            DT_ASSERT(m_base != nullptr, "PoolAllocator: failed to allocate backing block");
            DT_TRACK_ALLOC(m_base, totalBytes, m_category);

            // Thread every block into the initial freelist.
            for (usize i = 0; i < m_blockCount; ++i)
            {
                Node* node = reinterpret_cast<Node*>(m_base + i * m_blockSize);
                node->next = (i + 1 < m_blockCount)
                    ? reinterpret_cast<Node*>(m_base + (i + 1) * m_blockSize)
                    : nullptr;
            }
            m_freeListHead.store(reinterpret_cast<Node*>(m_base), std::memory_order_relaxed);
            m_liveCount.store(0, std::memory_order_relaxed);
        }

        ~PoolAllocator()
        {
            // A nonzero live count here means a caller is holding a pointer
            // into this pool past the pool's own lifetime - which will be a
            // use-after-free the moment the backing block is freed below.
            // This assert is the earliest point that bug can be caught.
            DT_ASSERT(m_liveCount.load(std::memory_order_acquire) == 0,
                "PoolAllocator destroyed with live allocations outstanding");

            DT_TRACK_FREE(m_base);
            std::free(m_base);
        }

        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        void* Allocate()
        {
            Node* head = m_freeListHead.load(std::memory_order_acquire);
            while (head != nullptr)
            {
                Node* next = head->next;
                if (m_freeListHead.compare_exchange_weak(
                        head, next, std::memory_order_acq_rel, std::memory_order_acquire))
                {
                    m_liveCount.fetch_add(1, std::memory_order_relaxed);
                    return static_cast<void*>(head);
                }
                // CAS failed: `head` was updated to the current value by the
                // failed compare_exchange_weak itself, loop retries with it.
            }
            // Pool exhausted. Returning nullptr (not asserting) is
            // deliberate: pool exhaustion for a growable system (e.g. ECS
            // component array hitting its initial reservation) is something
            // the caller can handle by growing to a new, larger pool and
            // migrating - see PoolAllocator::Grow pattern used by
            // ComponentArray<T> in core/containers.
            return nullptr;
        }

        void Free(void* ptr)
        {
            if (ptr == nullptr)
            {
                return;
            }

            DT_ASSERT(OwnsPointer(ptr), "PoolAllocator::Free called with pointer not owned by this pool");

            Node* node = static_cast<Node*>(ptr);
            Node* head = m_freeListHead.load(std::memory_order_acquire);
            do
            {
                node->next = head;
            } while (!m_freeListHead.compare_exchange_weak(
                head, node, std::memory_order_acq_rel, std::memory_order_acquire));

            m_liveCount.fetch_sub(1, std::memory_order_relaxed);
        }

        bool OwnsPointer(void* ptr) const
        {
            const u8* p = static_cast<const u8*>(ptr);
            return p >= m_base && p < m_base + (m_blockSize * m_blockCount);
        }

        usize GetBlockSize() const { return m_blockSize; }
        usize GetBlockCount() const { return m_blockCount; }
        usize GetLiveCount() const { return m_liveCount.load(std::memory_order_relaxed); }

    private:
        struct Node
        {
            Node* next;
        };

        u8* m_base = nullptr;
        usize m_blockSize;
        usize m_blockCount;
        std::atomic<Node*> m_freeListHead;
        std::atomic<usize> m_liveCount;
        MemoryCategory m_category;
    };
}
