#pragma once

#include "core/platform/Types.h"
#include <string>

// Forward declaration of miniaudio engine to avoid including miniaudio.h in the header
struct ma_engine;

namespace dt::audio
{
    class AudioEngine
    {
    public:
        AudioEngine() = default;
        ~AudioEngine();

        // Non-copyable, non-movable
        AudioEngine(const AudioEngine&) = delete;
        AudioEngine& operator=(const AudioEngine&) = delete;
        AudioEngine(AudioEngine&&) = delete;
        AudioEngine& operator=(AudioEngine&&) = delete;

        bool Initialize();
        void Shutdown();

        // Play a 2D sound effect from a file path
        void PlaySound2D(const std::string& path, f32 volume = 1.0f);

        // Returns nullptr if AudioEngine was never initialized.
        static AudioEngine* Get();

    private:
        ma_engine* m_engine = nullptr;
        bool m_initialized = false;
    };
}
