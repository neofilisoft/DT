// Copyright Neofilisoft. All Rights Reserved.
#pragma once

namespace lacrima::core
{
    // Interface for an engine subsystem.
    // Subsystems can be registered with the Engine or SimulationWorld to hook into the main loop.
    class ISubsystem
    {
    public:
        virtual ~ISubsystem() = default;

        // Called when the subsystem is registered/initialized
        virtual void OnInit() = 0;

        // Called every tick
        virtual void OnTick(float deltaTime) = 0;

        // Called when the engine/world is shutting down
        virtual void OnCleanup() = 0;
    };
}
