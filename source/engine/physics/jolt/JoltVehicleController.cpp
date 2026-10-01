// Copyright Neofilisoft. All Rights Reserved.
#include "JoltVehicleController.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/Physics/Vehicle/VehicleDifferential.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

namespace lacrima::physics
{
    // ---------------------------------------------------------------------------
    // Helpers - glm <-> Jolt conversion
    // ---------------------------------------------------------------------------
    static JPH::Vec3 ToJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
    static JPH::Quat ToJolt(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
    static glm::vec3 ToGlm(const JPH::Vec3& v)  { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
    static glm::vec3 ToGlm(const JPH::RVec3& v) { return glm::vec3((float)v.GetX(), (float)v.GetY(), (float)v.GetZ()); }
    static glm::quat ToGlm(const JPH::Quat& q)  { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

    // Object layer (mirrors JoltPhysicsSystem.cpp)
    static constexpr JPH::ObjectLayer LAYER_MOVING = 1;

    // ---------------------------------------------------------------------------
    // Constructor - build Jolt body + vehicle constraint
    // ---------------------------------------------------------------------------
    JoltVehicleController::JoltVehicleController(const VehicleDesc& desc,
                                                  JPH::PhysicsSystem* physicsSystem,
                                                  JPH::TempAllocator* tempAllocator)
        : m_physicsSystem(physicsSystem)
        , m_tempAllocator(tempAllocator)
        , m_wheelCount(static_cast<u32>(desc.wheels.size()))
    {
        JPH_ASSERT(physicsSystem != nullptr);
        JPH_ASSERT(!desc.wheels.empty());

        JPH::BodyInterface& bi = physicsSystem->GetBodyInterface();

        // 1. Create vehicle body (box shape)
        float volume = 8.0f * desc.halfExtents.x * desc.halfExtents.y * desc.halfExtents.z;
        float density = (volume > 0.0f) ? desc.mass / volume : 1000.0f;

        JPH::BoxShapeSettings bodyShapeSettings(ToJolt(desc.halfExtents));
        bodyShapeSettings.mDensity = density;
        auto shapeResult = bodyShapeSettings.Create();

        JPH::BodyCreationSettings bodySettings(
            shapeResult.Get(),
            JPH::RVec3(desc.position.x, desc.position.y, desc.position.z),
            ToJolt(desc.rotation),
            JPH::EMotionType::Dynamic,
            LAYER_MOVING
        );
        bodySettings.mOverrideMassProperties     = JPH::EOverrideMassProperties::CalculateInertia;
        bodySettings.mMassPropertiesOverride.mMass = desc.mass;
        bodySettings.mUserData                   = reinterpret_cast<JPH::uint64>(desc.userData);
        bodySettings.mAllowSleeping              = false;
        bodySettings.mLinearDamping              = 0.1f;
        bodySettings.mAngularDamping             = 0.1f;

        m_bodyID = bi.CreateAndAddBody(bodySettings, JPH::EActivation::Activate);

        // 2. VehicleConstraintSettings
        JPH::VehicleConstraintSettings vehicleSettings;
        vehicleSettings.mUp      = JPH::Vec3::sAxisY();
        vehicleSettings.mForward = JPH::Vec3::sAxisZ();
        vehicleSettings.mMaxPitchRollAngle = JPH::DegreesToRadians(60.0f);

        // Collision tester (ray from wheel downward)
        JPH::Ref<JPH::VehicleCollisionTesterRay> tester =
            new JPH::VehicleCollisionTesterRay(LAYER_MOVING);
        vehicleSettings.SetVehicleCollisionTester(tester);

        // 3. Wheels
        for (const WheelDesc& wDesc : desc.wheels)
        {
            JPH::WheelSettingsWV* ws = new JPH::WheelSettingsWV();
            ws->mPosition            = ToJolt(wDesc.positionLocal);
            ws->mSuspensionDirection = JPH::Vec3(0.0f, -1.0f, 0.0f);
            ws->mRadius              = wDesc.radius;
            ws->mWidth               = wDesc.width;
            ws->mSuspensionMinLength = wDesc.suspensionMinLength;
            ws->mSuspensionMaxLength = wDesc.suspensionMaxLength;
            // mSuspensionSpring uses FrequencyAndDamping mode by default
            ws->mSuspensionSpring.mFrequency = wDesc.suspensionFrequency;
            ws->mSuspensionSpring.mDamping   = wDesc.suspensionDamping;
            ws->mMaxSteerAngle               = wDesc.canSteer ? wDesc.maxSteerAngle : 0.0f;
            ws->mMaxHandBrakeTorque          = wDesc.handBrake ? wDesc.maxHandBrakeTorque : 0.0f;
            vehicleSettings.mWheels.push_back(ws);
        }

        // 4. WheeledVehicleControllerSettings
        JPH::WheeledVehicleControllerSettings* ctrlSettings = new JPH::WheeledVehicleControllerSettings();
        ctrlSettings->mEngine.mMaxTorque = desc.maxEngineTorque;

        // Build differentials based on drive type
        // Differential: left=0/2, right=1/3 (FL=0, FR=1, RL=2, RR=3)
        if (desc.differentialType == 0) // FWD
        {
            JPH::VehicleDifferentialSettings diff;
            diff.mLeftWheel  = 0; // FL
            diff.mRightWheel = 1; // FR
            diff.mEngineTorqueRatio = 1.0f;
            ctrlSettings->mDifferentials.push_back(diff);
        }
        else if (desc.differentialType == 1) // RWD
        {
            JPH::VehicleDifferentialSettings diff;
            diff.mLeftWheel  = 2; // RL
            diff.mRightWheel = 3; // RR
            diff.mEngineTorqueRatio = 1.0f;
            ctrlSettings->mDifferentials.push_back(diff);
        }
        else // AWD (2 differentials)
        {
            JPH::VehicleDifferentialSettings diffFront, diffRear;
            diffFront.mLeftWheel  = 0; diffFront.mRightWheel = 1;
            diffFront.mEngineTorqueRatio = 0.5f;
            diffRear.mLeftWheel   = 2; diffRear.mRightWheel  = 3;
            diffRear.mEngineTorqueRatio  = 0.5f;
            ctrlSettings->mDifferentials.push_back(diffFront);
            ctrlSettings->mDifferentials.push_back(diffRear);
        }

        vehicleSettings.mController = ctrlSettings;

        // 5. Create constraint
        JPH::Body* body = physicsSystem->GetBodyLockInterfaceNoLock().TryGetBody(m_bodyID);
        JPH_ASSERT(body != nullptr);

        m_vehicleConstraint = new JPH::VehicleConstraint(*body, vehicleSettings);
        physicsSystem->AddConstraint(m_vehicleConstraint);
        physicsSystem->AddStepListener(m_vehicleConstraint);
    }

    JoltVehicleController::~JoltVehicleController()
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid())
        {
            m_physicsSystem->RemoveStepListener(m_vehicleConstraint);
            m_physicsSystem->RemoveConstraint(m_vehicleConstraint);
            m_physicsSystem->GetBodyInterface().RemoveBody(m_bodyID);
            m_physicsSystem->GetBodyInterface().DestroyBody(m_bodyID);
        }
    }

    // ---------------------------------------------------------------------------
    // Driver Input
    // ---------------------------------------------------------------------------
    void JoltVehicleController::SetDriverInput(float forward, float right, float brake, float handBrake)
    {
        m_inputForward   = forward;
        m_inputRight     = right;
        m_inputBrake     = brake;
        m_inputHandBrake = handBrake;
    }

    // ---------------------------------------------------------------------------
    // Transform
    // ---------------------------------------------------------------------------
    glm::vec3 JoltVehicleController::GetPosition() const
    {
        return ToGlm(m_physicsSystem->GetBodyInterface().GetPosition(m_bodyID));
    }

    glm::quat JoltVehicleController::GetRotation() const
    {
        return ToGlm(m_physicsSystem->GetBodyInterface().GetRotation(m_bodyID));
    }

    void JoltVehicleController::SetPosition(const glm::vec3& pos)
    {
        m_physicsSystem->GetBodyInterface().SetPosition(
            m_bodyID,
            JPH::RVec3(pos.x, pos.y, pos.z),
            JPH::EActivation::Activate);
    }

    // ---------------------------------------------------------------------------
    // Velocity
    // ---------------------------------------------------------------------------
    glm::vec3 JoltVehicleController::GetLinearVelocity() const
    {
        return ToGlm(m_physicsSystem->GetBodyInterface().GetLinearVelocity(m_bodyID));
    }

    glm::vec3 JoltVehicleController::GetAngularVelocity() const
    {
        return ToGlm(m_physicsSystem->GetBodyInterface().GetAngularVelocity(m_bodyID));
    }

    float JoltVehicleController::GetSpeedKmh() const
    {
        glm::vec3 v = GetLinearVelocity();
        return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z) * 3.6f;
    }

