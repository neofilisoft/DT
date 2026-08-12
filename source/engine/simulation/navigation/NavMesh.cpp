#include "NavMesh.h"
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourNavMeshBuilder.h>
#include "core/platform/Assert.h"
#include <utility>

namespace dt::sim
{
    NavMesh::NavMesh() = default;

    NavMesh::~NavMesh()
    {
        if (m_navQuery)
        {
            dtFreeNavMeshQuery(m_navQuery);
        }
        if (m_navMesh)
        {
            dtFreeNavMesh(m_navMesh);
        }
    }

    NavMesh::NavMesh(NavMesh&& other) noexcept
        : m_navMesh(std::exchange(other.m_navMesh, nullptr)),
          m_navQuery(std::exchange(other.m_navQuery, nullptr))
    {
    }

    NavMesh& NavMesh::operator=(NavMesh&& other) noexcept
    {
        if (this != &other)
        {
            if (m_navQuery) dtFreeNavMeshQuery(m_navQuery);
            if (m_navMesh) dtFreeNavMesh(m_navMesh);
            m_navMesh = std::exchange(other.m_navMesh, nullptr);
            m_navQuery = std::exchange(other.m_navQuery, nullptr);
        }
        return *this;
    }

    bool NavMesh::InitializeFromDetour(dtNavMesh* navMesh)
    {
        if (m_navQuery) dtFreeNavMeshQuery(m_navQuery);
        if (m_navMesh) dtFreeNavMesh(m_navMesh);

        m_navMesh = navMesh;
        if (!m_navMesh) return false;

        m_navQuery = dtAllocNavMeshQuery();
        if (!m_navQuery) return false;

        // Initialize the query with the navmesh. Up to 2048 nodes in the search space.
        dtStatus status = m_navQuery->init(m_navMesh, 2048);
        if (dtStatusFailed(status))
        {
            dtFreeNavMeshQuery(m_navQuery);
            m_navQuery = nullptr;
            return false;
        }

        return true;
    }

    bool NavMesh::CreateTestNavMesh()
    {
        std::printf("CreateTestNavMesh: Start\n"); std::fflush(stdout);
        // Creates a simple 1-polygon navmesh for testing (a 100x100 square)
        dtNavMeshCreateParams params{};
        
        // Vertices (quantized to cs/ch)
        unsigned short verts[] = {
            0, 0, 0,
            500, 0, 0,
            500, 0, 500,
            0, 0, 500
        };
        
        // Polygons (indices to verts + adjacency info, size = polyCount * 2 * nvp)
        // Adjacency is 0xffff for no neighbor.
        unsigned short polys[] = {
            0, 1, 2, 3,
            0xffff, 0xffff, 0xffff, 0xffff
        };

        unsigned char polyAreas[] = { 1 };
        unsigned short polyFlags[] = { 1 };

        params.verts = verts;
        params.vertCount = 4;
        params.polys = polys;
        params.polyAreas = polyAreas;
        params.polyFlags = polyFlags;
        params.polyCount = 1;
        params.nvp = 4; // Max vertices per polygon

        params.walkableHeight = 2.0f;
        params.walkableRadius = 0.5f;
        params.walkableClimb = 0.5f;

        // Detail mesh (optional, we can leave empty for simple flat meshes)
        params.detailMeshes = nullptr;
        params.detailVerts = nullptr;
        params.detailVertsCount = 0;
        params.detailTris = nullptr;
        params.detailTriCount = 0;

        // Bounds
        params.bmin[0] = -50.0f; params.bmin[1] = 0.0f; params.bmin[2] = -50.0f;
        params.bmax[0] =  50.0f; params.bmax[1] = 0.0f; params.bmax[2] =  50.0f;
        params.cs = 0.2f;
        params.ch = 0.2f;

        params.buildBvTree = true;

        unsigned char* navData = nullptr;
        int navDataSize = 0;

        std::printf("CreateTestNavMesh: Calling dtCreateNavMeshData\n"); std::fflush(stdout);
        if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
        {
            std::printf("CreateTestNavMesh: dtCreateNavMeshData failed\n"); std::fflush(stdout);
            return false;
        }

        std::printf("CreateTestNavMesh: Allocating mesh\n"); std::fflush(stdout);
        dtNavMesh* mesh = dtAllocNavMesh();
        if (!mesh)
        {
            dtFree(navData);
            return false;
        }

        std::printf("CreateTestNavMesh: Calling init\n"); std::fflush(stdout);
        dtStatus status = mesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
        if (dtStatusFailed(status))
        {
            std::printf("CreateTestNavMesh: init failed\n"); std::fflush(stdout);
            dtFree(navData);
            dtFreeNavMesh(mesh);
            return false;
        }

        std::printf("CreateTestNavMesh: InitializeFromDetour\n"); std::fflush(stdout);
        return InitializeFromDetour(mesh);
    }

    std::vector<Vec3> NavMesh::FindPath(const Vec3& start, const Vec3& end) const
    {
        std::vector<Vec3> path;
        if (!m_navQuery) return path;

        dtQueryFilter filter;
        filter.setIncludeFlags(0xffff);
        filter.setExcludeFlags(0);

        // Detour uses float[3]
        float startPos[3] = { start.x, start.y, start.z };
        float endPos[3] = { end.x, end.y, end.z };
        float extents[3] = { 2.0f, 4.0f, 2.0f };

        dtPolyRef startRef = 0;
        float startPt[3];
        m_navQuery->findNearestPoly(startPos, extents, &filter, &startRef, startPt);

        dtPolyRef endRef = 0;
        float endPt[3];
        m_navQuery->findNearestPoly(endPos, extents, &filter, &endRef, endPt);

        if (!startRef || !endRef)
        {
            return path; // Start or end not on navmesh
        }

        dtPolyRef pathPolys[256];
        int pathCount = 0;

        dtStatus status = m_navQuery->findPath(startRef, endRef, startPt, endPt, &filter, pathPolys, &pathCount, 256);
        if (dtStatusFailed(status) || pathCount == 0)
        {
            return path;
        }

        // Find the actual points along the path polygons
        float pathPts[256 * 3];
        unsigned char pathFlags[256];
        dtPolyRef pathPolysFinal[256];
        int ptCount = 0;

        status = m_navQuery->findStraightPath(startPt, endPt, pathPolys, pathCount,
                                              pathPts, pathFlags, pathPolysFinal, &ptCount, 256);

        if (dtStatusSucceed(status) && ptCount > 0)
        {
            path.reserve(ptCount);
            for (int i = 0; i < ptCount; ++i)
            {
                path.push_back(Vec3(pathPts[i * 3], pathPts[i * 3 + 1], pathPts[i * 3 + 2]));
            }
        }

        return path;
    }

    bool NavMesh::FindNearestPoint(const Vec3& point, const Vec3& searchExtents, Vec3& outNearest) const
    {
        if (!m_navQuery) return false;

        dtQueryFilter filter;
        filter.setIncludeFlags(0xffff);
        filter.setExcludeFlags(0);

        float pos[3] = { point.x, point.y, point.z };
        float extents[3] = { searchExtents.x, searchExtents.y, searchExtents.z };

        dtPolyRef ref = 0;
        float nearest[3];
        dtStatus status = m_navQuery->findNearestPoly(pos, extents, &filter, &ref, nearest);

        if (dtStatusSucceed(status) && ref != 0)
        {
            outNearest = Vec3(nearest[0], nearest[1], nearest[2]);
            return true;
        }
        return false;
    }
}
