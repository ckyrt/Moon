#include "EnvironmentComponent.h"

#include "../core/Math/Quaternion.h"
#include "../core/Scene/Light.h"
#include "../core/Scene/Scene.h"
#include "../core/Scene/SceneNode.h"

#include <cmath>

namespace Moon {

namespace {

Light* FindPrimaryDirectionalLight(Scene* scene)
{
    if (!scene) {
        return nullptr;
    }

    Light* light = nullptr;
    scene->Traverse([&](SceneNode* node) {
        if (light) {
            return;
        }

        Light* candidate = node->GetComponent<Light>();
        if (!candidate || !candidate->IsEnabled()) {
            return;
        }

        if (candidate->GetType() == Light::Type::Directional) {
            light = candidate;
        }
    });

    return light;
}

Vector3 ChooseUpAxis(const Vector3& forward)
{
    const Vector3 worldUp(0.0f, 1.0f, 0.0f);
    if (std::abs(Vector3::Dot(forward.Normalized(), worldUp)) > 0.99f) {
        return Vector3(0.0f, 0.0f, 1.0f);
    }
    return worldUp;
}

} // namespace

EnvironmentComponent::EnvironmentComponent(SceneNode* owner)
    : Component(owner) {
}

void EnvironmentComponent::SetProfile(const EnvironmentProfile& profile) {
    m_system.SetProfile(profile);
}

const EnvironmentProfile& EnvironmentComponent::GetProfile() const {
    return m_system.GetProfile();
}

void EnvironmentComponent::SetWeather(WeatherType nextWeather, float transitionSeconds) {
    m_system.SetWeather(nextWeather, transitionSeconds);
}

void EnvironmentComponent::SetTimeOfDay(float hours) {
    m_system.SetTimeOfDay(hours);
}

const EnvironmentState& EnvironmentComponent::GetState() const {
    return m_system.GetState();
}

EnvironmentSystem& EnvironmentComponent::GetSystem() {
    return m_system;
}

const EnvironmentSystem& EnvironmentComponent::GetSystem() const {
    return m_system;
}

void EnvironmentComponent::Update(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }

    m_system.SetEnabled(true);
    m_system.Update(deltaTime);

    if (!m_system.GetProfile().syncPrimaryDirectionalLight) {
        return;
    }

    SceneNode* owner = GetOwner();
    Scene* scene = owner ? owner->GetScene() : nullptr;
    Light* light = FindPrimaryDirectionalLight(scene);
    if (!light) {
        return;
    }

    const EnvironmentState& state = m_system.GetState();
    SceneNode* lightNode = light->GetOwner();
    if (!lightNode) {
        return;
    }

    const Vector3 sunDirection = state.atmosphere.sunDirection.Normalized();
    lightNode->GetTransform()->SetWorldRotation(Quaternion::LookRotation(sunDirection, ChooseUpAxis(sunDirection)));
    light->SetColor(state.atmosphere.sunColor);
    light->SetIntensity(state.atmosphere.sunIntensity);
}

} // namespace Moon
