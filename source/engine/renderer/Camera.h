#pragma once

#include "core/math/Math.h"

namespace dt::renderer
{
    // A standalone, renderer-driven camera system.
    // Handles calculation of View and Projection matrices based on input or programmatic control.
    class Camera
    {
    public:
        Camera();
        ~Camera() = default;

        void SetPerspective(f32 fovYRadians, f32 aspectRatio, f32 nearZ, f32 farZ);
        
        // Positions the camera looking at a specific target
        void LookAt(const Vec3& eye, const Vec3& target, const Vec3& up);

        // Sets position and rotation angles (pitch/yaw)
        void SetTransform(const Vec3& position, f32 pitch, f32 yaw);

        // Getters
        const Mat4& GetViewMatrix() const { return m_viewMatrix; }
        const Mat4& GetProjectionMatrix() const { return m_projectionMatrix; }
        const Vec3& GetPosition() const { return m_position; }
        
        // Updates the view matrix if the position or rotation changed
        void Update();

    private:
        Mat4 m_viewMatrix;
        Mat4 m_projectionMatrix;

        Vec3 m_position;
        f32 m_pitch; // Rotation around X-axis
        f32 m_yaw;   // Rotation around Y-axis
        
        bool m_isDirty = true;
    };
}
