#pragma once

#include "core/math/Math.h"
#include <vector>

class dtNavMesh;
class dtNavMeshQuery;

namespace dt::sim
{
    /**
     * @brief Wrapper around Detour navigation mesh and query.
     */
    class NavMesh
    {
    public:
        NavMesh();
        ~NavMesh();

        // Non-copyable due to raw pointer ownership
        NavMesh(const NavMesh&) = delete;
        NavMesh& operator=(const NavMesh&) = delete;

        // Movable
        NavMesh(NavMesh&& other) noexcept;
        NavMesh& operator=(NavMesh&& other) noexcept;

        /**
         * @brief Takes ownership of a constructed dtNavMesh and initializes the query object.
         */
        bool InitializeFromDetour(dtNavMesh* navMesh);

        /**
         * @brief Creates a simple test navmesh (a single large quad/plane) for testing.
         */
        bool CreateTestNavMesh();

        /**
         * @brief Finds a path from start to end using A*.
         * @return A list of points forming the path. Empty if no path found.
         */
        std::vector<Vec3> FindPath(const Vec3& start, const Vec3& end) const;

        /**
         * @brief Snaps a point to the nearest valid point on the navmesh.
         */
        bool FindNearestPoint(const Vec3& point, const Vec3& searchExtents, Vec3& outNearest) const;

        dtNavMesh* GetDetourNavMesh() const { return m_navMesh; }
        dtNavMeshQuery* GetDetourQuery() const { return m_navQuery; }

    private:
        dtNavMesh* m_navMesh = nullptr;
        dtNavMeshQuery* m_navQuery = nullptr;
    };
}
