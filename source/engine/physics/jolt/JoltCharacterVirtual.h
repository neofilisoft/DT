// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include <physics/ICharacterController.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

namespace JPH {
    class PhysicsSystem;
    class TempAllocator;
}

namespace lacrima::physics
{
    class JoltCharacterVirtual : public ICharacterController
    {
    public:
        JoltCharacterVirtual(const CharacterControllerDesc& desc, JPH::PhysicsSystem* physicsSystem, JPH::TempAllocator* tempAllocator);
        ~JoltCharacterVirtual() override = default;

        void SetLinearVelocity(const glm::vec3& velocity) override;
        glm::vec3 GetLinearVelocity() const override;

        void SetPosition(const glm::vec3& position) override;
        glm::vec3 GetPosition() const override;

        void SetRotation(const glm::quat& rotation) override;
        glm::quat GetRotation() const override;

        GroundState GetGroundState() const override;
        bool IsOnGround() const override;
        glm::vec3 GetGroundNormal() const override;
        glm::vec3 GetGroundVelocity() const override;

        void Jump(float jumpSpeed) override;
        void Update(float deltaTime, const glm::vec3& gravity = glm::vec3(0.0f, -9.81f, 0.0f)) override;

        JPH::CharacterVirtual* GetJoltCharacter() { return m_character.GetPtr(); }

    private:
        JPH::Ref<JPH::CharacterVirtual> m_character;
        JPH::PhysicsSystem*             m_physicsSystem = nullptr;
        JPH::TempAllocator*             m_tempAllocator = nullptr;
        float                           m_stepHeight = 0.3f;
    };
}
