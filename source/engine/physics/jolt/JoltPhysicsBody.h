// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include <physics/IPhysicsBody.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace JPH {
    class BodyInterface;
}

namespace lacrima::physics
{
    class JoltPhysicsBody : public IPhysicsBody
    {
    public:
        JoltPhysicsBody(JPH::BodyID id, JPH::BodyInterface* bodyInterface);
        ~JoltPhysicsBody() override = default;

        void SetPosition(const glm::vec3& position) override;
        glm::vec3 GetPosition() const override;

        void SetRotation(const glm::quat& rotation) override;
        glm::quat GetRotation() const override;

        void SetLinearVelocity(const glm::vec3& velocity) override;
        glm::vec3 GetLinearVelocity() const override;

        void AddForce(const glm::vec3& force) override;
        void AddImpulse(const glm::vec3& impulse) override;

        void* GetNativeHandle() const override;

        JPH::BodyID GetBodyID() const { return m_id; }

    private:
        JPH::BodyID m_id;
        JPH::BodyInterface* m_bodyInterface;
    };
}

