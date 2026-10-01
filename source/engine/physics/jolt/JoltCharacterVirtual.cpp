// Copyright Neofilisoft. All Rights Reserved.
#include "JoltCharacterVirtual.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace lacrima::physics
{
    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
    }

    JoltCharacterVirtual::JoltCharacterVirtual(
        const CharacterControllerDesc& desc,
        JPH::PhysicsSystem* physicsSystem,
        JPH::TempAllocator* tempAllocator)
        : m_physicsSystem(physicsSystem)
        , m_tempAllocator(tempAllocator)
        , m_stepHeight(desc.stepHeight)
    {
        JPH::CharacterVirtualSettings settings;
        settings.mMass = desc.mass;
        settings.mMaxSlopeAngle = desc.maxSlopeAngle;
        settings.mMaxStrength = desc.maxStrength;

        // Create capsule standing upright with base at (0,0,0)
        JPH::RefConst<JPH::Shape> rawCapsule = new JPH::CapsuleShape(desc.halfHeight, desc.radius);
        settings.mShape = JPH::RotatedTranslatedShapeSettings(
            JPH::Vec3(0.0f, desc.halfHeight + desc.radius, 0.0f),
            JPH::Quat::sIdentity(),
            rawCapsule
        ).Create().Get();

        settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -desc.radius);

        JPH::RVec3 initialPos(desc.position.x, desc.position.y, desc.position.z);
        JPH::Quat initialRot(desc.rotation.x, desc.rotation.y, desc.rotation.z, desc.rotation.w);

        m_character = new JPH::CharacterVirtual(&settings, initialPos, initialRot, reinterpret_cast<uintptr_t>(desc.userData), m_physicsSystem);
    }

    void JoltCharacterVirtual::SetLinearVelocity(const glm::vec3& velocity)
    {
        if (m_character)
        {
            m_character->SetLinearVelocity(JPH::Vec3(velocity.x, velocity.y, velocity.z));
        }
    }

    glm::vec3 JoltCharacterVirtual::GetLinearVelocity() const
    {
        if (!m_character) return glm::vec3(0.0f);
        JPH::Vec3 v = m_character->GetLinearVelocity();
        return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
    }

    void JoltCharacterVirtual::SetPosition(const glm::vec3& position)
    {
        if (m_character)
        {
            m_character->SetPosition(JPH::RVec3(position.x, position.y, position.z));
        }
    }

    glm::vec3 JoltCharacterVirtual::GetPosition() const
    {
        if (!m_character) return glm::vec3(0.0f);
        JPH::RVec3 p = m_character->GetPosition();
        return glm::vec3(static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ()));
    }

    void JoltCharacterVirtual::SetRotation(const glm::quat& rotation)
    {
        if (m_character)
        {
            m_character->SetRotation(JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w));
        }
    }

    glm::quat JoltCharacterVirtual::GetRotation() const
    {
        if (!m_character) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        JPH::Quat q = m_character->GetRotation();
        return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
    }

    GroundState JoltCharacterVirtual::GetGroundState() const
    {
        if (!m_character) return GroundState::InAir;
        switch (m_character->GetGroundState())
        {
            case JPH::CharacterBase::EGroundState::OnGround:      return GroundState::OnGround;
            case JPH::CharacterBase::EGroundState::OnSteepGround: return GroundState::OnSteepGround;
            case JPH::CharacterBase::EGroundState::NotSupported:  return GroundState::NotSupported;
            case JPH::CharacterBase::EGroundState::InAir:         return GroundState::InAir;
        }
        return GroundState::InAir;
    }

    bool JoltCharacterVirtual::IsOnGround() const
    {
        if (!m_character) return false;
        return m_character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
    }

    glm::vec3 JoltCharacterVirtual::GetGroundNormal() const
    {
        if (!m_character) return glm::vec3(0.0f, 1.0f, 0.0f);
        JPH::Vec3 n = m_character->GetGroundNormal();
        return glm::vec3(n.GetX(), n.GetY(), n.GetZ());
    }

    glm::vec3 JoltCharacterVirtual::GetGroundVelocity() const
    {
        if (!m_character) return glm::vec3(0.0f);
        JPH::Vec3 gv = m_character->GetGroundVelocity();
        return glm::vec3(gv.GetX(), gv.GetY(), gv.GetZ());
    }

    void JoltCharacterVirtual::Jump(float jumpSpeed)
    {
        if (!m_character) return;
        if (IsOnGround())
        {
            JPH::Vec3 vel = m_character->GetLinearVelocity();
            vel.SetY(jumpSpeed);
            m_character->SetLinearVelocity(vel);
        }
    }

    void JoltCharacterVirtual::Update(float deltaTime, const glm::vec3& gravity)
    {
        if (!m_character || !m_physicsSystem || !m_tempAllocator || deltaTime <= 0.0f) return;

        JPH::Vec3 jGravity(gravity.x, gravity.y, gravity.z);
        JPH::Vec3 currentVel = m_character->GetLinearVelocity();

        // Apply gravity if in air
        if (!IsOnGround())
        {
            currentVel += jGravity * deltaTime;
        }
        else
        {
            if (currentVel.GetY() < 0.0f)
            {
                currentVel.SetY(0.0f);
            }
        }

        // Cancel velocity towards steep slopes
        currentVel = m_character->CancelVelocityTowardsSteepSlopes(currentVel);
        m_character->SetLinearVelocity(currentVel);

        JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
        updateSettings.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.5f, 0.0f);
        updateSettings.mWalkStairsStepUp     = JPH::Vec3(0.0f, m_stepHeight, 0.0f);

        m_character->ExtendedUpdate(
            deltaTime,
            jGravity,
            updateSettings,
            m_physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            m_physicsSystem->GetDefaultLayerFilter(Layers::MOVING),
            { },
            { },
            *m_tempAllocator
        );
    }
}
