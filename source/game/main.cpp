#include "Game.h"

#include "core/jobs/JobSystem.h"
#include "core/logging/Logger.h"
#include "renderer/VulkanRenderer.h"
#include "audio/AudioEngine.h"
#include "runtime/Application.h"

#include <memory>

// ---------------------------------------------------------------------------
// game/main.cpp
//
// Milestone 5 harness entry point: bootstraps engine-level singletons
// (Logger sinks, JobSystem), constructs the Domaintic-specific Game
// instance, wires its tick function into an engine Application with a
// real VulkanRenderer, and runs.
// ---------------------------------------------------------------------------

namespace
{
    constexpr dt::f32 kInitialTimeScale = 1.0f; // Default to 1x real-time for visual checking
    constexpr dt::usize kInitialEntityCount = 12;
}

int main()
{
    using namespace dt;

    Logger::Get().AddSink(std::make_unique<ConsoleLogSink>());
    Logger::Get().SetMinLevel(LogCategory::Core, LogLevel::Info);
    Logger::Get().SetMinLevel(LogCategory::Simulation, LogLevel::Info);
    Logger::Get().SetMinLevel(LogCategory::Renderer, LogLevel::Info);

    JobSystem::Get().Initialize();

    audio::AudioEngine audioEngine;
    audioEngine.Initialize();

    game::Game domainticGame(kInitialEntityCount);

    // Wire audio into simulation via the callback - keeps dt_simulation
    // dependency-free from dt_audio (per "Engine doesn't know Game" rule).
    domainticGame.World_Mutable().SetPlaySoundCallback([&](const std::string& path)
    {
        audioEngine.PlaySound2D(path);
    });

    VulkanRenderer renderer;

    // Game must outlive `app` - see MakeTickFunc's doc comment in Game.h.
    // Both are stack locals in this scope with domainticGame declared
    // first, so C++ destruction order (reverse of construction) guarantees
    // app is destroyed before domainticGame.
    Application app(domainticGame.MakeTickFunc(), renderer);
    renderer.SetApplication(&app);
    app.Sim().SetTimeScale(kInitialTimeScale);

    DT_LOG_INFO(LogCategory::Core, "Domaintic: {} entities, Vulkan Renderer active, time-scale x{:.0f}.",
        domainticGame.LiveEntityCount(), kInitialTimeScale);

    app.Run();

    audioEngine.Shutdown();
    JobSystem::Get().Shutdown();
    return 0;
}
