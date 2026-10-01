// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include <physics/IPhysicsSystem.h>
#include <box2d/box2d.h>
#include <vector>
#include <memory>

namespace lacrima::physics
{
    class Box2DPhysicsSystem : public IPhysicsSystem
    {
    public:
        Box2DPhysicsSystem();
        ~Box2DPhysicsSystem() override;

        bool Initialize() override;
        void Shutdown() override;
        
        void Step(float deltaTime) override;

        IPhysicsBody* CreateBody(const PhysicsBodyDesc& desc) override;
        void DestroyBody(IPhysicsBody* body) override;

        ICharacterController* CreateCharacterController(const CharacterControllerDesc& desc) override;
        void DestroyCharacterController(ICharacterController* controller) override;

        // Vehicle physics not supported by Box2D backend (2D only)
        IVehicleController* CreateVehicleController(const VehicleDesc& /*desc*/) override { return nullptr; }
        void DestroyVehicleController(IVehicleController* /*controller*/) override {}



        bool CastRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit) override;
        bool CastSphere(const glm::vec3& origin, const glm::vec3& direction, float radius, float maxDistance, RaycastHit& outHit) override;

    private:
        b2WorldId m_worldId;
        std::vector<std::unique_ptr<IPhysicsBody>> m_bodies;
    };
}


