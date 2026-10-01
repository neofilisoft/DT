#include "editor/scene/EditorCamera.h"
#include <algorithm>
#include <cmath>

namespace lacrima::editor
{
    EditorCamera::EditorCamera()
    {
        UpdateProjectionMatrix();
        UpdateViewMatrix();
    }

    EditorCamera::EditorCamera(float fovDegrees, float aspectRatio, float nearZ, float farZ)
        : m_fov(math::DegToRad(fovDegrees))
        , m_aspectRatio(aspectRatio > 0.0f ? aspectRatio : (16.0f / 9.0f))
        , m_nearZ(nearZ)
        , m_farZ(farZ)
    {
        UpdateProjectionMatrix();
        UpdateViewMatrix();
    }

    void EditorCamera::SetViewportSize(float width, float height)
    {
        if (width <= 0.0f || height <= 0.0f)
            return;

        float newAspect = width / height;
        if (std::abs(m_aspectRatio - newAspect) > 0.0001f)
        {
            m_aspectRatio = newAspect;
            m_projDirty = true;
        }
    }

    void EditorCamera::SetPerspective(float fovDegrees, float nearZ, float farZ)
    {
        m_fov = math::DegToRad(fovDegrees);
        m_nearZ = nearZ;
        m_farZ = farZ;
        m_projDirty = true;
    }

    void EditorCamera::OnMouseMove(float deltaX, float deltaY, bool isOrbiting, bool isPanning)
    {
        if (isOrbiting)
        {
            // Rotate around focal point
            m_yaw += deltaX * m_orbitSpeed;
            m_pitch -= deltaY * m_orbitSpeed;

            // Clamp pitch to prevent gimbal flip (-89 to +89 degrees)
            const float kMaxPitch = math::DegToRad(89.0f);
            m_pitch = math::Clamp(m_pitch, -kMaxPitch, kMaxPitch);

            m_viewDirty = true;
        }
        else if (isPanning)
        {
            // Pan moves focal point perpendicular to view direction
            float panScale = m_panSpeed * (m_distance * 0.25f);
            Vec3 right = GetRight();
            Vec3 up = GetUp();

            m_focalPoint = m_focalPoint - (right * (deltaX * panScale)) + (up * (deltaY * panScale));
            m_viewDirty = true;
        }
    }

    void EditorCamera::OnMouseScroll(float scrollDelta)
    {
        if (std::abs(scrollDelta) < 0.0001f)
            return;

        // Smooth zoom: factor scaled by distance
        float zoomFactor = 1.0f - (scrollDelta * 0.1f * m_zoomSpeed);
        m_distance *= zoomFactor;
        m_distance = math::Clamp(m_distance, 0.2f, 2000.0f);
        m_viewDirty = true;
    }

    void EditorCamera::SetFocalPoint(const Vec3& focalPoint)
    {
        m_focalPoint = focalPoint;
        m_viewDirty = true;
    }

    void EditorCamera::SetDistance(float distance)
    {
        m_distance = math::Clamp(distance, 0.2f, 2000.0f);
        m_viewDirty = true;
    }

    void EditorCamera::SetRotation(float pitchRadians, float yawRadians)
    {
        const float kMaxPitch = math::DegToRad(89.0f);
        m_pitch = math::Clamp(pitchRadians, -kMaxPitch, kMaxPitch);
        m_yaw = yawRadians;
        m_viewDirty = true;
    }

    void EditorCamera::Update()
    {
        if (m_projDirty)
        {
            UpdateProjectionMatrix();
        }
        if (m_viewDirty)
        {
            UpdateViewMatrix();
        }
    }

    Vec3 EditorCamera::GetPosition() const
    {
        Vec3 forward = GetForward();
        return m_focalPoint - (forward * m_distance);
    }

    Vec3 EditorCamera::GetForward() const
    {
        float cosPitch = std::cos(m_pitch);
        float sinPitch = std::sin(m_pitch);
        float cosYaw = std::cos(m_yaw);
        float sinYaw = std::sin(m_yaw);

        // Looking towards focal point in right-handed coords
        return Vec3(cosPitch * sinYaw, sinPitch, cosPitch * cosYaw).Normalized();
    }

    Vec3 EditorCamera::GetRight() const
    {
        Vec3 forward = GetForward();
        Vec3 worldUp(0.0f, 1.0f, 0.0f);
        return forward.Cross(worldUp).Normalized();
    }

    Vec3 EditorCamera::GetUp() const
    {
        Vec3 forward = GetForward();
        Vec3 right = GetRight();
        return right.Cross(forward).Normalized();
    }

    void EditorCamera::UpdateViewMatrix()
    {
        Vec3 position = GetPosition();
        Vec3 up = GetUp();
        m_viewMatrix = Mat4::LookAt(position, m_focalPoint, up);
        m_viewDirty = false;
    }

    void EditorCamera::UpdateProjectionMatrix()
    {
        m_projMatrix = Mat4::Perspective(m_fov, m_aspectRatio, m_nearZ, m_farZ);
        m_projDirty = false;
    }
}
