// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include <physics/PhysicsTypes.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lacrima::physics
{
    enum class GroundState
    {
        OnGround,
        OnSteepGround,
        NotSupported,
        InAir
    };

    struct CharacterControllerDesc
    {
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        float radius = 0.3f;       // Capsule radius (meters)
        float halfHeight = 0.6f;   // Capsule cylinder half-height (total height = 2*halfHeight + 2*radius = 1.8m)
        float mass = 70.0f;        // Mass in kg
        float maxSlopeAngle = 0.785398f; // Max walkable slope in radians (~45 degrees)
        float maxStrength = 100.0f;// Force to push other rigid bodies (N)
        float stepHeight = 0.3f;   // Max step-up height for stairs/curbs (meters)
        void* userData = nullptr;
    };

    class ICharacterController
    {
    public:
        virtual ~ICharacterController() = default;

        // Velocity and Movement
        virtual void SetLinearVelocity(const glm::vec3& velocity) = 0;
        virtual glm::vec3 GetLinearVelocity() const = 0;

        // Transform
        virtual void SetPosition(const glm::vec3& position) = 0;
        virtual glm::vec3 GetPosition() const = 0;

        virtual void SetRotation(const glm::quat& rotation) = 0;
        virtual glm::quat GetRotation() const = 0;

        // Ground Contact and Slope
        virtual GroundState GetGroundState() const = 0;
        virtual bool IsOnGround() const = 0;
        virtual glm::vec3 GetGroundNormal() const = 0;
        virtual glm::vec3 GetGroundVelocity() const = 0;

        // Actions
        virtual void Jump(float jumpSpeed) = 0;

        // Tick simulation
        virtual void Update(float deltaTime, const glm::vec3& gravity = glm::vec3(0.0f, -9.81f, 0.0f)) = 0;
    };
}
