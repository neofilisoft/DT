// ---------------------------------------------------------------------------
// source/tools/editor/main.cpp
//
// DTEditor standalone entry point.
// Initialises SDL3 + Vulkan renderer, creates a SimulationWorld, hands
// control to the Editor for frame-by-frame ImGui rendering.
// ---------------------------------------------------------------------------

#include "editor/editor.h"
#include "simulation/world/SimulationWorld.h"
#include "renderer/VulkanRenderer.h"
#include "runtime/Application.h"
#include "core/logging/Logger.h"

int main(int /*argc*/, char* /*argv*/[])
{
    dt::Logger::Get().AddSink(std::make_unique<dt::ConsoleLogSink>());
    DT_LOG_INFO(dt::LogCategory::Core, "DTEditor starting...");

    // Create the simulation world (editor starts with an empty world)
    dt::sim::SimulationWorld world(32);

    // Create the renderer
    dt::VulkanRenderer renderer;

    // Create the editor
    dt::editor::Editor editor;
    editor.Init(&world);

    // TODO: Wire the renderer's ImGui frame callback to the editor
    // renderer.SetEditorConstructCallback([&editor]()
    // {
    //     editor.Construct();
    // });

    // Run via Application (Sim loop will be driven only when in Play mode)
    auto tickFunc = [&world](dt::u64 tickIdx, dt::f64 dt_, dt::SimSnapshot& snap)
    {
        world.Tick(tickIdx, dt_, snap);
    };

    dt::Application app(tickFunc, renderer);
    app.Run();

    editor.Shutdown();
    DT_LOG_INFO(dt::LogCategory::Core, "DTEditor exited cleanly.");
    return 0;
}
