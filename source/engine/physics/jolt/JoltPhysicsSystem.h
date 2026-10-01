// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include <physics/IPhysicsSystem.h>
#include <memory>
#include <vector>

namespace JPH {
    class PhysicsSystem;
    class JobSystemThreadPool;
    class TempAllocatorImpl;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
}

namespace lacrima::physics
{
    class JoltPhysicsSystem : public IPhysicsSystem
    {
    public:
        JoltPhysicsSystem();
        ~JoltPhysicsSystem() override;

        bool Initialize() override;
        void Shutdown() override;
        
        void Step(float deltaTime) override;

        IPhysicsBody* CreateBody(const PhysicsBodyDesc& desc) override;
        void DestroyBody(IPhysicsBody* body) override;

        ICharacterController* CreateCharacterController(const CharacterControllerDesc& desc) override;
        void DestroyCharacterController(ICharacterController* controller) override;

        IVehicleController* CreateVehicleController(const VehicleDesc& desc) override;
        void DestroyVehicleController(IVehicleController* controller) override;



        bool CastRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit) override;
        bool CastSphere(const glm::vec3& origin, const glm::vec3& direction, float radius, float maxDistance, RaycastHit& outHit) override;

    private:
        JPH::PhysicsSystem* m_physicsSystem = nullptr;
        JPH::JobSystemThreadPool* m_jobSystem = nullptr;
        JPH::TempAllocatorImpl* m_tempAllocator = nullptr;

        JPH::BroadPhaseLayerInterface* m_broadPhaseLayerInterface = nullptr;
        JPH::ObjectVsBroadPhaseLayerFilter* m_objectVsBroadPhaseLayerFilter = nullptr;
        JPH::ObjectLayerPairFilter* m_objectLayerPairFilter = nullptr;

        std::vector<std::unique_ptr<IPhysicsBody>> m_bodies;
        std::vector<std::unique_ptr<ICharacterController>> m_characterControllers;
        std::vector<std::unique_ptr<IVehicleController>> m_vehicleControllers;
    };
}



