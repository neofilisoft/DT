// Copyright Neofilisoft. All Rights Reserved.
#include "VehicleSystem.h"

#include "simulation/world/SimulationWorld.h"
#include <physics/IPhysicsSystem.h>
#include <physics/IVehicleController.h>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

namespace lacrima::sim
{
    // -----------------------------------------------------------------------
    // BuildDefaultCarDesc - construct a standard 4-wheel car layout
    //
    // Wheel layout (vehicle-local space, Y-up):
    //
    //       FL (+x, +z)   FR (-x, +z)    <- front (canSteer=true)
    //       RL (+x, -z)   RR (-x, -z)    <- rear  (handBrake=true)
    //
    // Suspension hangs downward from wheel attachment points.
    // -----------------------------------------------------------------------
    physics::VehicleDesc VehicleSystem::BuildDefaultCarDesc(const VehicleComponent& comp,
                                                             const glm::vec3& spawnPos,
                                                             const glm::quat& spawnRot)
    {
        physics::VehicleDesc desc{};
        desc.position         = spawnPos;
        desc.rotation         = spawnRot;
        desc.halfExtents      = comp.halfExtents;
        desc.mass             = comp.mass;
        desc.maxEngineTorque  = comp.maxEngineTorque;
        desc.maxBrakeTorque   = comp.maxBrakeTorque;
        desc.maxSteerAngle    = comp.maxSteerAngle;
        desc.differentialType = comp.differentialType;

        // Axle offsets relative to body half-extents
        const float axleX    = comp.halfExtents.x * 0.9f;   // lateral
        const float frontZ   = comp.halfExtents.z * 0.75f;  // front axle
        const float rearZ    = comp.halfExtents.z * 0.75f;  // rear axle
        const float wheelY   = -comp.halfExtents.y;         // at bottom of body

        auto makeWheel = [&](float x, float z, bool steer, bool rear) -> physics::WheelDesc
        {
            physics::WheelDesc w{};
            w.positionLocal         = glm::vec3(x, wheelY, z);
            w.radius                = 0.35f;
            w.width                 = 0.22f;
            w.suspensionMinLength   = 0.05f;
            w.suspensionMaxLength   = 0.35f;
            w.suspensionFrequency   = 1.5f;
            w.suspensionDamping     = 0.5f;
            w.maxSteerAngle         = steer ? comp.maxSteerAngle : 0.0f;
            w.canSteer              = steer;
            w.canDrive              = true;
            w.handBrake             = rear;
            w.maxHandBrakeTorque    = rear ? 4000.0f : 0.0f;
            return w;
        };

        // FL, FR, RL, RR
        desc.wheels.push_back(makeWheel( axleX,  frontZ, true,  false));
        desc.wheels.push_back(makeWheel(-axleX,  frontZ, true,  false));
        desc.wheels.push_back(makeWheel( axleX, -rearZ,  false, true));
        desc.wheels.push_back(makeWheel(-axleX, -rearZ,  false, true));

        return desc;
    }

    // -----------------------------------------------------------------------
    // Update - main ECS tick (called from SimulationWorld tick graph)
    // -----------------------------------------------------------------------
    void VehicleSystem::Update(SimulationWorld& world, float dt)
    {
        auto* physics = world.GetPhysicsSystem();
        if (!physics) return;

        auto& vehicles   = world.Vehicles();
        auto& transforms = world.Transforms();

        vehicles.ForEach([&](Entity entity, VehicleComponent& vc)
        {
            TransformComponent* tc = transforms.Get(entity);
            if (!tc) return;

            // --- Lazy creation ---
            if (!vc.controller)
            {
                physics::VehicleDesc desc = BuildDefaultCarDesc(vc,
                                                         glm::vec3(tc->x, tc->y, tc->z),
                                                         glm::quat(glm::vec3(0.0f, tc->yaw, 0.0f)));
                vc.controller = physics->CreateVehicleController(desc);
                if (!vc.controller) return;
            }

            // --- Feed driver input ---
            vc.controller->SetDriverInput(vc.inputForward, vc.inputRight,
                                           vc.inputBrake, vc.inputHandBrake);

            // --- Read back transform ---
            glm::vec3 pos = vc.controller->GetPosition();
            tc->x = pos.x; tc->y = pos.y; tc->z = pos.z;
            // Extract yaw from rotation quaternion
            glm::quat rot = vc.controller->GetRotation();
            tc->yaw = std::atan2(2.0f * (rot.w * rot.y + rot.x * rot.z),
                                  1.0f - 2.0f * (rot.y * rot.y + rot.z * rot.z));
            vc.speedKmh  = vc.controller->GetSpeedKmh();
        });
    }

    // -----------------------------------------------------------------------
    // DestroyController - cleanup on entity destroy / PIE Stop
    // -----------------------------------------------------------------------
    void VehicleSystem::DestroyController(SimulationWorld& world, Entity entity)
    {
        auto* physics = world.GetPhysicsSystem();
        if (!physics) return;

        VehicleComponent* vc = world.Vehicles().Get(entity);
        if (!vc || !vc->controller) return;

        physics->DestroyVehicleController(vc->controller);
        vc->controller = nullptr;
    }
}


