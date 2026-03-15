#pragma once

#include "../core/Scene/Component.h"
#include "EnvironmentSystem.h"

namespace Moon {

class EnvironmentComponent : public Component {
public:
    explicit EnvironmentComponent(SceneNode* owner);
    ~EnvironmentComponent() override = default;

    void SetProfile(const EnvironmentProfile& profile);
    const EnvironmentProfile& GetProfile() const;

    void SetWeather(WeatherType nextWeather, float transitionSeconds);
    void SetTimeOfDay(float hours);

    const EnvironmentState& GetState() const;
    EnvironmentSystem& GetSystem();
    const EnvironmentSystem& GetSystem() const;

    void Update(float deltaTime) override;

private:
    EnvironmentSystem m_system;
};

} // namespace Moon
