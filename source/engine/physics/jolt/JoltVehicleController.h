// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include <physics/IVehicleController.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace JPH {
    class PhysicsSystem;
    class TempAllocator;
}

namespace lacrima::physics
{
    // ---------------------------------------------------------------------------
    // JoltVehicleController - Concrete Jolt WheeledVehicle implementation
    //
    // Maps lacrima VehicleDesc -> JPH VehicleConstraint + WheeledVehicleController.
    // One body + one VehicleConstraint is created per controller.
    // Body is added to the Jolt PhysicsSystem with MotionType::Dynamic so it
    // participates in all standard collision queries (CastRay, etc.).
    //
    // The VehicleConstraint is stepped inside JoltPhysicsSystem::Step after
    // calling JPH::PhysicsSystem::Update (Jolt's constraint integration is
    // called through the standard physics step).
    // ---------------------------------------------------------------------------
    class JoltVehicleController : public IVehicleController
    {
    public:
        JoltVehicleController(const VehicleDesc& desc,
                              JPH::PhysicsSystem* physicsSystem,
                              JPH::TempAllocator* tempAllocator);
        ~JoltVehicleController() override;

        // Driver input
        void SetDriverInput(float forward, float right, float brake, float handBrake) override;

        // Transform
        glm::vec3 GetPosition() const override;
        glm::quat GetRotation() const override;
        void      SetPosition(const glm::vec3& pos) override;

        // Velocity
        glm::vec3 GetLinearVelocity()  const override;
        glm::vec3 GetAngularVelocity() const override;
        float     GetSpeedKmh()        const override;

        // Wheel telemetry
        u32        GetWheelCount()        const override;
        WheelState GetWheelState(u32 idx) const override;

        // Step (called by JoltPhysicsSystem)
        void Step(float deltaTime) override;

        JPH::BodyID GetBodyID() const { return m_bodyID; }

    private:
        JPH::PhysicsSystem*            m_physicsSystem = nullptr;
        JPH::TempAllocator*            m_tempAllocator = nullptr;
        JPH::BodyID                    m_bodyID;
        JPH::Ref<JPH::VehicleConstraint> m_vehicleConstraint;
        u32                            m_wheelCount = 0;

        // Current driver inputs (latched until next SetDriverInput)
        float m_inputForward   = 0.0f;
        float m_inputRight     = 0.0f;
        float m_inputBrake     = 0.0f;
        float m_inputHandBrake = 0.0f;
    };
}
