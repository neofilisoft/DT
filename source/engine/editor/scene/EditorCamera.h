#pragma once

#include "core/math/Math.h"

namespace lacrima::editor
{
    // A fully functional 3D Orbit/Pan/Zoom Editor Camera.
    // Calculates View and Projection matrices matching the active Viewport aspect ratio.
    class EditorCamera
    {
    public:
        EditorCamera();
        EditorCamera(float fovDegrees, float aspectRatio, float nearZ, float farZ);

        void SetViewportSize(float width, float height);
        void SetPerspective(float fovDegrees, float nearZ, float farZ);

        // Interactive mouse controls
        void OnMouseMove(float deltaX, float deltaY, bool isOrbiting, bool isPanning);
        void OnMouseScroll(float scrollDelta);

        // Explicit transform setup
        void SetFocalPoint(const Vec3& focalPoint);
        void SetDistance(float distance);
        void SetRotation(float pitchRadians, float yawRadians);

        // Frame update
        void Update();

        // Matrix and vector accessors
        const Mat4& GetViewMatrix() const { return m_viewMatrix; }
        const Mat4& GetProjectionMatrix() const { return m_projMatrix; }
        Vec3 GetPosition() const;
        const Vec3& GetFocalPoint() const { return m_focalPoint; }
        float GetDistance() const { return m_distance; }
        float GetPitch() const { return m_pitch; }
        float GetYaw() const { return m_yaw; }

        Vec3 GetForward() const;
        Vec3 GetRight() const;
        Vec3 GetUp() const;

    private:
        void UpdateViewMatrix();
        void UpdateProjectionMatrix();

        Mat4 m_viewMatrix = Mat4::Identity();
        Mat4 m_projMatrix = Mat4::Identity();

        Vec3 m_focalPoint{0.0f, 0.0f, 0.0f};
        float m_distance = 10.0f;
        float m_pitch = 0.35f;  // ~20 degrees down
        float m_yaw = 0.75f;    // ~43 degrees

        float m_fov = math::DegToRad(45.0f);
        float m_aspectRatio = 16.0f / 9.0f;
        float m_nearZ = 0.1f;
        float m_farZ = 1000.0f;

        float m_orbitSpeed = 0.005f;
        float m_panSpeed = 0.003f;
        float m_zoomSpeed = 0.8f;

        bool m_viewDirty = true;
        bool m_projDirty = true;
    };
}
