#include "EnvironmentSystem.h"

#include <algorithm>
#include <cmath>

namespace Moon {

namespace {
constexpr float kHoursPerDay = 24.0f;
constexpr float kSecondsPerMinute = 60.0f;
constexpr float kPi = 3.1415926535f;

struct WeatherVisualPreset {
    Vector3 zenithTint;
    Vector3 horizonTint;
    Vector3 sunTint;
    float sunIntensityScale;
    float fogDensity;
    float cloudCoverage;
    float windSpeedScale;
    float gustScale;
    float turbulenceScale;
};

WeatherVisualPreset MakeWeatherPreset(WeatherType weather, const EnvironmentProfile& profile, float duskBlend)
{
    switch (weather) {
    case WeatherType::Clear:
        return {
            Vector3(1.08f, 1.10f, 1.18f),
            Vector3(1.04f, 1.02f, 1.00f),
            Vector3(1.02f, 1.00f, 0.96f),
            1.08f,
            profile.clearFogDensity,
            0.06f + 0.05f * duskBlend,
            1.00f,
            1.00f,
            1.00f
        };
    case WeatherType::Cloudy:
        return {
            Vector3(0.88f, 0.92f, 1.00f),
            Vector3(0.95f, 0.97f, 1.00f),
            Vector3(0.97f, 0.98f, 1.00f),
            0.82f,
            profile.clearFogDensity * 1.28f,
            profile.cloudyCloudCoverage,
            1.20f,
            1.25f,
            1.15f
        };
    case WeatherType::Rain:
        return {
            Vector3(0.62f, 0.68f, 0.79f),
            Vector3(0.74f, 0.78f, 0.84f),
            Vector3(0.86f, 0.90f, 0.95f),
            0.52f,
            profile.clearFogDensity * 1.95f,
            profile.rainCloudCoverage,
            1.45f,
            1.60f,
            1.45f
        };
    case WeatherType::Snow:
        return {
            Vector3(0.82f, 0.88f, 0.98f),
            Vector3(0.93f, 0.95f, 1.00f),
            Vector3(0.94f, 0.96f, 1.00f),
            0.62f,
            profile.clearFogDensity * 1.55f,
            0.78f,
            1.05f,
            1.10f,
            1.05f
        };
    case WeatherType::Fog:
        return {
            Vector3(0.78f, 0.80f, 0.82f),
            Vector3(0.82f, 0.83f, 0.84f),
            Vector3(0.88f, 0.89f, 0.90f),
            0.30f,
            profile.fogWeatherDensity,
            0.45f,
            0.70f,
            0.75f,
            0.65f
        };
    case WeatherType::Storm:
        return {
            Vector3(0.28f, 0.30f, 0.36f),
            Vector3(0.33f, 0.35f, 0.39f),
            Vector3(0.68f, 0.72f, 0.80f),
            0.18f,
            profile.fogWeatherDensity * 1.25f,
            0.95f,
            2.10f,
            2.30f,
            2.00f
        };
    }

    return {
        Vector3(1.0f, 1.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f),
        1.0f,
        profile.clearFogDensity,
        0.15f,
        1.0f,
        1.0f,
        1.0f
    };
}

Vector3 MultiplyComponents(const Vector3& a, const Vector3& b)
{
    return Vector3(a.x * b.x, a.y * b.y, a.z * b.z);
}
}

void EnvironmentSystem::SetEnabled(bool enabled) {
    m_state.enabled = enabled;
}

bool EnvironmentSystem::IsEnabled() const {
    return m_state.enabled;
}

void EnvironmentSystem::SetProfile(const EnvironmentProfile& profile) {
    m_profile = profile;
    if (m_profile.lockToFixedTime) {
        SetTimeOfDay(m_profile.fixedTimeHours);
    }
    UpdateAtmosphere();
}

const EnvironmentProfile& EnvironmentSystem::GetProfile() const {
    return m_profile;
}

void EnvironmentSystem::SetState(const EnvironmentState& state) {
    m_state = state;
    m_state.timeOfDay.timeOfDayHours = WrapHours(m_state.timeOfDay.timeOfDayHours);
    m_state.wind.direction = m_state.wind.direction.Normalized();
    UpdateAtmosphere();
}

const EnvironmentState& EnvironmentSystem::GetState() const {
    return m_state;
}

void EnvironmentSystem::SetWeather(WeatherType nextWeather, float transitionSeconds) {
    if (!m_profile.enableWeather) {
        m_state.weather.current = nextWeather;
        m_state.weather.target = nextWeather;
        m_state.weather.transitionAlpha = 1.0f;
        m_state.weather.transitionDurationSeconds = 0.0f;
        m_state.weather.transitionElapsedSeconds = 0.0f;
        UpdateAtmosphere();
        return;
    }

    m_state.weather.target = nextWeather;
    m_state.weather.transitionDurationSeconds = std::max(0.0f, transitionSeconds);
    m_state.weather.transitionElapsedSeconds = 0.0f;
    m_state.weather.transitionAlpha = m_state.weather.transitionDurationSeconds <= 0.0f ? 1.0f : 0.0f;

    if (m_state.weather.transitionAlpha >= 1.0f) {
        m_state.weather.current = nextWeather;
    }

    UpdateAtmosphere();
}

void EnvironmentSystem::SetTimeOfDay(float hours) {
    m_state.timeOfDay.timeOfDayHours = WrapHours(hours);
    UpdateAtmosphere();
}

void EnvironmentSystem::Update(float deltaTimeSeconds) {
    if (!m_state.enabled) {
        return;
    }

    AdvanceTime(deltaTimeSeconds);
    UpdateWeather(deltaTimeSeconds);
    UpdateAtmosphere();
}

void EnvironmentSystem::AdvanceTime(float deltaTimeSeconds) {
    if (!m_profile.enableDayNightCycle || m_profile.lockToFixedTime || m_state.timeOfDay.paused) {
        if (m_profile.lockToFixedTime) {
            m_state.timeOfDay.timeOfDayHours = WrapHours(m_profile.fixedTimeHours);
        }
        return;
    }

    const float dayLengthMinutes = std::max(0.1f, m_state.timeOfDay.dayLengthMinutes);
    const float gameHoursPerSecond = (kHoursPerDay / (dayLengthMinutes * kSecondsPerMinute)) * m_state.timeOfDay.timeScale;
    m_state.timeOfDay.timeOfDayHours = WrapHours(m_state.timeOfDay.timeOfDayHours + gameHoursPerSecond * deltaTimeSeconds);
}

void EnvironmentSystem::UpdateWeather(float deltaTimeSeconds) {
    WeatherState& weather = m_state.weather;
    if (weather.current == weather.target) {
        weather.transitionAlpha = 1.0f;
        weather.transitionElapsedSeconds = 0.0f;
        weather.transitionDurationSeconds = 0.0f;
        return;
    }

    if (weather.transitionDurationSeconds <= 0.0f) {
        weather.current = weather.target;
        weather.transitionAlpha = 1.0f;
        return;
    }

    weather.transitionElapsedSeconds += std::max(0.0f, deltaTimeSeconds);
    weather.transitionAlpha = Clamp01(weather.transitionElapsedSeconds / weather.transitionDurationSeconds);

    if (weather.transitionAlpha >= 1.0f) {
        weather.current = weather.target;
        weather.transitionDurationSeconds = 0.0f;
        weather.transitionElapsedSeconds = 0.0f;
    }
}

void EnvironmentSystem::UpdateAtmosphere() {
    const float daylight = ComputeDaylightFactor();
    const float sunAngle = (m_state.timeOfDay.timeOfDayHours / kHoursPerDay) * (2.0f * kPi) - (kPi * 0.5f);
    const float sunHeight = std::sin(sunAngle);
    const float horizontal = std::cos(sunAngle);

    m_state.atmosphere.sunDirection = Vector3(horizontal, -sunHeight, 0.15f).Normalized();
    m_state.atmosphere.sunIntensity =
        m_profile.minSunIntensity + (m_profile.maxSunIntensity - m_profile.minSunIntensity) * daylight;

    const Vector3 dawnZenith(0.20f, 0.30f, 0.58f);
    const Vector3 dayZenith(0.06f, 0.34f, 0.96f);
    const Vector3 nightZenith(0.01f, 0.02f, 0.06f);
    const Vector3 dawnHorizon(1.00f, 0.68f, 0.40f);
    const Vector3 dayHorizon(0.86f, 0.94f, 1.00f);
    const Vector3 nightHorizon(0.03f, 0.04f, 0.08f);

    const float duskBlend = Clamp01(1.0f - std::abs(0.5f - daylight) * 2.0f);
    const bool isNight = daylight <= 0.05f;

    const float zenithBlend = std::pow(daylight, 0.60f);
    const float horizonBlend = std::pow(daylight, 0.38f);
    m_state.atmosphere.skyZenithColor = isNight
        ? nightZenith
        : Lerp(dawnZenith, dayZenith, zenithBlend);
    m_state.atmosphere.skyHorizonColor = isNight
        ? nightHorizon
        : Lerp(dawnHorizon, dayHorizon, horizonBlend);
    m_state.atmosphere.sunColor = Lerp(Vector3(1.0f, 0.60f, 0.32f), Vector3(1.0f, 0.98f, 0.93f), std::pow(daylight, 0.52f));

    const WeatherVisualPreset currentPreset = MakeWeatherPreset(m_state.weather.current, m_profile, duskBlend);
    const WeatherVisualPreset targetPreset = MakeWeatherPreset(m_state.weather.target, m_profile, duskBlend);
    const float weatherBlend = Clamp01(m_state.weather.transitionAlpha);

    m_state.atmosphere.skyZenithColor =
        Lerp(MultiplyComponents(m_state.atmosphere.skyZenithColor, currentPreset.zenithTint),
             MultiplyComponents(m_state.atmosphere.skyZenithColor, targetPreset.zenithTint),
             weatherBlend);
    m_state.atmosphere.skyHorizonColor =
        Lerp(MultiplyComponents(m_state.atmosphere.skyHorizonColor, currentPreset.horizonTint),
             MultiplyComponents(m_state.atmosphere.skyHorizonColor, targetPreset.horizonTint),
             weatherBlend);
    m_state.atmosphere.sunColor =
        Lerp(MultiplyComponents(m_state.atmosphere.sunColor, currentPreset.sunTint),
             MultiplyComponents(m_state.atmosphere.sunColor, targetPreset.sunTint),
             weatherBlend);

    m_state.atmosphere.sunIntensity *=
        (1.0f - weatherBlend) * currentPreset.sunIntensityScale + weatherBlend * targetPreset.sunIntensityScale;
    m_state.atmosphere.fogDensity =
        (1.0f - weatherBlend) * currentPreset.fogDensity + weatherBlend * targetPreset.fogDensity;
    m_state.atmosphere.cloudCoverage =
        (1.0f - weatherBlend) * currentPreset.cloudCoverage + weatherBlend * targetPreset.cloudCoverage;

    const float baseWindSpeed = std::max(0.0f, m_profile.baseWindSpeed);
    const float baseGustStrength = std::max(0.0f, m_profile.baseWindGustStrength);
    const float baseTurbulence = std::max(0.0f, m_profile.baseWindTurbulence);
    m_state.wind.speed =
        baseWindSpeed * ((1.0f - weatherBlend) * currentPreset.windSpeedScale + weatherBlend * targetPreset.windSpeedScale);
    m_state.wind.gustStrength =
        baseGustStrength * ((1.0f - weatherBlend) * currentPreset.gustScale + weatherBlend * targetPreset.gustScale);
    m_state.wind.turbulence =
        baseTurbulence * ((1.0f - weatherBlend) * currentPreset.turbulenceScale + weatherBlend * targetPreset.turbulenceScale);

    if (!m_profile.enableClouds) {
        m_state.atmosphere.cloudCoverage = 0.0f;
    }
    if (!m_profile.enableFog) {
        m_state.atmosphere.fogDensity = 0.0f;
    }
    if (!m_profile.enableWind) {
        m_state.wind.speed = 0.0f;
        m_state.wind.gustStrength = 0.0f;
        m_state.wind.turbulence = 0.0f;
    }
}

float EnvironmentSystem::ComputeDaylightFactor() const {
    const float sunrise = m_profile.sunriseHour;
    const float sunset = m_profile.sunsetHour;
    const float current = m_state.timeOfDay.timeOfDayHours;

    if (current <= sunrise || current >= sunset) {
        return 0.0f;
    }

    const float dayProgress = (current - sunrise) / std::max(0.001f, sunset - sunrise);
    return std::sin(dayProgress * kPi);
}

float EnvironmentSystem::WrapHours(float hours) {
    float wrapped = std::fmod(hours, kHoursPerDay);
    if (wrapped < 0.0f) {
        wrapped += kHoursPerDay;
    }
    return wrapped;
}

float EnvironmentSystem::Clamp01(float value) {
    if (value <= 0.0f) {
        return 0.0f;
    }
    if (value >= 1.0f) {
        return 1.0f;
    }
    return value;
}

Vector3 EnvironmentSystem::Lerp(const Vector3& a, const Vector3& b, float t) {
    const float alpha = Clamp01(t);
    return a * (1.0f - alpha) + b * alpha;
}

} // namespace Moon
