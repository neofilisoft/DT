#include "core/profiler/Profiler.h"

#include <thread>

namespace dt
{
    void ProfilerThreadBuffer::RecordScope(const char* staticName, std::string dynamicName, u64 startNs, u64 endNs)
    {
        // No lock here: only the owning thread ever appends (see file
        // comment in Profiler.h). The mutex exists solely to guard against
        // a concurrent SnapshotAndClear() call from the Editor thread
        // racing this push_back / vector reallocation.
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(ProfileEvent{
            staticName,
            std::move(dynamicName),
            startNs,
            endNs,
            static_cast<u32>(std::hash<std::thread::id>{}(std::this_thread::get_id()))
        });
    }

    std::vector<ProfileEvent> ProfilerThreadBuffer::SnapshotAndClear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<ProfileEvent> result;
        result.swap(m_events);
        return result;
    }

    Profiler& Profiler::Get()
    {
        static Profiler instance;
        return instance;
    }

    void Profiler::UnregisterThreadBuffer(ProfilerThreadBuffer* buffer)
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        for (auto it = m_threadBuffers.begin(); it != m_threadBuffers.end(); ++it)
        {
            if (*it == buffer)
            {
                m_threadBuffers.erase(it);
                break;
            }
        }
    }

    ProfilerThreadBuffer& Profiler::GetThreadBuffer()
    {
        thread_local ProfilerThreadBuffer* buffer = nullptr;

        if (!buffer)
        {
            buffer = new ProfilerThreadBuffer();
            std::lock_guard<std::mutex> lock(m_registryMutex);
            m_threadBuffers.push_back(buffer);
        }

        return *buffer;
    }

    std::vector<ProfileEvent> Profiler::SnapshotAllThreads()
    {
        std::vector<ProfilerThreadBuffer*> buffersCopy;
        {
            std::lock_guard<std::mutex> lock(m_registryMutex);
            buffersCopy = m_threadBuffers;
        }

        std::vector<ProfileEvent> merged;
        for (ProfilerThreadBuffer* buffer : buffersCopy)
        {
            std::vector<ProfileEvent> threadEvents = buffer->SnapshotAndClear();
            merged.insert(merged.end(),
                std::make_move_iterator(threadEvents.begin()),
                std::make_move_iterator(threadEvents.end()));
        }
        return merged;
    }
}
