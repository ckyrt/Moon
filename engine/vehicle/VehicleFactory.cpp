#include "VehicleFactory.h"

#include "VehicleComponent.h"
#include "../core/Logging/Logger.h"
#include "../physics/PhysicsSystem.h"
#include "../physics/RigidBody.h"
#include "../terrain/TerrainComponent.h"
#include "core/Geometry/MeshGenerator.h"
#include "core/Scene/Material.h"
#include "core/Scene/MeshRenderer.h"
#include "core/Scene/Scene.h"
#include "core/Scene/SceneNode.h"
#include "core/Scene/Transform.h"

#include <algorithm>
#include <memory>

namespace Moon {

namespace {

float ComputeSpawnLift(const VehicleConfig& config)
{
    float requiredLift = config.chassisHalfExtents.y + 0.75f;
    for (const WheelConfig& wheel : config.wheels) {
        const float wheelLift = wheel.radius + wheel.suspensionRestLength - wheel.localAttachment.y + 0.45f;
        requiredLift = std::max(requiredLift, wheelLift);
    }
    return requiredLift;
}

SceneNode* AttachPrimitive(
    SceneNode* parent,
    const char* name,
    Mesh* mesh,
    MaterialPreset preset,
    const Vector3& localPosition,
    const Vector3& localScale,
    const Vector3& baseColor,
    const Quaternion& localRotation = Quaternion::Identity())
{
    if (!parent || !mesh) {
        delete mesh;
        return nullptr;
    }

    SceneNode* node = parent->GetScene()->CreateNode(name);
    node->SetParent(parent, false);
    node->GetTransform()->SetLocalPosition(localPosition);
    node->GetTransform()->SetLocalRotation(localRotation);
    node->GetTransform()->SetLocalScale(localScale);

    MeshRenderer* renderer = node->AddComponent<MeshRenderer>();
    renderer->SetMesh(std::shared_ptr<Mesh>(mesh));

    Material* material = node->AddComponent<Material>();
    material->SetMaterialPreset(preset);
    material->SetBaseColor(baseColor);
    return node;
}

}

SceneNode* VehicleFactory::CreateBuggy(Scene* scene, PhysicsSystem* physicsSystem, const Vector3& position)
{
    if (!scene || !physicsSystem) {
        return nullptr;
    }

    SceneNode* root = scene->CreateNode("Buggy Vehicle");
    root->GetTransform()->SetLocalPosition(position);

    RigidBody* rigidBody = root->AddComponent<RigidBody>();
    rigidBody->CreateBody(
        physicsSystem,
        PhysicsShapeType::Box,
        Vector3(1.15f, 0.55f, 2.15f),
        1450.0f);

    VehicleComponent* vehicle = root->AddComponent<VehicleComponent>();
    VehicleConfig config;
    config.kind = VehicleKind::Wheeled;
    config.mass = 1450.0f;
    config.chassisHalfExtents = Vector3(1.15f, 0.55f, 2.15f);
    config.engineForce = 13200.0f;
    config.brakeForce = 8400.0f;
    config.handbrakeForce = 11000.0f;
    config.maxForwardSpeed = 48.0f;
    config.maxReverseSpeed = 14.0f;
    config.maxSteerAngleDegrees = 30.0f;
    config.enterRadius = 7.5f;
    config.exitDistance = 4.0f;
    config.camera.followOffset = Vector3(0.0f, 4.0f, -10.0f);
    config.camera.lookOffset = Vector3(0.0f, 1.6f, 3.5f);
    config.wheels = {
        { Vector3( 1.05f, -0.10f,  1.55f), 0.55f, 0.55f, 42000.0f, 5200.0f, 30.0f, 0.5f, 1.0f, 0.0f },
        { Vector3(-1.05f, -0.10f,  1.55f), 0.55f, 0.55f, 42000.0f, 5200.0f, 30.0f, 0.5f, 1.0f, 0.0f },
        { Vector3( 1.05f, -0.10f, -1.55f), 0.55f, 0.55f, 44000.0f, 5400.0f,  0.0f, 0.5f, 1.0f, 1.0f },
        { Vector3(-1.05f, -0.10f, -1.55f), 0.55f, 0.55f, 44000.0f, 5400.0f,  0.0f, 0.5f, 1.0f, 1.0f }
    };
    vehicle->SetConfig(config);

    MOON_LOG_INFO(
        "VehicleFactory",
        "Creating buggy at (%.2f, %.2f, %.2f) using Jolt vehicle runtime",
        position.x,
        position.y,
        position.z);

    AttachPrimitive(
        root,
        "Buggy Body",
        MeshGenerator::CreateCube(1.0f, Vector3(0.88f, 0.33f, 0.15f)),
        MaterialPreset::Metal,
        Vector3(0.0f, 0.35f, 0.0f),
        Vector3(2.3f, 0.65f, 4.0f),
        Vector3(0.88f, 0.33f, 0.15f));

    AttachPrimitive(
        root,
        "Buggy Cabin",
        MeshGenerator::CreateCube(1.0f, Vector3(0.95f, 0.90f, 0.82f)),
        MaterialPreset::Steel,
        Vector3(0.0f, 1.05f, -0.10f),
        Vector3(1.8f, 0.80f, 1.7f),
        Vector3(0.95f, 0.90f, 0.82f));

    std::vector<SceneNode*> wheelNodes;
    const char* wheelNames[] = { "Wheel FL", "Wheel FR", "Wheel RL", "Wheel RR" };
    for (int i = 0; i < 4; ++i) {
        const WheelConfig& wheelConfig = config.wheels[static_cast<size_t>(i)];
        const Vector3 wheelPosition(
            wheelConfig.localAttachment.x,
            wheelConfig.localAttachment.y - wheelConfig.suspensionRestLength,
            wheelConfig.localAttachment.z);
        SceneNode* wheelNode = AttachPrimitive(
            root,
            wheelNames[i],
            MeshGenerator::CreateCylinder(0.5f, 0.5f, 1.0f, 24),
            MaterialPreset::Rubber,
            wheelPosition,
            Vector3(wheelConfig.radius * 2.0f, wheelConfig.radius * 0.55f, wheelConfig.radius * 2.0f),
            Vector3(0.12f, 0.12f, 0.12f));
        wheelNodes.push_back(wheelNode);
    }
    vehicle->SetWheelVisuals(wheelNodes);

    scene->TraverseActive([&](SceneNode* node) {
        if (!node) {
            return;
        }

        TerrainComponent* terrain = node->GetComponent<TerrainComponent>();
        if (!terrain) {
            return;
        }

        float groundHeight = 0.0f;
        Vector3 groundNormal(0.0f, 1.0f, 0.0f);
        if (terrain->SampleWorldHeightAndNormal(position, groundHeight, groundNormal)) {
            const float spawnLift = ComputeSpawnLift(config);
            rigidBody->SetPositionRotation(
                Vector3(position.x, groundHeight + spawnLift, position.z),
                Quaternion::Identity());
            rigidBody->SetLinearVelocity(Vector3(0.0f, 0.0f, 0.0f));
            rigidBody->SetAngularVelocity(Vector3(0.0f, 0.0f, 0.0f));
            MOON_LOG_INFO(
                "VehicleFactory",
                "Vehicle spawn terrainY=%.2f normal=(%.2f, %.2f, %.2f) lift=%.2f finalBodyPos=(%.2f, %.2f, %.2f)",
                groundHeight,
                groundNormal.x,
                groundNormal.y,
                groundNormal.z,
                spawnLift,
                rigidBody->GetPosition().x,
                rigidBody->GetPosition().y,
                rigidBody->GetPosition().z);
        }
    });

    const bool initialized = vehicle->InitializeVehicleRuntime(physicsSystem);
    MOON_LOG_INFO("VehicleFactory", "Jolt vehicle runtime init result: %s", initialized ? "success" : "failure");
    vehicle->PostPhysicsSync();
    return root;
}

} // namespace Moon
