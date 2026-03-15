#pragma once

#include "IEnvironmentSystem.h"

namespace Moon {

class EnvironmentSystem final : public IEnvironmentSystem {
public:
    EnvironmentSystem() = default;
    ~EnvironmentSystem() override = default;

    void SetEnabled(bool enabled) override;
    bool IsEnabled() const override;

    void SetProfile(const EnvironmentProfile& profile) override;
    const EnvironmentProfile& GetProfile() const override;

    void SetState(const EnvironmentState& state) override;
    const EnvironmentState& GetState() const override;

    void SetWeather(WeatherType nextWeather, float transitionSeconds) override;
    void SetTimeOfDay(float hours) override;
    void Update(float deltaTimeSeconds) override;

private:
    void AdvanceTime(float deltaTimeSeconds);
    void UpdateWeather(float deltaTimeSeconds);
    void UpdateAtmosphere();
    float ComputeDaylightFactor() const;
    static float WrapHours(float hours);
    static float Clamp01(float value);
    static Vector3 Lerp(const Vector3& a, const Vector3& b, float t);

    EnvironmentProfile m_profile;
    EnvironmentState m_state;
};

} // namespace Moon
