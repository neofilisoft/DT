// Copyright Neofilisoft. All Rights Reserved.
#include "Box2DPhysicsBody.h"

namespace lacrima::physics
{
    Box2DPhysicsBody::Box2DPhysicsBody(b2BodyId bodyId)
        : m_bodyId(bodyId)
    {
    }

    Box2DPhysicsBody::~Box2DPhysicsBody()
    {
        // Body destruction is managed by Box2DPhysicsSystem
    }

    glm::vec3 Box2DPhysicsBody::GetPosition() const
    {
        b2Vec2 pos = b2Body_GetPosition(m_bodyId);
        return glm::vec3(pos.x, pos.y, 0.0f);
    }

    void Box2DPhysicsBody::SetPosition(const glm::vec3& position)
    {
        b2Rot rot = b2Body_GetRotation(m_bodyId);
        b2Body_SetTransform(m_bodyId, b2Vec2{position.x, position.y}, rot);
    }

    glm::quat Box2DPhysicsBody::GetRotation() const
    {
        b2Rot rot = b2Body_GetRotation(m_bodyId);
        float angle = b2Rot_GetAngle(rot);
        return glm::quat(glm::vec3(0.0f, 0.0f, angle));
    }

    void Box2DPhysicsBody::SetRotation(const glm::quat& rotation)
    {
        b2Vec2 pos = b2Body_GetPosition(m_bodyId);
        glm::vec3 euler = glm::eulerAngles(rotation);
        float angle = euler.z; // Box2D is 2D, only uses rotation around Z
        b2Body_SetTransform(m_bodyId, pos, b2MakeRot(angle));
    }

    void Box2DPhysicsBody::AddForce(const glm::vec3& force)
    {
        b2Body_ApplyForceToCenter(m_bodyId, b2Vec2{force.x, force.y}, true);
    }

    void Box2DPhysicsBody::AddImpulse(const glm::vec3& impulse)
    {
        b2Body_ApplyLinearImpulseToCenter(m_bodyId, b2Vec2{impulse.x, impulse.y}, true);
    }

    void Box2DPhysicsBody::SetLinearVelocity(const glm::vec3& velocity)
    {
        b2Body_SetLinearVelocity(m_bodyId, b2Vec2{velocity.x, velocity.y});
    }

    glm::vec3 Box2DPhysicsBody::GetLinearVelocity() const
    {
        b2Vec2 vel = b2Body_GetLinearVelocity(m_bodyId);
        return glm::vec3(vel.x, vel.y, 0.0f);
    }
}

