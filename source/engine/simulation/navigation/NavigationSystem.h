#pragma once
#include "NavMesh.h"
#include "NavAgentComponent.h"
#include "simulation/spatial/TransformComponent.h"

namespace dt::sim
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
        std::vector<dt::Vec3> FindPath(const dt::Vec3& start, const dt::Vec3& end) const;

        /**
         * @brief Update agents
         */
        void StepNavigation(SimulationWorld* world, float dt);

    private:
        NavMesh m_navMesh;
    };
}
