#pragma once

#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"

#include <cmath>

// ---------------------------------------------------------------------------
// Math.h
//
// Minimal, dependency-free math primitives (Vec2/Vec3/Vec4/Quat/Mat4).
// Deliberately NOT wrapping an external math library (GLM, etc.) despite
// GLM being a very common choice, for one concrete reason specific to this
// engine: the Reflection system (core/reflection) needs to generate
// TypeInfo for every serializable field type, including math types used in
// simulation state (a Sim's world position, a Lot's bounds). Reflecting
// into a third-party header we don't control is fragile - GLM's types are
// template aliases over glm::vec<L, T, Q> that don't present a stable,
// reflectable struct layout across GLM versions. Owning these types means
// REFLECT_FIELD(position) on a Vec3 always sees exactly the three named
// floats we declared, forever, regardless of any dependency upgrade.
//
// This is a deliberately small tradeoff: a 1-3 developer team occasionally
// hand-writing a missing vector op is a lower long-term cost than
// debugging a reflection break caused by an upstream math library's
// internal representation changing.
// ---------------------------------------------------------------------------

namespace dt
{
    struct Vec2
    {
        f32 x = 0.0f, y = 0.0f;

        Vec2() = default;
        Vec2(f32 inX, f32 inY) : x(inX), y(inY) {}

        Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
        Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
        Vec2 operator*(f32 s) const { return { x * s, y * s }; }
        Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
        Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }

        f32 Dot(const Vec2& o) const { return x * o.x + y * o.y; }
        f32 LengthSq() const { return Dot(*this); }
        f32 Length() const { return std::sqrt(LengthSq()); }

