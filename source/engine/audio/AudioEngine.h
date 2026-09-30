// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include <string>
#include <vector>

// Forward declaration of miniaudio engine to avoid including miniaudio.h in the header
struct ma_engine;
struct ma_sound;

namespace lacrima::audio
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

        // Cleans up one-shot sounds that have finished playing.
        void Update();

        void SetMasterVolume(f32 volume);

        // BGM Control (using libogg for high quality decode)
        void PlayBGM(const std::string& path, f32 volume = 1.0f);
        void StopBGM();

        // Returns nullptr if AudioEngine was never initialized.
        static AudioEngine* Get();

    private:
        ma_engine* m_engine = nullptr;
        ma_sound* m_bgmSound = nullptr;
        bool m_initialized = false;
        bool m_bgmInitialized = false;
        std::vector<ma_sound*> m_activeSounds;
    };
}

