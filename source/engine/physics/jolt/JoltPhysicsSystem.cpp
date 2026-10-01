// Copyright Neofilisoft. All Rights Reserved.
#include "JoltPhysicsSystem.h"
#include "JoltPhysicsBody.h"
#include "JoltCharacterVirtual.h"
#include "JoltVehicleController.h"
#include <core/platform/Assert.h>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/BodyLock.h>

#include <stdarg.h>
#include <iostream>

namespace lacrima::physics
{
    // Layer definitions
    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    };

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::uint NUM_LAYERS(2);
    };

    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl()
        {
            mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }

        virtual JPH::uint GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            return mObjectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
        {
            switch ((JPH::BroadPhaseLayer::Type)inLayer)
            {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
            default: JPH_ASSERT(false); return "INVALID";
            }
        }
#endif

    private:
        JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            switch (inLayer1)
            {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
            }
        }
    };

    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
        {
            switch (inObject1)
            {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
            }
        }
    };

    static void TraceImpl(const char* inFMT, ...)
    {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);
        std::cout << buffer << std::endl;
    }

    JoltPhysicsSystem::JoltPhysicsSystem()
    {
    }

    JoltPhysicsSystem::~JoltPhysicsSystem()
    {
        Shutdown();
    }

    bool JoltPhysicsSystem::Initialize()
    {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = TraceImpl;

        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        m_tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024); // 10MB
        m_jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

        m_broadPhaseLayerInterface = new BPLayerInterfaceImpl();
        m_objectVsBroadPhaseLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
        m_objectLayerPairFilter = new ObjectLayerPairFilterImpl();

        m_physicsSystem = new JPH::PhysicsSystem();
        m_physicsSystem->Init(1024, 0, 1024, 1024,
            *m_broadPhaseLayerInterface,
            *m_objectVsBroadPhaseLayerFilter,
            *m_objectLayerPairFilter);

        return true;
    }

    void JoltPhysicsSystem::Shutdown()
    {
        m_bodies.clear();
        m_characterControllers.clear();

        if (m_physicsSystem) {
            delete m_physicsSystem;
            m_physicsSystem = nullptr;
        }
        if (m_jobSystem) {
            delete m_jobSystem;
            m_jobSystem = nullptr;
        }
        if (m_tempAllocator) {
            delete m_tempAllocator;
            m_tempAllocator = nullptr;
        }
        if (m_broadPhaseLayerInterface) {
            delete m_broadPhaseLayerInterface;
            m_broadPhaseLayerInterface = nullptr;
        }
        if (m_objectVsBroadPhaseLayerFilter) {
            delete m_objectVsBroadPhaseLayerFilter;
            m_objectVsBroadPhaseLayerFilter = nullptr;
        }
        if (m_objectLayerPairFilter) {
            delete m_objectLayerPairFilter;
            m_objectLayerPairFilter = nullptr;
        }

        if (JPH::Factory::sInstance) {
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    }

    void JoltPhysicsSystem::Step(float deltaTime)
    {
        if (m_physicsSystem)
        {
            int collisionSteps = 1;
            m_physicsSystem->Update(deltaTime, collisionSteps, m_tempAllocator, m_jobSystem);
        }
    }

    IPhysicsBody* JoltPhysicsSystem::CreateBody(const PhysicsBodyDesc& desc)
    {
        JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();
        
        JPH::Ref<JPH::Shape> shape;
        switch (desc.shapeType)
        {
        case PhysicsShapeType::Box:
            shape = new JPH::BoxShape(JPH::Vec3(desc.dimensions.x, desc.dimensions.y, desc.dimensions.z));
            break;
        case PhysicsShapeType::Sphere:
            shape = new JPH::SphereShape(desc.dimensions.x);
            break;
        case PhysicsShapeType::Capsule:
            shape = new JPH::CapsuleShape(desc.dimensions.y, desc.dimensions.x);
            break;
        }

        JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
        JPH::ObjectLayer layer = Layers::MOVING;

        switch (desc.bodyType)
        {
        case PhysicsBodyType::Static:
            motionType = JPH::EMotionType::Static;
            layer = Layers::NON_MOVING;
            break;
        case PhysicsBodyType::Kinematic:
            motionType = JPH::EMotionType::Kinematic;
            layer = Layers::MOVING;
            break;
        case PhysicsBodyType::Dynamic:
            motionType = JPH::EMotionType::Dynamic;
            layer = Layers::MOVING;
            break;
        }

        JPH::BodyCreationSettings settings(shape,
            JPH::Vec3(desc.position.x, desc.position.y, desc.position.z),
            JPH::Quat(desc.rotation.x, desc.rotation.y, desc.rotation.z, desc.rotation.w),
            motionType,
            layer);

        settings.mFriction = desc.friction;
        settings.mRestitution = desc.restitution;
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = desc.mass;

        JPH::BodyID bodyID = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
        if (bodyID.IsInvalid()) return nullptr;

        auto body = std::make_unique<JoltPhysicsBody>(bodyID, &bodyInterface);
        IPhysicsBody* bodyPtr = body.get();
        m_bodies.push_back(std::move(body));

        return bodyPtr;
    }

    ICharacterController* JoltPhysicsSystem::CreateCharacterController(const CharacterControllerDesc& desc)
    {
        if (!m_physicsSystem || !m_tempAllocator) return nullptr;
        auto controller = std::make_unique<JoltCharacterVirtual>(desc, m_physicsSystem, m_tempAllocator);
        ICharacterController* ptr = controller.get();
        m_characterControllers.push_back(std::move(controller));
        return ptr;
    }

    void JoltPhysicsSystem::DestroyCharacterController(ICharacterController* controller)
    {
        if (!controller) return;
        auto it = std::find_if(m_characterControllers.begin(), m_characterControllers.end(),
            [controller](const std::unique_ptr<ICharacterController>& c) { return c.get() == controller; });
        if (it != m_characterControllers.end())
        {
            m_characterControllers.erase(it);
        }
    }

    void JoltPhysicsSystem::DestroyBody(IPhysicsBody* body)
    {
        JoltPhysicsBody* joltBody = static_cast<JoltPhysicsBody*>(body);
        if (joltBody)
        {
            JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();
            bodyInterface.RemoveBody(joltBody->GetBodyID());
            bodyInterface.DestroyBody(joltBody->GetBodyID());

            auto it = std::find_if(m_bodies.begin(), m_bodies.end(), [joltBody](const std::unique_ptr<IPhysicsBody>& ptr) {
                return ptr.get() == joltBody;
            });

            if (it != m_bodies.end()) {
                m_bodies.erase(it);
            }
        }
    }
    bool JoltPhysicsSystem::CastRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit)
    {
        if (!m_physicsSystem) return false;

        glm::vec3 dir = direction;
        float len = glm::length(dir);
        if (len < 1e-6f || maxDistance <= 0.0f) return false;
        dir = glm::normalize(dir);

        JPH::RVec3 jOrigin(origin.x, origin.y, origin.z);
        JPH::Vec3 jDir(dir.x * maxDistance, dir.y * maxDistance, dir.z * maxDistance);
        JPH::RRayCast ray(jOrigin, jDir);
        JPH::RayCastResult hit;

        bool hasHit = m_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, hit);
        if (hasHit)
        {
            outHit.hasHit = true;
            outHit.fraction = hit.mFraction;
            JPH::RVec3 hitPos = ray.GetPointOnRay(hit.mFraction);
            outHit.hitPoint = glm::vec3(static_cast<float>(hitPos.GetX()), static_cast<float>(hitPos.GetY()), static_cast<float>(hitPos.GetZ()));

            JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), hit.mBodyID);
            if (lock.Succeeded())
            {
                const JPH::Body& body = lock.GetBody();
                JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPos);
                outHit.hitNormal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
                outHit.bodyUserData = reinterpret_cast<void*>(body.GetUserData());
            }
            return true;
        }
        return false;
    }

    bool JoltPhysicsSystem::CastSphere(const glm::vec3& origin, const glm::vec3& direction, float radius, float maxDistance, RaycastHit& outHit)
    {
        if (!m_physicsSystem) return false;

        glm::vec3 dir = direction;
        float len = glm::length(dir);
        if (len < 1e-6f || maxDistance <= 0.0f) return false;
        dir = glm::normalize(dir);

        JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(std::max(radius, 0.01f));
        JPH::RVec3 jOrigin(origin.x, origin.y, origin.z);
        JPH::Vec3 jDir(dir.x * maxDistance, dir.y * maxDistance, dir.z * maxDistance);

        JPH::RShapeCast shapeCast(sphere, JPH::Vec3::sReplicate(1.0f), JPH::RMat44::sTranslation(jOrigin), jDir);

        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
        JPH::ShapeCastSettings settings;
        m_physicsSystem->GetNarrowPhaseQuery().CastShape(shapeCast, settings, JPH::RVec3::sZero(), collector);

        if (collector.HadHit())
        {
            outHit.hasHit = true;
            outHit.fraction = collector.mHit.mFraction;
            JPH::RVec3 hitPos = jOrigin + jDir * collector.mHit.mFraction;
            outHit.hitPoint = glm::vec3(static_cast<float>(hitPos.GetX()), static_cast<float>(hitPos.GetY()), static_cast<float>(hitPos.GetZ()));

            JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), collector.mHit.mBodyID2);
            if (lock.Succeeded())
            {
                const JPH::Body& body = lock.GetBody();
                JPH::Vec3 normal = -collector.mHit.mPenetrationAxis.Normalized();
                outHit.hitNormal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
                outHit.bodyUserData = reinterpret_cast<void*>(body.GetUserData());
            }
            return true;
        }
        return false;
    }
}

// ---------------------------------------------------------------------------
// Vehicle Controller - appended to JoltPhysicsSystem.cpp
// ---------------------------------------------------------------------------
namespace lacrima::physics
{
    IVehicleController* JoltPhysicsSystem::CreateVehicleController(const VehicleDesc& desc)
    {
        auto controller = std::make_unique<JoltVehicleController>(desc, m_physicsSystem, m_tempAllocator);
        IVehicleController* ptr = controller.get();
        m_vehicleControllers.push_back(std::move(controller));
        return ptr;
    }

    void JoltPhysicsSystem::DestroyVehicleController(IVehicleController* controller)
    {
        auto it = std::find_if(m_vehicleControllers.begin(), m_vehicleControllers.end(),
            [controller](const std::unique_ptr<IVehicleController>& c) { return c.get() == controller; });
        if (it != m_vehicleControllers.end())
            m_vehicleControllers.erase(it);
    }
}