    // ---------------------------------------------------------------------------
    // Wheel Telemetry
    // ---------------------------------------------------------------------------
    u32 JoltVehicleController::GetWheelCount() const
    {
        return m_wheelCount;
    }

    WheelState JoltVehicleController::GetWheelState(u32 idx) const
    {
        WheelState state{};
        if (!m_vehicleConstraint || idx >= m_wheelCount) return state;

        const JPH::Wheel* w = m_vehicleConstraint->GetWheel(static_cast<JPH::uint>(idx));
        if (!w) return state;

        state.suspensionLength = w->GetSuspensionLength();
        state.steerAngle       = w->GetSteerAngle();
        state.isInContact      = w->HasContact();

        if (w->HasContact())
        {
            state.contactPoint  = ToGlm(w->GetContactPosition());
            state.contactNormal = ToGlm(w->GetContactNormal());
        }

        return state;
    }

    // ---------------------------------------------------------------------------
    // Step - feed driver input (called by JoltPhysicsSystem before physics Step)
    // ---------------------------------------------------------------------------
    void JoltVehicleController::Step(float /*deltaTime*/)
    {
        if (!m_vehicleConstraint) return;

        auto* ctrl = static_cast<JPH::WheeledVehicleController*>(
            m_vehicleConstraint->GetController());
        if (!ctrl) return;

        ctrl->SetDriverInput(m_inputForward, m_inputRight, m_inputBrake, m_inputHandBrake);
    }
}
