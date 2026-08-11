#pragma once

#include "core/memory/MemoryTracker.h"
#include "core/platform/Assert.h"
#include "core/platform/Types.h"

#include <cstdlib>
#include <mutex>

// ---------------------------------------------------------------------------
// FreeListAllocator.h
//
// General-purpose variable-size allocator using a first-fit strategy over
// an explicit doubly-linked freelist of blocks, with automatic coalescing
// of adjacent free blocks on Free(). This is the allocator for things that
// are neither per-frame scratch (LinearAllocator) nor fixed-size
// (PoolAllocator): asset blob storage (a texture and an audio clip are
// different sizes), Lua-side userdata backing storage, and any subsystem
// arena where allocation sizes vary and lifetimes are not frame-scoped.
//
// Threading: mutex-guarded. A first-fit freelist walk with coalescing
// cannot be made lock-free without a substantially more complex design
// (e.g. per-size-class lock-free free lists, which is what a production
// general allocator like mimalloc/tcmalloc does internally). Given the
// target team size and that this allocator is explicitly NOT used on the
// per-tick simulation hot path (that's PoolAllocator/ECS territory), a
// mutex is the correct complexity/performance tradeoff here. If profiling
// later shows contention (e.g. concurrent asset streaming from multiple
// job workers hammering one FreeListAllocator instance), the fix is
// per-thread arenas feeding a shared FreeListAllocator only on overflow,
// not rewriting this into a lock-free allocator.
// ---------------------------------------------------------------------------

namespace dt
{
    class FreeListAllocator
    {
    public:
        explicit FreeListAllocator(usize capacityBytes, MemoryCategory category = MemoryCategory::Core)
            : m_capacity(capacityBytes)
            , m_category(category)
        {
            DT_ASSERT(capacityBytes >= sizeof(BlockHeader) * 2, "FreeListAllocator capacity too small");

            m_base = static_cast<u8*>(std::malloc(capacityBytes));
            DT_ASSERT(m_base != nullptr, "FreeListAllocator: failed to allocate backing block");
            DT_TRACK_ALLOC(m_base, capacityBytes, m_category);

            BlockHeader* initial = reinterpret_cast<BlockHeader*>(m_base);
            initial->size = capacityBytes - sizeof(BlockHeader);
            initial->free = true;
            initial->prev = nullptr;
            initial->next = nullptr;
            m_freeListHead = initial;
        }

        ~FreeListAllocator()
        {
            DT_TRACK_FREE(m_base);
            std::free(m_base);
        }

        FreeListAllocator(const FreeListAllocator&) = delete;
        FreeListAllocator& operator=(const FreeListAllocator&) = delete;

        void* Allocate(usize sizeBytes, usize alignment = alignof(std::max_align_t))
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // Round requested size up so the payload region is large enough
            // to hold a BlockHeader when it is later freed and might need
            // to become a freelist node itself.
            usize alignedSize = AlignUp(sizeBytes, alignment);
            if (alignedSize < sizeof(BlockHeader))
            {
                alignedSize = sizeof(BlockHeader);
            }

            BlockHeader* block = m_freeListHead;
            while (block != nullptr)
            {
                if (block->free && block->size >= alignedSize)
                {
                    SplitBlock(block, alignedSize);
                    block->free = false;
                    return BlockToPayload(block);
                }
                block = block->next;
            }

            // Out of space. Callers of subsystem-arena allocators are
            // expected to size their arena from real workload profiling
            // (see Profiler category "Memory/FreeListArena/<name>" high
            // water marks) rather than this allocator silently growing via
            // realloc, which would invalidate every previously returned
            // pointer - unacceptable for an allocator whose whole contract
            // is stable addresses for as long as the caller holds them.
            return nullptr;
        }

        void Free(void* ptr)
        {
            if (ptr == nullptr)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(m_mutex);

            BlockHeader* block = PayloadToBlock(ptr);
            DT_ASSERT(!block->free, "FreeListAllocator::Free called on already-free block (double free)");
            block->free = true;

            CoalesceWithNext(block);
            if (block->prev != nullptr && block->prev->free)
            {
                CoalesceWithNext(block->prev);
            }
        }

        usize GetCapacity() const { return m_capacity; }

    private:
        struct BlockHeader
        {
            usize size;   // payload size, excluding this header
            bool free;
            BlockHeader* prev;
            BlockHeader* next;
        };

        static usize AlignUp(usize value, usize alignment)
        {
            return (value + (alignment - 1)) & ~(alignment - 1);
        }

        static void* BlockToPayload(BlockHeader* block)
        {
            return reinterpret_cast<u8*>(block) + sizeof(BlockHeader);
        }

        static BlockHeader* PayloadToBlock(void* payload)
        {
            return reinterpret_cast<BlockHeader*>(static_cast<u8*>(payload) - sizeof(BlockHeader));
        }

        // Splits `block` so the first `requiredSize` bytes remain in
        // `block` and any remainder large enough to be useful becomes a new
        // free block immediately after it in the list.
        void SplitBlock(BlockHeader* block, usize requiredSize)
        {
            const usize remaining = block->size - requiredSize;
            if (remaining <= sizeof(BlockHeader))
            {
                // Remainder too small to be a usable block on its own;
                // absorb it into this allocation rather than creating a
                // freelist node that could never satisfy a future request.
                return;
            }

            u8* newBlockAddr = reinterpret_cast<u8*>(block) + sizeof(BlockHeader) + requiredSize;
            BlockHeader* newBlock = reinterpret_cast<BlockHeader*>(newBlockAddr);
            newBlock->size = remaining - sizeof(BlockHeader);
            newBlock->free = true;
            newBlock->prev = block;
            newBlock->next = block->next;

            if (block->next != nullptr)
            {
                block->next->prev = newBlock;
            }
            block->next = newBlock;
            block->size = requiredSize;
        }

        void CoalesceWithNext(BlockHeader* block)
        {
            BlockHeader* next = block->next;
            if (next != nullptr && next->free)
            {
                block->size += sizeof(BlockHeader) + next->size;
                block->next = next->next;
                if (next->next != nullptr)
                {
                    next->next->prev = block;
                }
            }
        }

        u8* m_base = nullptr;
        usize m_capacity;
        BlockHeader* m_freeListHead = nullptr;
        std::mutex m_mutex;
        MemoryCategory m_category;
    };
}
