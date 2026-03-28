#pragma once

#include "../core/Math/Vector3.h"
#include <string>

namespace Moon {

enum class WeatherType {
    Clear,
    Cloudy,
    Rain,
    Snow,
    Fog,
    Storm
};

struct TimeOfDayState {
    float timeOfDayHours = 12.0f;
    float dayLengthMinutes = 24.0f;
    float timeScale = 60.0f;
    bool paused = false;
};

struct WeatherState {
    WeatherType current = WeatherType::Clear;
    WeatherType target = WeatherType::Clear;
    float transitionAlpha = 1.0f;
    float transitionDurationSeconds = 0.0f;
    float transitionElapsedSeconds = 0.0f;
};

struct WindFieldState {
    Vector3 direction = Vector3(1.0f, 0.0f, 0.0f);
    float speed = 2.0f;
    float gustStrength = 0.15f;
    float turbulence = 0.05f;
};

struct AtmosphereState {
    Vector3 sunDirection = Vector3(0.0f, -1.0f, 0.0f);
    Vector3 sunColor = Vector3(1.0f, 0.95f, 0.85f);
    float sunIntensity = 10.0f;
    Vector3 skyZenithColor = Vector3(0.18f, 0.35f, 0.75f);
    Vector3 skyHorizonColor = Vector3(0.85f, 0.65f, 0.45f);
    float fogDensity = 0.0025f;
    float cloudCoverage = 0.15f;
};

struct EnvironmentState {
    bool enabled = true;
    TimeOfDayState timeOfDay;
    WeatherState weather;
    WindFieldState wind;
    AtmosphereState atmosphere;
};

struct EnvironmentProfile {
    std::string name = "DefaultEnvironment";
    bool enableDayNightCycle = true;
    bool enableWeather = true;
    bool enableWind = true;
    bool enableClouds = true;
    bool enableFog = true;
    bool syncPrimaryDirectionalLight = true;
    bool lockToFixedTime = false;
    float fixedTimeHours = 12.0f;
    float sunriseHour = 6.0f;
    float sunsetHour = 18.0f;
    float minSunIntensity = 0.05f;
    float maxSunIntensity = 12.0f;
    float clearFogDensity = 0.0015f;
    float fogWeatherDensity = 0.015f;
    float rainCloudCoverage = 0.85f;
    float cloudyCloudCoverage = 0.65f;
    float baseWindSpeed = 2.0f;
    float baseWindGustStrength = 0.15f;
    float baseWindTurbulence = 0.05f;
};

} // namespace Moon
