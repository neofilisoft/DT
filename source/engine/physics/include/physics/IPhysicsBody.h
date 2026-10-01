// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include <physics/PhysicsTypes.h>

namespace lacrima::physics
{
    class IPhysicsBody
    {
    public:
        virtual ~IPhysicsBody() = default;

        virtual void SetPosition(const glm::vec3& position) = 0;
        virtual glm::vec3 GetPosition() const = 0;

        virtual void SetRotation(const glm::quat& rotation) = 0;
        virtual glm::quat GetRotation() const = 0;

        virtual void SetLinearVelocity(const glm::vec3& velocity) = 0;
        virtual glm::vec3 GetLinearVelocity() const = 0;

        virtual void AddForce(const glm::vec3& force) = 0;
        virtual void AddImpulse(const glm::vec3& impulse) = 0;
        
        virtual void* GetNativeHandle() const = 0;
    };
}

