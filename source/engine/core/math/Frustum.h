// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/math/Math.h"
#include <cmath>
#include <algorithm>

namespace lacrima
{
    // Axis-Aligned Bounding Box (AABB) in 3D world space.
    struct AABB
    {
        Vec3 min{ 0.0f, 0.0f, 0.0f };
        Vec3 max{ 0.0f, 0.0f, 0.0f };

        AABB() = default;
        AABB(const Vec3& inMin, const Vec3& inMax) : min(inMin), max(inMax) {}

        Vec3 Center() const { return (min + max) * 0.5f; }
        Vec3 Extents() const { return (max - min) * 0.5f; }
        Vec3 Size() const { return max - min; }

        bool Contains(const Vec3& p) const
        {
            return (p.x >= min.x && p.x <= max.x &&
                    p.y >= min.y && p.y <= max.y &&
                    p.z >= min.z && p.z <= max.z);
        }

        bool Intersects(const AABB& o) const
        {
            return (min.x <= o.max.x && max.x >= o.min.x &&
                    min.y <= o.max.y && max.y >= o.min.y &&
                    min.z <= o.max.z && max.z >= o.min.z);
        }
    };

    // 3D Plane represented by normal (A, B, C) and signed distance D (Ax + By + Cz + D = 0).
    struct Plane
    {
        Vec3 normal{ 0.0f, 1.0f, 0.0f };
        f32 distance = 0.0f;

        Plane() = default;
        Plane(const Vec3& n, f32 d) : normal(n), distance(d) {}
        Plane(f32 a, f32 b, f32 c, f32 d) : normal(a, b, c), distance(d) {}

        f32 SignedDistance(const Vec3& p) const
        {
            return normal.Dot(p) + distance;
        }

        void Normalize()
        {
            f32 len = normal.Length();
            if (len > 1e-8f)
            {
                f32 inv = 1.0f / len;
                normal = normal * inv;
                distance *= inv;
            }
        }
    };

    // 6-Plane Viewing Frustum for 3D visibility testing and spatial culling.
    class Frustum
    {
    public:
        enum PlaneIndex
        {
            Left = 0,
            Right,
            Bottom,
            Top,
            Near,
            Far,
            Count = 6
        };

        Plane planes[Count];

        Frustum() = default;

        // Extracts the 6 frustum planes from a column-major View-Projection matrix
        // using the Gribb-Hartmann method (matching Vulkan clip space [0, 1] depth).
        static Frustum FromViewProjection(const Mat4& vp)
        {
            Frustum f;
            const f32* m = vp.m; // column-major: row i, col j is m[j*4 + i]
            
            // Left:   row3 + row0
            f.planes[Left]   = Plane(m[3] + m[0], m[7] + m[4], m[11] + m[8],  m[15] + m[12]);
            // Right:  row3 - row0
            f.planes[Right]  = Plane(m[3] - m[0], m[7] - m[4], m[11] - m[8],  m[15] - m[12]);
            // Bottom: row3 + row1
            f.planes[Bottom] = Plane(m[3] + m[1], m[7] + m[5], m[11] + m[9],  m[15] + m[13]);
            // Top:    row3 - row1
            f.planes[Top]    = Plane(m[3] - m[1], m[7] - m[5], m[11] - m[9],  m[15] - m[13]);
            // Near:   row2 (for Vulkan depth [0, 1])
            f.planes[Near]   = Plane(m[2],        m[6],        m[10],         m[14]);
            // Far:    row3 - row2
            f.planes[Far]    = Plane(m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]);

            for (u32 i = 0; i < Count; ++i)
            {
                f.planes[i].Normalize();
            }

            return f;
        }

        // Tests if an AABB is inside or intersecting the frustum.
        // Returns false if the AABB is completely outside any of the 6 planes.
        bool Intersects(const AABB& aabb) const
        {
            const Vec3 center = aabb.Center();
            const Vec3 extents = aabb.Extents();

            for (u32 i = 0; i < Count; ++i)
            {
                const Plane& p = planes[i];
                // Compute projected radius on plane normal
                f32 r = extents.x * std::abs(p.normal.x) +
                        extents.y * std::abs(p.normal.y) +
                        extents.z * std::abs(p.normal.z);

                if (p.SignedDistance(center) < -r)
                {
                    return false; // Outside this plane
                }
            }
            return true;
        }

        // Tests if a sphere (center + radius) is inside or intersecting the frustum.
        bool Intersects(const Vec3& center, f32 radius) const
        {
            for (u32 i = 0; i < Count; ++i)
            {
                if (planes[i].SignedDistance(center) < -radius)
                {
                    return false;
                }
            }
            return true;
        }
    };
}
