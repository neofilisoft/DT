// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include <physics/IPhysicsBody.h>
#include <box2d/box2d.h>

namespace lacrima::physics
{
    class Box2DPhysicsBody : public IPhysicsBody
    {
    public:
        Box2DPhysicsBody(b2BodyId bodyId);
        ~Box2DPhysicsBody() override;

        glm::vec3 GetPosition() const override;
        void SetPosition(const glm::vec3& position) override;

        glm::quat GetRotation() const override;
        void SetRotation(const glm::quat& rotation) override;

        void AddForce(const glm::vec3& force) override;
        void AddImpulse(const glm::vec3& impulse) override;

        void SetLinearVelocity(const glm::vec3& velocity) override;
        glm::vec3 GetLinearVelocity() const override;

        void* GetNativeHandle() const override { return (void*)&m_bodyId; }

        b2BodyId GetBox2DBodyId() const { return m_bodyId; }

    private:
        b2BodyId m_bodyId;
    };
}

