// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include <physics/PhysicsTypes.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lacrima::physics
{
    // ---------------------------------------------------------------------------
    // IVehicleController.h - Abstract interface for wheeled vehicle physics
    //
    // Wraps a physics-backend vehicle (e.g. Jolt WheeledVehicleController).
    // Intentionally decoupled from Jolt headers so the simulation / ECS layer
    // can depend on lacrima_physics without pulling in Jolt.h.
    //
    // Coordinate convention: Y-up, right-handed (same as the rest of the engine).
    //
    // Lifecycle
    //   1. IPhysicsSystem::CreateVehicleController(VehicleDesc) returns a new
    //      owned IVehicleController*.
    //   2. Each physics Step(), the system calls IVehicleController::Step().
    //   3. Call IPhysicsSystem::DestroyVehicleController() to free.
    // ---------------------------------------------------------------------------

    struct WheelDesc
    {
        glm::vec3 positionLocal{0.0f};      // Wheel attachment point in vehicle-local space
        float     radius      = 0.35f;      // Wheel radius (meters)
        float     width       = 0.22f;      // Wheel width (meters)
        float     suspensionMinLength = 0.1f;
        float     suspensionMaxLength = 0.4f;
        float     suspensionFrequency = 1.5f; // Natural frequency (Hz)
        float     suspensionDamping   = 0.5f; // Damping ratio
        float     maxSteerAngle = 0.0f;     // 0 = non-steering wheel (rad)
        float     maxHandBrakeTorque = 4000.0f; // Rear wheels only
        bool      canSteer  = false;
        bool      canDrive  = true;         // Connected to drivetrain
        bool      handBrake = false;        // Rear wheel handbrake
    };

    struct VehicleDesc
    {
        glm::vec3 position{0.0f, 1.0f, 0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};

        // Body shape (box half-extents in local space)
        glm::vec3 halfExtents{1.0f, 0.35f, 2.2f};
        float     mass = 1500.0f;             // kg

        // Drivetrain
        float     maxEngineTorque  = 500.0f;  // Nm
        float     maxBrakeTorque   = 1500.0f; // Nm per wheel
        float     maxSteerAngle    = 0.5236f; // ~30 deg in radians

        // Differential: 0 = front, 1 = rear, 2 = all-wheel
        int       differentialType = 1;

        std::vector<WheelDesc> wheels;

        void* userData = nullptr;
    };

    // Wheel runtime state (read-only, updated each step by the vehicle controller)
    struct WheelState
    {
        glm::vec3 contactPoint{0.0f};   // World-space contact point
        glm::vec3 contactNormal{0.0f, 1.0f, 0.0f};
        float     suspensionLength = 0.0f;
        float     steerAngle       = 0.0f;  // Current steer angle (rad)
        float     rotationAngle    = 0.0f;  // Cumulative spin (rad)
        bool      isInContact      = false;
    };

    class IVehicleController
    {
    public:
        virtual ~IVehicleController() = default;

        // --- Driver input (call every frame before Step) ---
        // forward:  -1..+1  (-1=full reverse, +1=full throttle)
        // right:    -1..+1  (-1=full left, +1=full right)
        // brake:     0..+1
        // handBrake: 0..+1
        virtual void SetDriverInput(float forward, float right, float brake, float handBrake) = 0;

        // --- Transform (world space) ---
        virtual glm::vec3 GetPosition() const = 0;
        virtual glm::quat GetRotation() const = 0;
        virtual void      SetPosition(const glm::vec3& pos) = 0;

        // --- Velocity ---
        virtual glm::vec3 GetLinearVelocity()  const = 0;
        virtual glm::vec3 GetAngularVelocity() const = 0;
        virtual float     GetSpeedKmh()        const = 0;

        // --- Wheel telemetry (indexed by wheel order from VehicleDesc::wheels) ---
        virtual u32             GetWheelCount()         const = 0;
        virtual WheelState      GetWheelState(u32 idx)  const = 0;

        // --- Simulation (called by JoltPhysicsSystem::Step, not by user code) ---
        virtual void Step(float deltaTime) = 0;
    };
}
