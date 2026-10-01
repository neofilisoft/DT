// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include "core/math/Math.h"
#include "core/reflection/Reflection.h"
#include <vector>

namespace lacrima::sim
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

        REFLECT_BEGIN(NavAgentComponent)
            REFLECT_FIELD_ARRAY(currentPath)
            REFLECT_FIELD(currentWaypointIndex)
            REFLECT_FIELD(moveSpeed)
            REFLECT_FIELD(pathRadius)
            REFLECT_FIELD(hasPath)
        REFLECT_END()
    };
}

