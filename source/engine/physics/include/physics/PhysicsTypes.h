// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include <core/platform/Types.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lacrima::physics
{
    enum class PhysicsBackend
    {
        Jolt3D,
        Box2D
    };

    enum class PhysicsBodyType
    {
        Static,
        Kinematic,
        Dynamic
    };

    enum class PhysicsShapeType
    {
        Box,
        Sphere,
        Capsule
    };

    struct PhysicsBodyDesc
    {
        PhysicsBodyType bodyType = PhysicsBodyType::Dynamic;
        PhysicsShapeType shapeType = PhysicsShapeType::Box;
        
        glm::vec3 position{0.0f};
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // w, x, y, z

        // Dimensions:
        // Box: half-extents
        // Sphere: radius = x
        // Capsule: radius = x, half-height = y
        glm::vec3 dimensions{0.5f};
        
        float mass = 1.0f;
        float friction = 0.5f;
        float restitution = 0.0f;
    };

    struct RaycastHit
    {
        bool hasHit = false;
        glm::vec3 hitPoint{0.0f};
        glm::vec3 hitNormal{0.0f, 1.0f, 0.0f};
        float fraction = 1.0f;
        void* bodyUserData = nullptr;
    };
}

