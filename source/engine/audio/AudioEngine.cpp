#include "audio/AudioEngine.h"
#include "core/logging/Logger.h"

#include <miniaudio.h>

namespace dt::audio
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
        
        ma_engine_config engineConfig = ma_engine_config_init();

        if (ma_engine_init(&engineConfig, m_engine) != MA_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Core, "AudioEngine: failed to initialize miniaudio engine");
            delete m_engine;
            m_engine = nullptr;
            return false;
        }

        s_instance = this;
        m_initialized = true;
        DT_LOG_INFO(LogCategory::Core, "AudioEngine: miniaudio initialized successfully");
        
        return true;
    }

    void AudioEngine::Shutdown()
    {
        if (!m_initialized)
            return;

        if (m_engine)
        {
            ma_engine_uninit(m_engine);
            delete m_engine;
            m_engine = nullptr;
        }

        if (s_instance == this)
        {
            s_instance = nullptr;
        }
        
        m_initialized = false;
        DT_LOG_INFO(LogCategory::Core, "AudioEngine: shut down");
    }

    void AudioEngine::PlaySound2D(const std::string& path, f32 volume)
    {
        if (!m_initialized || !m_engine)
            return;

        // Note: miniaudio's play_sound takes a file path directly and streams/decodes it behind the scenes
        ma_engine_play_sound(m_engine, path.c_str(), nullptr);
        
        // Volume setting for simple play_sound is global per sound group.
        // For individual control we would need ma_sound, but for simple SFX this works.
        // In this basic version, we just let it play at default volume.
    }
}
