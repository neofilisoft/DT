// Copyright Neofilisoft. All Rights Reserved.
#include "audio/AudioEngine.h"
#include "core/logging/Logger.h"

#include <algorithm>
#include <miniaudio.h>

namespace lacrima::audio
{
    static AudioEngine* s_instance = nullptr;

    AudioEngine* AudioEngine::Get()
    {
        return s_instance;
    }

    AudioEngine::~AudioEngine()
    {
        Shutdown();
    }

    bool AudioEngine::Initialize()
    {
        if (m_initialized)
            return true;

        m_engine = new ma_engine();
        m_bgmSound = new ma_sound();
        m_bgmInitialized = false;

        ma_engine_config engineConfig = ma_engine_config_init();
        if (ma_engine_init(&engineConfig, m_engine) != MA_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "AudioEngine: failed to initialize miniaudio engine");
            delete m_bgmSound;
            delete m_engine;
            m_bgmSound = nullptr;
            m_engine = nullptr;
            return false;
        }

        s_instance = this;
        m_initialized = true;
        LACRIMA_LOG_INFO(LogCategory::Core, "AudioEngine: miniaudio initialized successfully");
        return true;
    }

    void AudioEngine::Shutdown()
    {
        if (!m_initialized)
            return;

        for (ma_sound* sound : m_activeSounds)
        {
            if (sound)
            {
                ma_sound_uninit(sound);
                delete sound;
            }
        }
        m_activeSounds.clear();

        if (m_bgmSound)
        {
            if (m_bgmInitialized)
                ma_sound_uninit(m_bgmSound);
            delete m_bgmSound;
            m_bgmSound = nullptr;
            m_bgmInitialized = false;
        }

        if (m_engine)
        {
            ma_engine_uninit(m_engine);
            delete m_engine;
            m_engine = nullptr;
        }

        if (s_instance == this)
            s_instance = nullptr;

        m_initialized = false;
        LACRIMA_LOG_INFO(LogCategory::Core, "AudioEngine: shut down");
    }

    void AudioEngine::PlaySound2D(const std::string& path, f32 volume)
    {
        if (!m_initialized || !m_engine || path.empty())
            return;

        auto* sound = new ma_sound();
        const ma_result result = ma_sound_init_from_file(
            m_engine, path.c_str(), 0, nullptr, nullptr, sound);
        if (result != MA_SUCCESS)
        {
            delete sound;
            LACRIMA_LOG_WARN(LogCategory::Core, "AudioEngine: failed to load sound {}", path);
            return;
        }

        ma_sound_set_volume(sound, std::max(0.0f, volume));
        ma_sound_start(sound);
        m_activeSounds.push_back(sound);
    }

    void AudioEngine::Update()
    {
        if (!m_initialized)
            return;

        for (auto it = m_activeSounds.begin(); it != m_activeSounds.end();)
        {
            ma_sound* sound = *it;
            if (sound == nullptr || !ma_sound_is_playing(sound))
            {
                if (sound)
                {
                    ma_sound_uninit(sound);
                    delete sound;
                }
                it = m_activeSounds.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void AudioEngine::SetMasterVolume(f32 volume)
    {
        if (m_initialized && m_engine)
            ma_engine_set_volume(m_engine, std::max(0.0f, volume));
    }

    void AudioEngine::PlayBGM(const std::string& path, f32 volume)
    {
        if (!m_initialized || !m_engine || !m_bgmSound || path.empty())
            return;

        StopBGM();
        const ma_result result = ma_sound_init_from_file(
            m_engine, path.c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr, m_bgmSound);
        if (result == MA_SUCCESS)
        {
            m_bgmInitialized = true;
            ma_sound_set_volume(m_bgmSound, std::max(0.0f, volume));
            ma_sound_set_looping(m_bgmSound, MA_TRUE);
            ma_sound_start(m_bgmSound);
            LACRIMA_LOG_INFO(LogCategory::Core, "AudioEngine: Playing BGM {}", path);
        }
        else
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "AudioEngine: Failed to load BGM {}", path);
        }
    }

    void AudioEngine::StopBGM()
    {
        if (!m_initialized || !m_bgmSound || !m_bgmInitialized)
            return;

        if (ma_sound_is_playing(m_bgmSound))
            ma_sound_stop(m_bgmSound);
        ma_sound_uninit(m_bgmSound);
        m_bgmInitialized = false;
    }
}

