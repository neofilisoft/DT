#include "NavigationSystem.h"
#include "simulation/world/SimulationWorld.h"
#include <cmath>

namespace dt::sim
{
    void NavigationSystem::Initialize(SimulationWorld* world)
    {
        // Add component arrays to world if needed, usually handled by SimulationWorld itself
    }

    void NavigationSystem::CreateTestNavMesh()
    {
        m_navMesh.CreateTestNavMesh();
    }

    std::vector<Vec3> NavigationSystem::FindPath(const Vec3& start, const Vec3& end) const
    {
        return m_navMesh.FindPath(start, end);
    }

    void NavigationSystem::StepNavigation(SimulationWorld* world, float dt)
    {
        auto& agents = world->NavAgents();
        auto& transforms = world->Transforms();

        agents.ForEach([&](Handle<EntityTag> entity, NavAgentComponent& agent) {
            if (!agent.hasPath) return;
            if (agent.currentWaypointIndex >= agent.currentPath.size())
            {
                agent.ClearPath();
                return;
            }

            TransformComponent* transform = transforms.Get(entity);
            if (!transform) return;

            Vec3 targetPos = agent.currentPath[agent.currentWaypointIndex];
            Vec3 currentPos(transform->x, transform->y, transform->z);

            Vec3 diff = targetPos - currentPos;
            float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

            // Arrived at waypoint?
            if (distSq <= agent.pathRadius * agent.pathRadius)
            {
                agent.currentWaypointIndex++;
                if (agent.currentWaypointIndex >= agent.currentPath.size())
                {
                    agent.ClearPath();
                }
                return; // Wait for next tick to move to next waypoint
            }

            // Move towards waypoint
            float dist = std::sqrt(distSq);
            Vec3 dir = diff * (1.0f / dist);

            // Simple Euler integration
            float moveStep = agent.moveSpeed * dt;
            if (moveStep > dist) moveStep = dist; // Don't overshoot

            currentPos = currentPos + dir * moveStep;
            transform->x = currentPos.x;
            transform->y = currentPos.y;
            transform->z = currentPos.z;
            
            // Optionally, update rotation to face direction
            // Note: In 3D we'd need atan2 for Y-axis rotation
        });
    }
}
