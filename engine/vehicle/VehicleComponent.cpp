#include "VehicleComponent.h"

#include "../core/Logging/Logger.h"
#include "../physics/PhysicsSystem.h"
#include "../physics/RigidBody.h"
#include "core/Math/Quaternion.h"
#include "core/Scene/SceneNode.h"
#include "core/Scene/Transform.h"

#include <Jolt/Math/Mat44.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include <algorithm>
#include <cmath>

namespace Moon {

namespace {

constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;
constexpr float kRadiansToDegrees = 180.0f / 3.14159265358979323846f;
constexpr float kWheelWidth = 0.32f;
constexpr int kDynamicLayer = 1;

Quaternion ToMoonQuaternion(const JPH::Quat& rotation)
{
    return Quaternion(rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW());
}

Vector3 ToMoonVec3(const JPH::Vec3& value)
{
    return Vector3(value.GetX(), value.GetY(), value.GetZ());
}

Vector3 ToMoonRVec3(const JPH::RVec3& value)
{
    return Vector3(value.GetX(), value.GetY(), value.GetZ());
}

void ConfigureDefaultFriction(JPH::WheelSettingsWV& wheel)
{
    wheel.mLongitudinalFriction.Clear();
    wheel.mLongitudinalFriction.AddPoint(0.0f, 1.1f);
    wheel.mLongitudinalFriction.AddPoint(0.08f, 1.25f);
    wheel.mLongitudinalFriction.AddPoint(0.20f, 1.0f);
    wheel.mLongitudinalFriction.AddPoint(0.50f, 0.8f);

    wheel.mLateralFriction.Clear();
    wheel.mLateralFriction.AddPoint(0.0f, 1.1f);
    wheel.mLateralFriction.AddPoint(3.0f, 1.2f);
    wheel.mLateralFriction.AddPoint(8.0f, 1.0f);
    wheel.mLateralFriction.AddPoint(20.0f, 0.75f);
}

}

VehicleComponent::VehicleComponent(SceneNode* owner)
    : Component(owner) {
}

VehicleComponent::~VehicleComponent()
{
    ShutdownVehicleRuntime();
}

void VehicleComponent::SetConfig(const VehicleConfig& config)
{
    m_config = config;
    m_wheelRuntime.assign(m_config.wheels.size(), WheelRuntimeState{});
}

bool VehicleComponent::InitializeVehicleRuntime(PhysicsSystem* physicsSystem)
{
    ShutdownVehicleRuntime();

    m_physicsSystem = physicsSystem;
    MOON_LOG_INFO("Vehicle", "Initializing Jolt vehicle runtime for '%s'.", m_owner ? m_owner->GetName().c_str() : "<null>");
    RigidBody* rigidBody = GetRigidBody();
    if (!m_physicsSystem || !rigidBody || !rigidBody->HasBody()) {
        MOON_LOG_ERROR("Vehicle", "Failed to initialize Jolt vehicle runtime: missing physics body.");
        return false;
    }

    JPH::BodyLockWrite bodyLock(m_physicsSystem->GetJoltSystem().GetBodyLockInterface(), rigidBody->GetBodyID());
    if (!bodyLock.Succeeded()) {
        MOON_LOG_ERROR("Vehicle", "Failed to initialize Jolt vehicle runtime: Jolt body lock failed.");
        return false;
    }
    JPH::Body& body = bodyLock.GetBody();

    JPH::VehicleConstraintSettings vehicleSettings;
    vehicleSettings.mUp = JPH::Vec3(0.0f, 1.0f, 0.0f);
    vehicleSettings.mForward = JPH::Vec3(0.0f, 0.0f, 1.0f);
    vehicleSettings.mMaxPitchRollAngle = 0.9f * 3.14159265358979323846f;

    for (const WheelConfig& wheelConfig : m_config.wheels) {
        auto* wheel = new JPH::WheelSettingsWV();
        wheel->mPosition = JPH::Vec3(
            wheelConfig.localAttachment.x,
            wheelConfig.localAttachment.y,
            wheelConfig.localAttachment.z);
        wheel->mSuspensionForcePoint = wheel->mPosition;
        wheel->mSuspensionDirection = JPH::Vec3(0.0f, -1.0f, 0.0f);
        wheel->mSteeringAxis = JPH::Vec3(0.0f, 1.0f, 0.0f);
        wheel->mWheelUp = JPH::Vec3(0.0f, 1.0f, 0.0f);
        wheel->mWheelForward = JPH::Vec3(0.0f, 0.0f, 1.0f);
        wheel->mSuspensionMinLength = std::max(0.10f, wheelConfig.suspensionRestLength * 0.20f);
        wheel->mSuspensionMaxLength = wheelConfig.suspensionRestLength;
        wheel->mSuspensionSpring.mMode = JPH::ESpringMode::FrequencyAndDamping;
        wheel->mSuspensionSpring.mFrequency = 2.2f;
        wheel->mSuspensionSpring.mDamping = 0.85f;
        wheel->mRadius = wheelConfig.radius;
        wheel->mWidth = kWheelWidth;
        wheel->mMaxSteerAngle = wheelConfig.maxSteerAngleDegrees * kDegreesToRadians;
        wheel->mMaxBrakeTorque = wheelConfig.brakeMultiplier * m_config.brakeForce;
        wheel->mMaxHandBrakeTorque = wheelConfig.handbrakeMultiplier * m_config.handbrakeForce;
        ConfigureDefaultFriction(*wheel);
        vehicleSettings.mWheels.push_back(wheel);
    }
    auto* controllerSettings = new JPH::WheeledVehicleControllerSettings();
    controllerSettings->mEngine.mMaxTorque = std::max(400.0f, m_config.engineForce * 0.11f);
    controllerSettings->mEngine.mMinRPM = 900.0f;
    controllerSettings->mEngine.mMaxRPM = 6200.0f;
    controllerSettings->mEngine.mNormalizedTorque.AddPoint(0.0f, 0.8f);
    controllerSettings->mEngine.mNormalizedTorque.AddPoint(0.4f, 1.0f);
    controllerSettings->mEngine.mNormalizedTorque.AddPoint(0.75f, 0.95f);
    controllerSettings->mEngine.mNormalizedTorque.AddPoint(1.0f, 0.7f);
    controllerSettings->mTransmission.mMode = JPH::ETransmissionMode::Auto;
    controllerSettings->mTransmission.mShiftUpRPM = 4500.0f;
    controllerSettings->mTransmission.mShiftDownRPM = 1800.0f;
    controllerSettings->mTransmission.mClutchStrength = 6.0f;
    controllerSettings->mDifferentialLimitedSlipRatio = 1.4f;
    controllerSettings->mDifferentials.resize(2);
    controllerSettings->mDifferentials[0].mLeftWheel = 0;
    controllerSettings->mDifferentials[0].mRightWheel = 1;
    controllerSettings->mDifferentials[0].mEngineTorqueRatio = 0.5f;
    controllerSettings->mDifferentials[0].mLimitedSlipRatio = 1.4f;
    controllerSettings->mDifferentials[1].mLeftWheel = 2;
    controllerSettings->mDifferentials[1].mRightWheel = 3;
    controllerSettings->mDifferentials[1].mEngineTorqueRatio = 0.5f;
    controllerSettings->mDifferentials[1].mLimitedSlipRatio = 1.4f;
    vehicleSettings.mController = controllerSettings;
    if (m_config.wheels.size() >= 4) {
        vehicleSettings.mAntiRollBars.resize(2);
        vehicleSettings.mAntiRollBars[0].mLeftWheel = 0;
        vehicleSettings.mAntiRollBars[0].mRightWheel = 1;
        vehicleSettings.mAntiRollBars[1].mLeftWheel = 2;
        vehicleSettings.mAntiRollBars[1].mRightWheel = 3;
    }

    m_vehicleConstraint = new JPH::VehicleConstraint(body, vehicleSettings);
    m_collisionTester = new JPH::VehicleCollisionTesterCastCylinder(static_cast<JPH::ObjectLayer>(kDynamicLayer));
    m_vehicleConstraint->SetVehicleCollisionTester(m_collisionTester);

    m_physicsSystem->AddConstraint(m_vehicleConstraint);
    m_physicsSystem->AddStepListener(m_vehicleConstraint);

    MOON_LOG_INFO("Vehicle", "Initialized Jolt vehicle runtime for '%s'.", m_owner ? m_owner->GetName().c_str() : "<null>");
    return true;
}

void VehicleComponent::ShutdownVehicleRuntime()
{
    if (m_physicsSystem && m_vehicleConstraint) {
        m_physicsSystem->RemoveStepListener(m_vehicleConstraint);
        m_physicsSystem->RemoveConstraint(m_vehicleConstraint);
    }

    delete m_vehicleConstraint;
    m_vehicleConstraint = nullptr;

    delete m_collisionTester;
    m_collisionTester = nullptr;
    m_controllerRuntimeConfigured = false;
    m_physicsSystem = nullptr;
}

RigidBody* VehicleComponent::GetRigidBody() const
{
    return m_owner ? m_owner->GetComponent<RigidBody>() : nullptr;
}

float VehicleComponent::ResolveForwardInput(float forwardSpeed) const
{
    if (m_inputState.throttle > 0.0f) {
        return m_inputState.throttle;
    }

    if (m_inputState.brake <= 0.0f) {
        return 0.0f;
    }

    if (forwardSpeed > 0.75f) {
        return 0.0f;
    }

    return -m_inputState.brake;
}

void VehicleComponent::Update(float)
{
    if (!m_vehicleConstraint) {
        return;
    }

    if (!ConfigureControllerRuntime()) {
        return;
    }

    RigidBody* rigidBody = GetRigidBody();
    Transform* transform = m_owner ? m_owner->GetTransform() : nullptr;
    if (!rigidBody || !transform) {
        return;
    }

    const Vector3 worldForward = transform->GetForward().Normalized();
    const float forwardSpeed = Vector3::Dot(rigidBody->GetLinearVelocity(), worldForward);
    m_currentSpeed = forwardSpeed;

    VehicleInputState input = m_inputState;
    if (!m_driverOccupied) {
        input.throttle = 0.0f;
        input.brake = 1.0f;
        input.handbrake = true;
        input.steering = 0.0f;
    }

    const float forwardInput = ResolveForwardInput(forwardSpeed);
    const float brakeInput = (forwardSpeed > 0.75f && input.brake > 0.0f) ? input.brake : 0.0f;
    auto* controller = static_cast<JPH::WheeledVehicleController*>(m_vehicleConstraint->GetController());
    if (!controller) {
        return;
    }
    controller->SetDriverInput(forwardInput, input.steering, brakeInput, input.handbrake ? 1.0f : 0.0f);

    if (std::abs(forwardInput) > 0.001f || brakeInput > 0.001f || std::abs(input.steering) > 0.001f || input.handbrake) {
        rigidBody->SetEnabled(true);
        if (m_physicsSystem) {
            m_physicsSystem->ActivateBody(rigidBody->GetBodyID());
        }
    }
}

bool VehicleComponent::ConfigureControllerRuntime()
{
    m_controllerRuntimeConfigured = true;
    return true;
}

void VehicleComponent::PostPhysicsSync()
{
    if (!m_vehicleConstraint) {
        return;
    }

    UpdateWheelRuntimeFromJolt();
    UpdateWheelVisualsFromJolt();
}

void VehicleComponent::UpdateWheelRuntimeFromJolt()
{
    const JPH::Wheels& wheels = m_vehicleConstraint->GetWheels();
    const size_t count = std::min(m_wheelRuntime.size(), static_cast<size_t>(wheels.size()));
    for (size_t i = 0; i < count; ++i) {
        WheelRuntimeState& runtime = m_wheelRuntime[i];
        const JPH::Wheel* wheel = wheels[static_cast<JPH::uint>(i)];
        runtime.grounded = wheel->HasContact();
        runtime.compression = m_config.wheels[i].suspensionRestLength - wheel->GetSuspensionLength();
        runtime.steerAngleDegrees = wheel->GetSteerAngle() * kRadiansToDegrees;
        runtime.spinAngleDegrees = wheel->GetRotationAngle() * kRadiansToDegrees;
        runtime.angularVelocity = wheel->GetAngularVelocity();
        runtime.lastSuspensionForce = wheel->GetSuspensionLambda();
        runtime.lastDriveForce = wheel->GetLongitudinalLambda();
        runtime.lastLateralSpeed = wheel->GetLateralLambda();
        if (wheel->HasContact()) {
            runtime.contactPoint = ToMoonRVec3(wheel->GetContactPosition());
            runtime.contactNormal = ToMoonVec3(wheel->GetContactNormal());
        }
    }
}

void VehicleComponent::UpdateWheelVisualsFromJolt()
{
    const size_t count = std::min(m_wheelVisuals.size(), static_cast<size_t>(m_vehicleConstraint->GetWheels().size()));
    for (size_t i = 0; i < count; ++i) {
        SceneNode* wheelNode = m_wheelVisuals[i];
        if (!wheelNode) {
            continue;
        }

        const bool isLeftWheel = m_config.wheels[i].localAttachment.x > 0.0f;
        const JPH::Vec3 wheelRight = isLeftWheel ? -JPH::Vec3::sAxisX() : JPH::Vec3::sAxisX();
        const JPH::RMat44 worldTransform = m_vehicleConstraint->GetWheelWorldTransform(
            static_cast<JPH::uint>(i),
            wheelRight,
            JPH::Vec3::sAxisY());

        wheelNode->GetTransform()->SetWorldPosition(ToMoonRVec3(worldTransform.GetTranslation()));
        wheelNode->GetTransform()->SetWorldRotation(ToMoonQuaternion(worldTransform.GetQuaternion()));
    }
}

} // namespace Moon
