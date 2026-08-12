#pragma once
#include "core/math/Math.h"
#include <vector>

namespace dt::sim
{
    /**
     * @brief Component indicating an entity can navigate using the NavMesh.
     */
    struct NavAgentComponent
    {
        std::vector<Vec3> currentPath;
        size_t currentWaypointIndex = 0;
        
        float moveSpeed = 5.0f;
        float pathRadius = 0.5f; // Used for arrival checking

        bool hasPath = false;

        void SetPath(const std::vector<Vec3>& path)
        {
            currentPath = path;
            currentWaypointIndex = 0;
            hasPath = !currentPath.empty();
        }

        void ClearPath()
        {
            currentPath.clear();
            currentWaypointIndex = 0;
            hasPath = false;
        }
    };
}