        Vec2 Normalized() const
        {
            const f32 len = Length();
            return (len > 1e-8f) ? Vec2(x / len, y / len) : Vec2(0.0f, 0.0f);
        }
    };

    struct Vec3
    {
        f32 x = 0.0f, y = 0.0f, z = 0.0f;

        Vec3() = default;
        Vec3(f32 inX, f32 inY, f32 inZ) : x(inX), y(inY), z(inZ) {}

        Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
        Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
        Vec3 operator*(f32 s) const { return { x * s, y * s, z * s }; }
        Vec3 operator-() const { return { -x, -y, -z }; }
        Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
        Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }

        f32 Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }

        Vec3 Cross(const Vec3& o) const
        {
            return {
                y * o.z - z * o.y,
                z * o.x - x * o.z,
                x * o.y - y * o.x
            };
        }

        f32 LengthSq() const { return Dot(*this); }
        f32 Length() const { return std::sqrt(LengthSq()); }

        Vec3 Normalized() const
        {
            const f32 len = Length();
            return (len > 1e-8f) ? Vec3(x / len, y / len, z / len) : Vec3(0.0f, 0.0f, 0.0f);
        }

        static Vec3 Lerp(const Vec3& a, const Vec3& b, f32 t)
        {
            return a + (b - a) * t;
        }
    };

    struct Vec4
    {
        f32 x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

        Vec4() = default;
        Vec4(f32 inX, f32 inY, f32 inZ, f32 inW) : x(inX), y(inY), z(inZ), w(inW) {}
        explicit Vec4(const Vec3& v, f32 inW = 1.0f) : x(v.x), y(v.y), z(v.z), w(inW) {}
    };

    // Hamilton convention (w, x, y, z internally stored as x,y,z,w to match
    // Vec4 layout for reflection/serialization consistency across all
    // 4-float types).
    struct Quat
    {
        f32 x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;

        Quat() = default;
        Quat(f32 inX, f32 inY, f32 inZ, f32 inW) : x(inX), y(inY), z(inZ), w(inW) {}

        static Quat FromAxisAngle(const Vec3& axis, f32 radians)
        {
            const Vec3 n = axis.Normalized();
            const f32 halfAngle = radians * 0.5f;
            const f32 s = std::sin(halfAngle);
            return Quat(n.x * s, n.y * s, n.z * s, std::cos(halfAngle));
        }

        Quat operator*(const Quat& o) const
        {
            return Quat(
                w * o.x + x * o.w + y * o.z - z * o.y,
                w * o.y - x * o.z + y * o.w + z * o.x,
                w * o.z + x * o.y - y * o.x + z * o.w,
                w * o.w - x * o.x - y * o.y - z * o.z
            );
        }

        Vec3 RotateVector(const Vec3& v) const
        {
            // Standard quaternion-vector rotation via q * v * q^-1, expanded
            // to avoid constructing a full quaternion-as-vec4 for `v`.
            const Vec3 qv(x, y, z);
            const Vec3 t = qv.Cross(v) * 2.0f;
            return v + t * w + qv.Cross(t);
        }

        static Quat Slerp(const Quat& a, const Quat& b, f32 t)
        {
            f32 cosHalfTheta = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
            Quat bAdj = b;
            if (cosHalfTheta < 0.0f)
            {
                bAdj = Quat(-b.x, -b.y, -b.z, -b.w);
                cosHalfTheta = -cosHalfTheta;
            }

            if (cosHalfTheta > 0.9995f)
            {
                // Nearly parallel: linear interpolation avoids a
                // divide-by-near-zero in the sin(theta) denominator below.
                Quat result(
                    a.x + (bAdj.x - a.x) * t,
                    a.y + (bAdj.y - a.y) * t,
                    a.z + (bAdj.z - a.z) * t,
                    a.w + (bAdj.w - a.w) * t
                );
                const f32 len = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w);
                return Quat(result.x / len, result.y / len, result.z / len, result.w / len);
            }

            const f32 halfTheta = std::acos(cosHalfTheta);
            const f32 sinHalfTheta = std::sin(halfTheta);
            const f32 ratioA = std::sin((1.0f - t) * halfTheta) / sinHalfTheta;
            const f32 ratioB = std::sin(t * halfTheta) / sinHalfTheta;

            return Quat(
                a.x * ratioA + bAdj.x * ratioB,
                a.y * ratioA + bAdj.y * ratioB,
                a.z * ratioA + bAdj.z * ratioB,
                a.w * ratioA + bAdj.w * ratioB
            );
        }
    };

    // Column-major 4x4, matching Vulkan/DirectX12 convention (both APIs
    // expect column-major by default in their respective shader languages'
    // native matrix layout, so storing column-major here means the
    // renderer's per-frame matrix upload is a direct memcpy with no
    // transpose step needed at the GPU upload boundary).
    struct Mat4
    {
        f32 m[16] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };

        static Mat4 Identity() { return Mat4(); }

        static Mat4 Translation(const Vec3& t)
        {
            Mat4 result;
            result.m[12] = t.x;
            result.m[13] = t.y;
            result.m[14] = t.z;
            return result;
        }

        static Mat4 FromQuat(const Quat& q)
        {
            Mat4 result;
            const f32 xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
            const f32 xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
            const f32 wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

            result.m[0] = 1.0f - 2.0f * (yy + zz);
            result.m[1] = 2.0f * (xy + wz);
            result.m[2] = 2.0f * (xz - wy);

            result.m[4] = 2.0f * (xy - wz);
            result.m[5] = 1.0f - 2.0f * (xx + zz);
            result.m[6] = 2.0f * (yz + wx);

            result.m[8] = 2.0f * (xz + wy);
            result.m[9] = 2.0f * (yz - wx);
            result.m[10] = 1.0f - 2.0f * (xx + yy);

            return result;
        }

        Mat4 operator*(const Mat4& o) const
        {
            Mat4 result;
            for (int col = 0; col < 4; ++col)
            {
                for (int row = 0; row < 4; ++row)
                {
                    f32 sum = 0.0f;
                    for (int k = 0; k < 4; ++k)
                    {
                        sum += m[k * 4 + row] * o.m[col * 4 + k];
                    }
                    result.m[col * 4 + row] = sum;
                }
            }
            return result;
        }

        // Right-handed perspective projection with [0, 1] depth range
        // (matching Vulkan/DX12's clip-space convention, unlike OpenGL's
        // [-1, 1] range) - since Vulkan is one of the two target graphics
        // APIs, using a [-1,1]-range projection here would require an
        // extra correction matrix multiply per draw, which we avoid by
        // building the right convention in from the start.
        static Mat4 Perspective(f32 fovYRadians, f32 aspectRatio, f32 nearZ, f32 farZ)
        {
            Mat4 result;
            for (int i = 0; i < 16; ++i) result.m[i] = 0.0f;

            const f32 tanHalfFovY = std::tan(fovYRadians * 0.5f);
            result.m[0] = 1.0f / (aspectRatio * tanHalfFovY);
            result.m[5] = 1.0f / tanHalfFovY;
            result.m[10] = farZ / (nearZ - farZ);
            result.m[11] = -1.0f;
            result.m[14] = -(farZ * nearZ) / (farZ - nearZ);
            return result;
        }

        static Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
        {
            const Vec3 f = (center - eye).Normalized();
            const Vec3 s = f.Cross(up).Normalized();
            const Vec3 u = s.Cross(f);

            Mat4 result;
            result.m[0] = s.x;
            result.m[4] = s.y;
            result.m[8] = s.z;
            result.m[12] = -s.Dot(eye);

            result.m[1] = u.x;
            result.m[5] = u.y;
            result.m[9] = u.z;
            result.m[13] = -u.Dot(eye);

            result.m[2] = -f.x;
            result.m[6] = -f.y;
            result.m[10] = -f.z;
            result.m[14] = f.Dot(eye);

            result.m[3] = 0.0f;
            result.m[7] = 0.0f;
            result.m[11] = 0.0f;
            result.m[15] = 1.0f;
            return result;
        }

        static Mat4 Orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 nearZ, f32 farZ)
        {
            Mat4 result;
            for (int i = 0; i < 16; ++i) result.m[i] = 0.0f;

            result.m[0] = 2.0f / (right - left);
            result.m[5] = 2.0f / (top - bottom);
            result.m[10] = 1.0f / (nearZ - farZ);
            result.m[12] = -(right + left) / (right - left);
            result.m[13] = -(top + bottom) / (top - bottom);
            result.m[14] = nearZ / (nearZ - farZ);
            result.m[15] = 1.0f;
            return result;
        }
    };

    namespace math
    {
        constexpr f32 kPi = 3.14159265358979323846f;

        inline f32 DegToRad(f32 degrees) { return degrees * (kPi / 180.0f); }
        inline f32 RadToDeg(f32 radians) { return radians * (180.0f / kPi); }

        inline f32 Clamp(f32 value, f32 lo, f32 hi)
        {
            return value < lo ? lo : (value > hi ? hi : value);
        }

        inline f32 Lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
    }

    // Reflection field-type mappings for math primitives. These live here
    // rather than in Reflection.h itself because Reflection.h is a Core
    // leaf header with no knowledge of Vec2/Vec3/etc; Math.h is the module
    // that knows both sides, so it is the correct place to bridge them.
    template <> struct DT_FieldTypeOf<Vec2> { static constexpr FieldType value = FieldType::Vec2; };
    template <> struct DT_FieldTypeOf<Vec3> { static constexpr FieldType value = FieldType::Vec3; };
    template <> struct DT_FieldTypeOf<Vec4> { static constexpr FieldType value = FieldType::Vec4; };
    template <> struct DT_FieldTypeOf<Quat> { static constexpr FieldType value = FieldType::Quat; };
    template <> struct DT_FieldTypeOf<Mat4> { static constexpr FieldType value = FieldType::Mat4; };
}
