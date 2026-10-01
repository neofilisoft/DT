// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/world/WorldStreamer.h"
#include "simulation/world/SimulationWorld.h"
#include "core/jobs/Async.h"
#include "core/logging/Logger.h"

#include <cmath>
#include <algorithm>
#include <chrono>

namespace lacrima::sim
{
    WorldStreamer::~WorldStreamer()
    {
        Shutdown();
    }

    void WorldStreamer::Init(f32 chunkSize, f32 streamingRadius)
    {
        m_chunkSize = (chunkSize > 1.0f) ? chunkSize : 64.0f;
        m_streamingRadius = (streamingRadius > m_chunkSize) ? streamingRadius : 128.0f;
        m_chunks.clear();
        m_completedLoads.clear();
        m_pendingJobCount.store(0);

        LACRIMA_LOG_INFO(lacrima::LogCategory::Core,
            "WorldStreamer: Initialized with chunk size %.1fm, radius %.1fm",
            m_chunkSize, m_streamingRadius);
    }

    void WorldStreamer::Shutdown()
    {
        WaitAllPendingJobs();

        {
            std::lock_guard<std::mutex> lock(m_completedMutex);
            m_completedLoads.clear();
        }
        m_chunks.clear();
    }

    void WorldStreamer::SetViewerPosition(const Vec3& pos)
    {
        m_viewerPosition = pos;
    }

    ChunkPos WorldStreamer::GetChunkPos(f32 x, f32 z) const
    {
        return {
            static_cast<i32>(std::floor(x / m_chunkSize)),
            static_cast<i32>(std::floor(z / m_chunkSize))
        };
    }

    AABB WorldStreamer::GetChunkBounds(const ChunkPos& pos, f32 minY, f32 maxY) const
    {
        f32 minX = static_cast<f32>(pos.x) * m_chunkSize;
        f32 maxX = minX + m_chunkSize;
        f32 minZ = static_cast<f32>(pos.z) * m_chunkSize;
        f32 maxZ = minZ + m_chunkSize;

        return AABB(Vec3(minX, minY, minZ), Vec3(maxX, maxY, maxZ));
    }

    bool WorldStreamer::IsChunkLoaded(const ChunkPos& pos) const
    {
        auto it = m_chunks.find(pos);
        return (it != m_chunks.end() && it->second.state == ChunkState::Loaded);
    }

    ChunkState WorldStreamer::GetChunkState(const ChunkPos& pos) const
    {
        auto it = m_chunks.find(pos);
        if (it != m_chunks.end())
        {
            return it->second.state;
        }
        return ChunkState::Unloaded;
    }

    usize WorldStreamer::GetLoadedChunkCount() const
    {
        usize count = 0;
        for (const auto& [pos, data] : m_chunks)
        {
            if (data.state == ChunkState::Loaded)
            {
                count++;
            }
        }
        return count;
    }

    usize WorldStreamer::GetLoadingChunkCount() const
    {
        usize count = 0;
        for (const auto& [pos, data] : m_chunks)
        {
            if (data.state == ChunkState::Loading)
            {
                count++;
            }
        }
        return count;
    }

    void WorldStreamer::Step(SimulationWorld& world)
    {
        // 1. Process completed async background load jobs on main thread
        std::vector<ChunkPos> completed;
        {
            std::lock_guard<std::mutex> lock(m_completedMutex);
            completed.swap(m_completedLoads);
        }

        for (const auto& pos : completed)
        {
            auto it = m_chunks.find(pos);
            if (it != m_chunks.end() && it->second.state == ChunkState::Loading)
            {
                it->second.state = ChunkState::Loaded;
                LACRIMA_LOG_TRACE(lacrima::LogCategory::Core,
                    "WorldStreamer: Chunk (%d, %d) finished loading into world", pos.x, pos.z);
            }
        }

        // 2. Identify chunks that need to be loaded around viewer
        const ChunkPos centerChunk = GetChunkPos(m_viewerPosition.x, m_viewerPosition.z);
        const i32 chunkRadius = static_cast<i32>(std::ceil(m_streamingRadius / m_chunkSize));
        const f32 radiusSq = m_streamingRadius * m_streamingRadius;

        for (i32 dx = -chunkRadius; dx <= chunkRadius; ++dx)
        {
            for (i32 dz = -chunkRadius; dz <= chunkRadius; ++dz)
            {
                ChunkPos targetPos{ centerChunk.x + dx, centerChunk.z + dz };

                // Calculate distance from viewer to closest point of chunk or chunk center
                AABB bounds = GetChunkBounds(targetPos);
                Vec3 center = bounds.Center();
                f32 distSq = (center.x - m_viewerPosition.x) * (center.x - m_viewerPosition.x) +
                             (center.z - m_viewerPosition.z) * (center.z - m_viewerPosition.z);

                if (distSq <= radiusSq)
                {
                    auto it = m_chunks.find(targetPos);
                    if (it == m_chunks.end() || it->second.state == ChunkState::Unloaded)
                    {
                        ChunkData data;
                        data.pos = targetPos;
                        data.bounds = bounds;
                        data.state = ChunkState::Loading;
                        m_chunks[targetPos] = std::move(data);

                        m_pendingJobCount.fetch_add(1, std::memory_order_relaxed);

                        // Dispatch asynchronous chunk streaming job via JobSystem
                        lacrima::Async([this, targetPos]() {
                            // Perform background data decompression / asset setup
                            {
                                std::lock_guard<std::mutex> lock(m_completedMutex);
                                m_completedLoads.push_back(targetPos);
                            }
                            m_pendingJobCount.fetch_sub(1, std::memory_order_release);
                        });
                    }
                }
            }
        }

        // 3. Unload chunks that have moved beyond streaming radius + hysteresis margin
        const f32 hysteresis = m_chunkSize * 1.5f;
        const f32 unloadRadius = m_streamingRadius + hysteresis;
        const f32 unloadRadiusSq = unloadRadius * unloadRadius;

        std::vector<ChunkPos> toRemove;
        for (auto& [pos, data] : m_chunks)
        {
            if (data.state == ChunkState::Loaded)
            {
                Vec3 center = data.bounds.Center();
                f32 distSq = (center.x - m_viewerPosition.x) * (center.x - m_viewerPosition.x) +
                             (center.z - m_viewerPosition.z) * (center.z - m_viewerPosition.z);

                if (distSq > unloadRadiusSq)
                {
                    // Clean up chunk entities
                    for (Entity e : data.entities)
                    {
                        world.DestroyEntity(e);
                    }
                    data.entities.clear();
                    data.state = ChunkState::Unloaded;
                    toRemove.push_back(pos);

                    LACRIMA_LOG_TRACE(lacrima::LogCategory::Core,
                        "WorldStreamer: Unloading chunk (%d, %d)", pos.x, pos.z);
                }
            }
        }

        for (const auto& pos : toRemove)
        {
            m_chunks.erase(pos);
        }
    }

    void WorldStreamer::GetVisibleChunks(const Frustum& frustum, std::vector<ChunkPos>& outVisible) const
    {
        outVisible.clear();
        for (const auto& [pos, data] : m_chunks)
        {
            if (data.state == ChunkState::Loaded)
            {
                if (frustum.Intersects(data.bounds))
                {
                    outVisible.push_back(pos);
                }
            }
        }
    }

    void WorldStreamer::WaitAllPendingJobs()
    {
        while (m_pendingJobCount.load(std::memory_order_acquire) > 0)
        {
            if (!lacrima::JobSystem::Get().TryStealAndRunOne())
            {
                std::this_thread::yield();
            }
        }
    }
}
