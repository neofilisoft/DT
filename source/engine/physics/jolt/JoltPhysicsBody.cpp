// Copyright Neofilisoft. All Rights Reserved.
#include "JoltPhysicsBody.h"
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Quat.h>

namespace lacrima::physics
{
    static inline JPH::Vec3 ToJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
    static inline glm::vec3 ToGlm(const JPH::Vec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
    static inline JPH::Quat ToJolt(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
    static inline glm::quat ToGlm(const JPH::Quat& q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

    JoltPhysicsBody::JoltPhysicsBody(JPH::BodyID id, JPH::BodyInterface* bodyInterface)
        : m_id(id), m_bodyInterface(bodyInterface)
    {
    }

    void JoltPhysicsBody::SetPosition(const glm::vec3& position)
    {
        m_bodyInterface->SetPosition(m_id, ToJolt(position), JPH::EActivation::Activate);
    }

    glm::vec3 JoltPhysicsBody::GetPosition() const
    {
        return ToGlm(m_bodyInterface->GetPosition(m_id));
    }

    void JoltPhysicsBody::SetRotation(const glm::quat& rotation)
    {
        m_bodyInterface->SetRotation(m_id, ToJolt(rotation), JPH::EActivation::Activate);
    }

    glm::quat JoltPhysicsBody::GetRotation() const
    {
        return ToGlm(m_bodyInterface->GetRotation(m_id));
    }

    void JoltPhysicsBody::SetLinearVelocity(const glm::vec3& velocity)
    {
        m_bodyInterface->SetLinearVelocity(m_id, ToJolt(velocity));
    }

    glm::vec3 JoltPhysicsBody::GetLinearVelocity() const
    {
        return ToGlm(m_bodyInterface->GetLinearVelocity(m_id));
    }

    void JoltPhysicsBody::AddForce(const glm::vec3& force)
    {
        m_bodyInterface->AddForce(m_id, ToJolt(force));
    }

    void JoltPhysicsBody::AddImpulse(const glm::vec3& impulse)
    {
        m_bodyInterface->AddImpulse(m_id, ToJolt(impulse));
    }

    void* JoltPhysicsBody::GetNativeHandle() const
    {
        // Return a pointer to a static/thread-local representation or cast BodyID directly if size matches.
        // For simplicity, we just return the raw 32-bit ID casted to void pointer.
        return reinterpret_cast<void*>(static_cast<uintptr_t>(m_id.GetIndexAndSequenceNumber()));
    }
}

