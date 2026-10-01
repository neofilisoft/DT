// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/math/Math.h"
#include "core/math/Frustum.h"
#include "runtime/Entity.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>

namespace lacrima::sim
{
    class SimulationWorld;

    enum class ChunkState
    {
        Unloaded,
        Loading,
        Loaded,
        Unloading
    };

    struct ChunkPos
    {
        i32 x = 0;
        i32 z = 0;

        bool operator==(const ChunkPos& other) const { return x == other.x && z == other.z; }
    };

    struct ChunkHasher
    {
        std::size_t operator()(const ChunkPos& p) const
        {
            return std::hash<i32>()(p.x) ^ (std::hash<i32>()(p.z) << 1);
        }
    };

    struct ChunkData
    {
        ChunkPos pos;
        AABB bounds;
        ChunkState state = ChunkState::Unloaded;
        std::vector<Entity> entities;
    };

    // 3D World Partitioning and Chunk Streaming Subsystem.
    // Dynamically manages streaming chunks (64m x 64m) around the active viewer
    // using asynchronous background jobs (JobSystem) and provides fast Frustum Culling.
    class WorldStreamer
    {
    public:
        WorldStreamer() = default;
        ~WorldStreamer();

        void Init(f32 chunkSize = 64.0f, f32 streamingRadius = 128.0f);
        void Shutdown();

        void SetViewerPosition(const Vec3& pos);
        const Vec3& GetViewerPosition() const { return m_viewerPosition; }

        f32 GetChunkSize() const { return m_chunkSize; }
        f32 GetStreamingRadius() const { return m_streamingRadius; }
        void SetStreamingRadius(f32 radius) { m_streamingRadius = radius; }

        ChunkPos GetChunkPos(f32 x, f32 z) const;
        AABB GetChunkBounds(const ChunkPos& pos, f32 minY = -100.0f, f32 maxY = 500.0f) const;

        bool IsChunkLoaded(const ChunkPos& pos) const;
        ChunkState GetChunkState(const ChunkPos& pos) const;

        usize GetLoadedChunkCount() const;
        usize GetLoadingChunkCount() const;

        const std::unordered_map<ChunkPos, ChunkData, ChunkHasher>& GetChunks() const { return m_chunks; }

        // Ticks streaming logic: evaluates chunk distance to viewer, enqueues background
        // load jobs via JobSystem, checks completed jobs, and unloads far chunks.
        void Step(SimulationWorld& world);

        // Queries all currently loaded chunks whose AABB intersects the given camera Frustum.
        void GetVisibleChunks(const Frustum& frustum, std::vector<ChunkPos>& outVisible) const;

        // Blocks until all pending background chunk jobs are completed (useful for test sync or shutdown).
        void WaitAllPendingJobs();

    private:
        f32 m_chunkSize = 64.0f;
        f32 m_streamingRadius = 128.0f;
        Vec3 m_viewerPosition{ 0.0f, 0.0f, 0.0f };

        std::unordered_map<ChunkPos, ChunkData, ChunkHasher> m_chunks;

        // Thread-safe queue of completed chunk loads reported by worker threads
        mutable std::mutex m_completedMutex;
        std::vector<ChunkPos> m_completedLoads;

        std::atomic<i32> m_pendingJobCount{ 0 };
    };
}
