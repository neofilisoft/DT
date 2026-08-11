#include "renderer/Camera.h"

namespace dt::renderer
{
    Camera::Camera()
        : m_position(0.0f, 0.0f, 0.0f), m_pitch(0.0f), m_yaw(0.0f), m_isDirty(true)
    {
        m_viewMatrix = Mat4::Identity();
        m_projectionMatrix = Mat4::Identity();
    }

    void Camera::SetPerspective(f32 fovYRadians, f32 aspectRatio, f32 nearZ, f32 farZ)
    {
        m_projectionMatrix = Mat4::Perspective(fovYRadians, aspectRatio, nearZ, farZ);
    }

    void Camera::LookAt(const Vec3& eye, const Vec3& target, const Vec3& up)
    {
        m_position = eye;
        m_viewMatrix = Mat4::LookAt(eye, target, up);
        m_isDirty = false; // LookAt directly sets the view matrix
    }

    void Camera::SetTransform(const Vec3& position, f32 pitch, f32 yaw)
    {
        m_position = position;
        m_pitch = pitch;
        m_yaw = yaw;
        m_isDirty = true;
    }

    void Camera::Update()
    {
        if (!m_isDirty) return;

        // Calculate view matrix from pitch/yaw (using standard FPS camera math)
        // In a right-handed system:
        // x = cos(pitch) * sin(yaw)
        // y = sin(pitch)
        // z = cos(pitch) * cos(yaw)
        
        f32 cosPitch = std::cos(m_pitch);
        Vec3 forward(
            cosPitch * std::sin(m_yaw),
            std::sin(m_pitch),
            cosPitch * std::cos(m_yaw)
        );
        forward = forward.Normalized();

        Vec3 right = Vec3(0.0f, 1.0f, 0.0f).Cross(forward).Normalized();
        Vec3 up = forward.Cross(right).Normalized();

        m_viewMatrix = Mat4::LookAt(m_position, m_position + forward, up);
        m_isDirty = false;
    }
}
