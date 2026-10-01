// Copyright Neofilisoft. All Rights Reserved.
#include "Box2DPhysicsSystem.h"
#include "Box2DPhysicsBody.h"
#include <core/platform/Assert.h>
#include <algorithm>

namespace lacrima::physics
{
    Box2DPhysicsSystem::Box2DPhysicsSystem()
    {
        m_worldId = b2_nullWorldId;
    }

    Box2DPhysicsSystem::~Box2DPhysicsSystem()
    {
        Shutdown();
    }

    bool Box2DPhysicsSystem::Initialize()
    {
        b2WorldDef worldDef = b2DefaultWorldDef();
        worldDef.gravity = b2Vec2{0.0f, -9.81f};
        m_worldId = b2CreateWorld(&worldDef);
        return b2World_IsValid(m_worldId);
    }

    void Box2DPhysicsSystem::Shutdown()
    {
        m_bodies.clear();
        if (b2World_IsValid(m_worldId)) {
            b2DestroyWorld(m_worldId);
            m_worldId = b2_nullWorldId;
        }
    }

    void Box2DPhysicsSystem::Step(float deltaTime)
    {
        if (b2World_IsValid(m_worldId)) {
            b2World_Step(m_worldId, deltaTime, 4); // 4 substeps is usually good for Box2D v3
        }
    }

    IPhysicsBody* Box2DPhysicsSystem::CreateBody(const PhysicsBodyDesc& desc)
    {
        if (!b2World_IsValid(m_worldId)) return nullptr;

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.position = b2Vec2{desc.position.x, desc.position.y};
        
        glm::vec3 euler = glm::eulerAngles(desc.rotation);
        bodyDef.rotation = b2MakeRot(euler.z);

        switch (desc.bodyType)
        {
        case PhysicsBodyType::Static:
            bodyDef.type = b2_staticBody;
            break;
        case PhysicsBodyType::Kinematic:
            bodyDef.type = b2_kinematicBody;
            break;
        case PhysicsBodyType::Dynamic:
            bodyDef.type = b2_dynamicBody;
            break;
        }

        b2BodyId bodyId = b2CreateBody(m_worldId, &bodyDef);

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.material.friction = desc.friction;
        shapeDef.material.restitution = desc.restitution;
        shapeDef.density = desc.mass > 0.0f ? (desc.mass / (desc.dimensions.x * desc.dimensions.y * 4.0f)) : 1.0f; // Approx density

        switch (desc.shapeType)
        {
        case PhysicsShapeType::Box: {
            b2Polygon box = b2MakeBox(desc.dimensions.x, desc.dimensions.y);
            b2CreatePolygonShape(bodyId, &shapeDef, &box);
            break;
        }
        case PhysicsShapeType::Sphere: {
            b2Circle circle = { b2Vec2{0.0f, 0.0f}, desc.dimensions.x };
            b2CreateCircleShape(bodyId, &shapeDef, &circle);
            break;
        }
        case PhysicsShapeType::Capsule: {
            b2Capsule capsule = { b2Vec2{0.0f, -desc.dimensions.y}, b2Vec2{0.0f, desc.dimensions.y}, desc.dimensions.x };
            b2CreateCapsuleShape(bodyId, &shapeDef, &capsule);
            break;
        }
        }

        auto body = std::make_unique<Box2DPhysicsBody>(bodyId);
        IPhysicsBody* bodyPtr = body.get();
        m_bodies.push_back(std::move(body));

        return bodyPtr;
    }

    void Box2DPhysicsSystem::DestroyBody(IPhysicsBody* body)
    {
        Box2DPhysicsBody* b2Body = static_cast<Box2DPhysicsBody*>(body);
        if (b2Body) {
            b2DestroyBody(b2Body->GetBox2DBodyId());
            auto it = std::find_if(m_bodies.begin(), m_bodies.end(),
                [body](const std::unique_ptr<IPhysicsBody>& ptr) { return ptr.get() == body; });
            if (it != m_bodies.end()) {
                m_bodies.erase(it);
            }
        }
    }
    bool Box2DPhysicsSystem::CastRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit)
    {
        if (!b2World_IsValid(m_worldId)) return false;

        glm::vec3 dir = direction;
        float len = glm::length(dir);
        if (len < 1e-6f || maxDistance <= 0.0f) return false;
        dir = glm::normalize(dir);

        b2Pos bOrigin{origin.x, origin.y};
        b2Vec2 bTranslation{dir.x * maxDistance, dir.y * maxDistance};
        b2QueryFilter filter = b2DefaultQueryFilter();

        b2RayResult result = b2World_CastRayClosest(m_worldId, bOrigin, bTranslation, filter);
        if (result.hit)
        {
            outHit.hasHit = true;
            outHit.fraction = result.fraction;
            outHit.hitPoint = glm::vec3(result.point.x, result.point.y, origin.z);
            outHit.hitNormal = glm::vec3(result.normal.x, result.normal.y, 0.0f);
            return true;
        }
        return false;
    }

    bool Box2DPhysicsSystem::CastSphere(const glm::vec3& origin, const glm::vec3& direction, float radius, float maxDistance, RaycastHit& outHit)
    {
        // 2D equivalent: circle cast or raycast fallback
        return CastRay(origin, direction, maxDistance, outHit);
    }
    ICharacterController* Box2DPhysicsSystem::CreateCharacterController(const CharacterControllerDesc& /*desc*/)
    {
        return nullptr;
    }

    void Box2DPhysicsSystem::DestroyCharacterController(ICharacterController* /*controller*/)
    {
    }
}
