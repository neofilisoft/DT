// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/spatial/SpringArmSystem.h"
#include "physics/IPhysicsSystem.h"
#include <algorithm>
#include <cmath>

namespace lacrima::sim
{
    void SpringArmSystem::Update(
        f32 deltaTime,
        ComponentArray<Entity, TransformComponent>& transforms,
        ComponentArray<Entity, SpringArmComponent>& springArms,
        physics::IPhysicsSystem* physicsSystem)
    {
        if (springArms.Size() == 0) return;

        f32 dt = std::max(deltaTime, 0.0001f);

        springArms.ForEach([&transforms, physicsSystem, dt](Entity entity, SpringArmComponent& arm) {
            TransformComponent* transform = transforms.Get(entity);
            if (!transform) return;

            // 1. Character pivot in world space
            Vec3 characterPos(transform->x, transform->y, transform->z);
            Vec3 pivot = characterPos + arm.targetOffset;

            // 2. Camera rotation lag (frame-rate independent smoothing)
            f32 rotAlpha = arm.enableCameraLag ? (1.0f - std::exp(-arm.cameraLagSpeed * dt)) : 1.0f;
            arm.currentYaw = std::lerp(arm.currentYaw, arm.targetYaw, rotAlpha);
            arm.currentPitch = std::clamp(std::lerp(arm.currentPitch, arm.targetPitch, rotAlpha), -1.45f, 1.45f);

            // 3. Direction vector from yaw & pitch
            f32 cosPitch = std::cos(arm.currentPitch);
            Vec3 lookDir(
                cosPitch * std::sin(arm.currentYaw),
                std::sin(arm.currentPitch),
                cosPitch * std::cos(arm.currentYaw)
            );
            lookDir = lookDir.Normalized();

            // Camera sits behind character
            Vec3 backDir = lookDir * -1.0f;

            // Clamp desired arm length
            f32 desiredArmLength = std::clamp(arm.targetArmLength, arm.minArmLength, arm.maxArmLength);
            f32 targetDistance = desiredArmLength;

            // 4. Jolt Physics SphereCast collision query
            if (arm.doCollisionTest && physicsSystem)
            {
                physics::RaycastHit hit;
                glm::vec3 rayOrigin(pivot.x, pivot.y, pivot.z);
                glm::vec3 rayDir(backDir.x, backDir.y, backDir.z);

                bool hasCollision = physicsSystem->CastSphere(rayOrigin, rayDir, arm.probeRadius, desiredArmLength, hit);
                if (!hasCollision)
                {
                    hasCollision = physicsSystem->CastRay(rayOrigin, rayDir, desiredArmLength, hit);
                }

                if (hasCollision && hit.hasHit)
                {
                    f32 hitDist = hit.fraction * desiredArmLength - arm.collisionPadding;
                    targetDistance = std::max(arm.minArmLength, hitDist);
                }
            }

            // 5. Arm length retraction & extension smoothing
            if (targetDistance < arm.currentArmLength)
            {
                // Instant pull-in to guarantee camera NEVER clips into walls or terrain
                arm.currentArmLength = targetDistance;
            }
            else
            {
                // Smooth zoom-out when clear of obstacles
                f32 zoomAlpha = arm.enableCameraLag ? (1.0f - std::exp(-arm.cameraLagSpeed * 0.5f * dt)) : 1.0f;
                arm.currentArmLength = std::lerp(arm.currentArmLength, targetDistance, zoomAlpha);
            }

            // 6. Final camera world coordinates
            arm.computedCameraPosition = pivot + backDir * arm.currentArmLength;
            arm.computedLookAtTarget = pivot;
        });
    }
}