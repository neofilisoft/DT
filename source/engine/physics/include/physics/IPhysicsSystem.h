// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include <physics/PhysicsTypes.h>
#include <physics/IPhysicsBody.h>
#include <physics/ICharacterController.h>
#include <physics/IVehicleController.h>

namespace lacrima::physics
{
    class IPhysicsSystem
    {
    public:
        virtual ~IPhysicsSystem() = default;

        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;
        
        virtual void Step(float deltaTime) = 0;

        virtual IPhysicsBody* CreateBody(const PhysicsBodyDesc& desc) = 0;
        virtual void DestroyBody(IPhysicsBody* body) = 0;

        virtual ICharacterController* CreateCharacterController(const CharacterControllerDesc& desc) = 0;
        virtual void DestroyCharacterController(ICharacterController* controller) = 0;

        virtual IVehicleController* CreateVehicleController(const VehicleDesc& desc) = 0;
        virtual void DestroyVehicleController(IVehicleController* controller) = 0;

        virtual bool CastRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit) = 0;
        virtual bool CastSphere(const glm::vec3& origin, const glm::vec3& direction, float radius, float maxDistance, RaycastHit& outHit) = 0;
    };
}

