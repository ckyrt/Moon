#pragma once

#include "core/Math/Vector3.h"

#include <vector>

namespace Moon {

enum class VehicleKind {
    Wheeled,
    Marine,
    Flight
};

struct VehicleInputState {
    float throttle = 0.0f;
    float brake = 0.0f;
    float steering = 0.0f;
    bool handbrake = false;
    bool exitRequested = false;
};

struct VehicleCameraSettings {
    Vector3 followOffset = Vector3(0.0f, 3.8f, -9.5f);
    Vector3 lookOffset = Vector3(0.0f, 1.4f, 3.0f);
    float positionSmoothing = 6.0f;
    float targetSmoothing = 8.0f;
};

struct WheelConfig {
    Vector3 localAttachment = Vector3(0.0f, 0.0f, 0.0f);
    float radius = 0.55f;
    float suspensionRestLength = 0.45f;
    float springStrength = 34000.0f;
    float damperStrength = 4200.0f;
    float maxSteerAngleDegrees = 0.0f;
    float driveMultiplier = 0.0f;
    float brakeMultiplier = 1.0f;
    float handbrakeMultiplier = 0.0f;
};

struct WheelRuntimeState {
    bool grounded = false;
    float compression = 0.0f;
    float previousCompression = 0.0f;
    float steerAngleDegrees = 0.0f;
    float angularVelocity = 0.0f;
    float spinAngleDegrees = 0.0f;
    float lastLongitudinalSpeed = 0.0f;
    float lastLateralSpeed = 0.0f;
    float lastSuspensionForce = 0.0f;
    float lastDriveForce = 0.0f;
    Vector3 contactPoint = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 contactNormal = Vector3(0.0f, 1.0f, 0.0f);
};

struct VehicleConfig {
    VehicleKind kind = VehicleKind::Wheeled;
    float mass = 1450.0f;
    Vector3 chassisHalfExtents = Vector3(1.15f, 0.55f, 2.15f);
    float engineForce = 9200.0f;
    float brakeForce = 7600.0f;
    float handbrakeForce = 9800.0f;
    float lateralGrip = 2400.0f;
    float longitudinalDrag = 95.0f;
    float angularDamping = 2.8f;
    float maxSteerAngleDegrees = 30.0f;
    float steerSpeed = 120.0f;
    float maxForwardSpeed = 52.0f;
    float maxReverseSpeed = 16.0f;
    float enterRadius = 6.0f;
    float exitDistance = 3.0f;
    VehicleCameraSettings camera;
    std::vector<WheelConfig> wheels;
};

} // namespace Moon
