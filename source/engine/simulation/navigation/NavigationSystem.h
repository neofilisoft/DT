// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include "NavMesh.h"
#include "NavAgentComponent.h"
#include "simulation/spatial/TransformComponent.h"

namespace lacrima::sim
{
    class SimulationWorld;

    /**
     * @brief System responsible for ticking navigation agents along their paths.
     */
    class NavigationSystem
    {
    public:
        NavigationSystem() = default;

        void Initialize(SimulationWorld* world);
        
        // Sets up a basic test nav mesh
        void CreateTestNavMesh();

        // Query the navmesh
        const NavMesh& GetNavMesh() const { return m_navMesh; }

        /**
         * @brief Request a path. (Synchronous for now, can be made async)
         */
        std::vector<lacrima::Vec3> FindPath(const lacrima::Vec3& start, const lacrima::Vec3& end) const;

        /**
         * @brief Casts a ray on the navmesh from start to end.
         * @return True if the ray hit a wall/boundary (line of sight is blocked), false if the path is clear.
         */
        bool Raycast(const lacrima::Vec3& start, const lacrima::Vec3& end, lacrima::Vec3& outHitPosition) const;

        /**
         * @brief Update agents
         */
        void StepNavigation(SimulationWorld* world, float dt);

    private:
        NavMesh m_navMesh;
    };
}

