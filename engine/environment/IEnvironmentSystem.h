#pragma once

#include "EnvironmentTypes.h"

namespace Moon {

class IEnvironmentSystem {
public:
    virtual ~IEnvironmentSystem() = default;

    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsEnabled() const = 0;

    virtual void SetProfile(const EnvironmentProfile& profile) = 0;
    virtual const EnvironmentProfile& GetProfile() const = 0;

    virtual void SetState(const EnvironmentState& state) = 0;
    virtual const EnvironmentState& GetState() const = 0;

    virtual void SetWeather(WeatherType nextWeather, float transitionSeconds) = 0;
    virtual void SetTimeOfDay(float hours) = 0;
    virtual void Update(float deltaTimeSeconds) = 0;
};

} // namespace Moon
