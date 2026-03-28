#include "ProceduralTerrainGenerator.h"

#include "../core/Math/Vector2.h"

#include <algorithm>
#include <cmath>

namespace Moon {

namespace {

constexpr float kPi = 3.1415926535f;

float Clamp01(float value)
{
    if (value <= 0.0f) {
        return 0.0f;
    }
    if (value >= 1.0f) {
        return 1.0f;
    }
    return value;
}

float SmoothStep(float edge0, float edge1, float x)
{
    const float t = Clamp01((x - edge0) / std::max(0.0001f, edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

float Lerp(float a, float b, float t)
{
    return a * (1.0f - t) + b * t;
}

float RemapClamped(float value, float inMin, float inMax, float outMin, float outMax)
{
    const float t = Clamp01((value - inMin) / std::max(0.0001f, inMax - inMin));
    return Lerp(outMin, outMax, t);
}

float HashNoise(int x, int y, uint32_t seed)
{
    uint32_t h = seed;
    h ^= static_cast<uint32_t>(x) * 374761393u;
    h ^= static_cast<uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0x00ffffffu) / static_cast<float>(0x00ffffffu);
}

float ValueNoise(float x, float y, uint32_t seed)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const float sx = tx * tx * (3.0f - 2.0f * tx);
    const float sy = ty * ty * (3.0f - 2.0f * ty);

    const float n00 = HashNoise(x0, y0, seed);
    const float n10 = HashNoise(x1, y0, seed);
    const float n01 = HashNoise(x0, y1, seed);
    const float n11 = HashNoise(x1, y1, seed);

    const float nx0 = Lerp(n00, n10, sx);
    const float nx1 = Lerp(n01, n11, sx);
    return Lerp(nx0, nx1, sy);
}

float FractalNoise(float x, float y, uint32_t seed, int octaves, float lacunarity, float gain)
{
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float total = 0.0f;
    float normalization = 0.0f;

    for (int octave = 0; octave < octaves; ++octave) {
        total += (ValueNoise(x * frequency, y * frequency, seed + static_cast<uint32_t>(octave) * 101u) * 2.0f - 1.0f) * amplitude;
        normalization += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    if (normalization <= 0.0f) {
        return 0.0f;
    }
    return total / normalization;
}

float DistanceToSegment(const Vector2& point, const Vector2& a, const Vector2& b)
{
    const Vector2 ab = b - a;
    const float lengthSq = Vector2::Dot(ab, ab);
    if (lengthSq <= 0.0001f) {
        return (point - a).Length();
    }

    const float t = Clamp01(Vector2::Dot(point - a, ab) / lengthSq);
    const Vector2 projection = a + ab * t;
    return (point - projection).Length();
}

Vector2 CatmullRomInterpolate2D(const Vector2& p0, const Vector2& p1, const Vector2& p2, const Vector2& p3, float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return (p1 * 2.0f +
        (p2 - p0) * t +
        (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
        (-p0 + p1 * 3.0f - p2 * 3.0f + p3) * t3) * 0.5f;
}

std::vector<float> ResampleRiverPolyline(const std::vector<float>& controlPolyline)
{
    const int controlPointCount = static_cast<int>(controlPolyline.size() / 3);
    if (controlPointCount < 2) {
        return controlPolyline;
    }

    float estimatedLength = 0.0f;
    for (int i = 0; i < controlPointCount - 1; ++i) {
        const Vector2 a(controlPolyline[static_cast<size_t>(i) * 3 + 0], controlPolyline[static_cast<size_t>(i) * 3 + 2]);
        const Vector2 b(controlPolyline[static_cast<size_t>(i + 1) * 3 + 0], controlPolyline[static_cast<size_t>(i + 1) * 3 + 2]);
        estimatedLength += (b - a).Length();
    }

    const int sampleCount = std::max(controlPointCount * 6, static_cast<int>(estimatedLength / 18.0f));
    std::vector<float> sampledPolyline;
    sampledPolyline.reserve(static_cast<size_t>(sampleCount + 1) * 3);

    for (int i = 0; i <= sampleCount; ++i) {
        const float globalT = static_cast<float>(i) / static_cast<float>(sampleCount);
        const float scaledT = globalT * static_cast<float>(controlPointCount - 1);
        const int segment = std::min(static_cast<int>(scaledT), controlPointCount - 2);
        const float localT = scaledT - static_cast<float>(segment);

        const int idx0 = std::max(0, segment - 1);
        const int idx1 = segment;
        const int idx2 = segment + 1;
        const int idx3 = std::min(controlPointCount - 1, segment + 2);

        const Vector2 p0(controlPolyline[static_cast<size_t>(idx0) * 3 + 0], controlPolyline[static_cast<size_t>(idx0) * 3 + 2]);
        const Vector2 p1(controlPolyline[static_cast<size_t>(idx1) * 3 + 0], controlPolyline[static_cast<size_t>(idx1) * 3 + 2]);
        const Vector2 p2(controlPolyline[static_cast<size_t>(idx2) * 3 + 0], controlPolyline[static_cast<size_t>(idx2) * 3 + 2]);
        const Vector2 p3(controlPolyline[static_cast<size_t>(idx3) * 3 + 0], controlPolyline[static_cast<size_t>(idx3) * 3 + 2]);

        const Vector2 p = CatmullRomInterpolate2D(p0, p1, p2, p3, localT);
        sampledPolyline.push_back(p.x);
        sampledPolyline.push_back(0.0f);
        sampledPolyline.push_back(p.y);
    }

    return sampledPolyline;
}

std::vector<float> BuildRiverPolyline(const TerrainGenerationSettings& settings, uint32_t riverIndex)
{
    std::vector<float> polyline;
    const int controlPointCount = 11;
    polyline.reserve(static_cast<size_t>(controlPointCount) * 3);

    const float normalizedIndex = settings.riverCount > 1
        ? static_cast<float>(riverIndex) / static_cast<float>(settings.riverCount - 1)
        : 0.5f;
    const float lateralOffset = (normalizedIndex - 0.5f) * settings.worldWidth * 0.42f;
    const float meanderScale = 0.06f + settings.riverMeander * 0.14f;

    for (int i = 0; i < controlPointCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(controlPointCount - 1);
        const float worldZ = -settings.worldDepth * 0.5f + t * settings.worldDepth;
        const float bendPrimary = std::sin(t * (2.0f + settings.riverMeander * 1.8f) * kPi + 0.35f + riverIndex * 0.7f) * settings.worldWidth * meanderScale;
        const float bendSecondary = std::sin(t * 6.0f * kPi + 0.8f + riverIndex * 1.37f) * settings.worldWidth * 0.03f * settings.riverMeander;
        const float worldX = lateralOffset + bendPrimary + bendSecondary;

        polyline.push_back(worldX);
        polyline.push_back(0.0f);
        polyline.push_back(worldZ);
    }

    return ResampleRiverPolyline(polyline);
}

float ComputeRiverDistance(const Vector2& point, const std::vector<float>& polyline)
{
    if (polyline.size() < 6) {
        return 999999.0f;
    }

    float minDistance = 999999.0f;
    const int pointCount = static_cast<int>(polyline.size() / 3);
    for (int i = 0; i < pointCount - 1; ++i) {
        const Vector2 a(polyline[static_cast<size_t>(i) * 3 + 0], polyline[static_cast<size_t>(i) * 3 + 2]);
        const Vector2 b(polyline[static_cast<size_t>(i + 1) * 3 + 0], polyline[static_cast<size_t>(i + 1) * 3 + 2]);
        minDistance = std::min(minDistance, DistanceToSegment(point, a, b));
    }
    return minDistance;
}

struct RiverChannelSample {
    float distance = 999999.0f;
    float width = 0.0f;
    float longitudinalT = 0.0f;
    bool valid = false;
};

std::vector<float> BuildRiverWidthProfile(
    const TerrainGenerationSettings& settings,
    const std::vector<float>& polyline,
    uint32_t riverIndex)
{
    const size_t pointCount = polyline.size() / 3;
    std::vector<float> widths(pointCount, settings.riverWidth);
    if (pointCount == 0) {
        return widths;
    }

    const float riverSpread = settings.riverCount > 1
        ? static_cast<float>(riverIndex) / static_cast<float>(settings.riverCount - 1)
        : 0.5f;
    const float riverBias = 0.90f + (riverSpread - 0.5f) * 0.18f;

    for (size_t i = 0; i < pointCount; ++i) {
        const float t = pointCount > 1 ? static_cast<float>(i) / static_cast<float>(pointCount - 1) : 0.0f;
        const float downstreamGrowth = Lerp(0.62f, 1.28f, std::pow(t, 0.82f));
        const float valleyPulse = 1.0f + std::sin(t * kPi * (2.2f + settings.riverMeander * 1.8f) + riverIndex * 0.9f) * 0.10f;
        const float localVariation = 1.0f + std::sin(t * kPi * 7.0f + riverIndex * 1.7f) * 0.05f;
        widths[i] = std::max(
            settings.riverWidth * 0.45f,
            settings.riverWidth * downstreamGrowth * valleyPulse * localVariation * riverBias);
    }

    return widths;
}

RiverChannelSample ComputeRiverChannelSample(
    const Vector2& point,
    const std::vector<float>& polyline,
    const std::vector<float>& widthProfile)
{
    RiverChannelSample best;
    if (polyline.size() < 6 || widthProfile.empty()) {
        return best;
    }

    const size_t pointCount = polyline.size() / 3;
    for (size_t i = 0; i + 1 < pointCount; ++i) {
        const Vector2 a(polyline[i * 3 + 0], polyline[i * 3 + 2]);
        const Vector2 b(polyline[(i + 1) * 3 + 0], polyline[(i + 1) * 3 + 2]);
        const Vector2 ab = b - a;
        const float lengthSq = Vector2::Dot(ab, ab);
        const float segmentT = lengthSq > 0.0001f ? Clamp01(Vector2::Dot(point - a, ab) / lengthSq) : 0.0f;
        const Vector2 projection = a + ab * segmentT;
        const float distance = (point - projection).Length();

        if (distance < best.distance) {
            const float widthA = widthProfile[std::min(i, widthProfile.size() - 1)];
            const float widthB = widthProfile[std::min(i + 1, widthProfile.size() - 1)];
            best.distance = distance;
            best.width = Lerp(widthA, widthB, segmentT);
            best.longitudinalT = pointCount > 1
                ? (static_cast<float>(i) + segmentT) / static_cast<float>(pointCount - 1)
                : 0.0f;
            best.valid = true;
        }
    }

    return best;
}

RiverChannelSample ComputeRiverChannelSample(
    const Vector2& point,
    const std::vector<std::vector<float>>& polylines,
    const std::vector<std::vector<float>>& widthProfiles)
{
    RiverChannelSample best;
    for (size_t i = 0; i < polylines.size(); ++i) {
        const std::vector<float>& polyline = polylines[i];
        const std::vector<float>& widthProfile = i < widthProfiles.size() ? widthProfiles[i] : std::vector<float>{};
        const RiverChannelSample sample = ComputeRiverChannelSample(point, polyline, widthProfile);
        if (sample.valid && sample.distance < best.distance) {
            best = sample;
        }
    }
    return best;
}

} // namespace

TerrainGenerationSettings ProceduralTerrainGenerator::CreateSettingsFromWorldBuildSpec(const WorldBuildSpec& spec)
{
    TerrainGenerationSettings settings;
    settings.resolution = spec.world.heightmapResolution;
    settings.worldWidth = spec.world.sizeMetersX;
    settings.worldDepth = spec.world.sizeMetersZ;
    settings.heightScale = spec.terrain.maxHeightMeters;
    settings.seaLevel01 = Clamp01(spec.world.seaLevel);
    settings.baseHeight01 = Clamp01(settings.seaLevel01 + 0.07f + spec.terrain.relief * 0.08f);
    settings.riverWidth = Lerp(spec.hydrology.riverWidthMinMeters, spec.hydrology.riverWidthMaxMeters, 0.5f);
    settings.riverDepth = Lerp(spec.hydrology.riverDepthMinMeters, spec.hydrology.riverDepthMaxMeters, 0.5f);
    settings.grassClusterBudget = static_cast<uint32_t>(900.0f + spec.vegetation.grassDensity * 3800.0f);
    settings.seed = spec.seed;
    settings.relief = spec.terrain.relief;
    settings.mountainDensity = spec.terrain.mountainDensity;
    settings.mountainScale = spec.terrain.mountainScale;
    settings.hillDensity = spec.terrain.hillDensity;
    settings.flatAreaRatio = spec.terrain.flatAreaRatio;
    settings.roughness = spec.terrain.roughness;
    settings.erosionStrength = spec.terrain.erosionStrength;
    settings.cliffFrequency = spec.terrain.cliffFrequency;
    settings.ridgeDirectionDegrees = spec.terrain.ridgeDirectionDegrees;
    settings.hasOcean = spec.hydrology.hasOcean;
    settings.oceanCoverage = Clamp01(spec.hydrology.oceanCoverage);
    settings.beachWidth = Clamp01(0.08f + spec.surface.sandNearWater * 0.40f);
    settings.coastalShelf = Clamp01(0.06f + spec.world.seaLevel * 0.30f);
    settings.riverCount = std::max(1u, spec.hydrology.riverCount);
    settings.riverMeander = spec.hydrology.riverMeander;
    settings.grassHeight = spec.vegetation.grassHeight;
    settings.shrubDensity = Clamp01(spec.vegetation.shrubDensity);
    settings.treeDensity = Clamp01(spec.vegetation.treeDensity);
    if (!spec.constraints.mustHaveMainRiver && spec.hydrology.riverCount == 0) {
        settings.riverCount = 0;
    }
    return settings;
}

TerrainGenerationResult ProceduralTerrainGenerator::CreateFromWorldBuildSpec(const WorldBuildSpec& spec)
{
    return CreateOpenWorldLandscape(CreateSettingsFromWorldBuildSpec(spec));
}

TerrainGenerationResult ProceduralTerrainGenerator::CreateOpenWorldLandscape(const TerrainGenerationSettings& settings)
{
    TerrainGenerationResult result;
    result.riverWidth = settings.riverWidth;
    result.riverDepth = settings.riverDepth;
    result.seaLevelWorldY = settings.seaLevel01 * settings.heightScale;

    if (settings.riverCount > 0) {
        result.riverPolylines.reserve(settings.riverCount);
        result.riverWidthProfiles.reserve(settings.riverCount);
        for (uint32_t riverIndex = 0; riverIndex < settings.riverCount; ++riverIndex) {
            std::vector<float> polyline = BuildRiverPolyline(settings, riverIndex);
            result.riverWidthProfiles.push_back(BuildRiverWidthProfile(settings, polyline, riverIndex));
            result.riverPolylines.push_back(std::move(polyline));
        }
    }

    TerrainData terrainData;
    terrainData.heightmap.Resize(settings.resolution, settings.resolution, settings.baseHeight01);
    terrainData.materialLayers = {
        {"grass", "Grass", "", 0.0f, 0.62f, 35.0f},
        {"dirt", "Dirt", "", 0.18f, 0.75f, 48.0f},
        {"rock", "Rock", "", 0.45f, 1.0f, 90.0f}
    };
    terrainData.activeMaterialLayerCount = static_cast<uint32_t>(terrainData.materialLayers.size());

    const float ridgeRadians = settings.ridgeDirectionDegrees * (kPi / 180.0f);
    const float ridgeCos = std::cos(ridgeRadians);
    const float ridgeSin = std::sin(ridgeRadians);

    for (uint32_t z = 0; z < settings.resolution; ++z) {
        for (uint32_t x = 0; x < settings.resolution; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(settings.resolution - 1);
            const float v = static_cast<float>(z) / static_cast<float>(settings.resolution - 1);
            const float worldX = (u - 0.5f) * settings.worldWidth;
            const float worldZ = (v - 0.5f) * settings.worldDepth;

            const float ru = ridgeCos * (u - 0.5f) - ridgeSin * (v - 0.5f);
            const float rv = ridgeSin * (u - 0.5f) + ridgeCos * (v - 0.5f);

            const float broad = FractalNoise((ru + 0.5f) * 2.5f, (rv + 0.5f) * 2.5f, settings.seed + 11u, 4, 2.1f, 0.5f);
            const float detail = FractalNoise((ru + 0.5f) * 8.0f, (rv + 0.5f) * 8.0f, settings.seed + 29u, 3, 2.2f, 0.45f);
            const float ridged = 1.0f - std::abs(FractalNoise((ru + 0.5f) * 3.4f, (rv + 0.5f) * 3.4f, settings.seed + 71u, 4, 2.0f, 0.5f));
            const float cliffNoise = 1.0f - std::abs(FractalNoise((ru + 0.5f) * 11.0f, (rv + 0.5f) * 11.0f, settings.seed + 109u, 2, 2.0f, 0.5f));

            const float primaryMountainMask = SmoothStep(-0.08f, 0.28f, -rv + settings.mountainDensity * 0.24f);
            const float secondaryHillMask = SmoothStep(-0.35f, 0.45f, rv + settings.hillDensity * 0.20f);
            const float radialDistance = std::sqrt(worldX * worldX + worldZ * worldZ) / std::max(1.0f, settings.worldWidth * 0.5f);
            const float centerPlainMask = 1.0f - SmoothStep(0.12f, 0.18f + settings.flatAreaRatio * 1.2f, radialDistance);

            float height01 = settings.baseHeight01;
            height01 += broad * (0.10f + settings.roughness * 0.15f) * settings.relief;
            height01 += detail * (0.03f + settings.roughness * 0.09f);
            height01 += ridged * (0.08f + settings.mountainScale * 0.26f) * primaryMountainMask * settings.mountainDensity;
            height01 += broad * (0.04f + settings.hillDensity * 0.10f) * secondaryHillMask;
            height01 += cliffNoise * settings.cliffFrequency * 0.10f * primaryMountainMask;
            height01 -= centerPlainMask * (0.04f + settings.flatAreaRatio * 0.18f);
            height01 = Lerp(height01, SmoothStep(0.0f, 1.0f, height01), settings.erosionStrength * 0.12f);

            if (!result.riverPolylines.empty()) {
                const RiverChannelSample riverSample = ComputeRiverChannelSample(
                    Vector2(worldX, worldZ),
                    result.riverPolylines,
                    result.riverWidthProfiles);
                if (riverSample.valid) {
                    const float localWidth = std::max(settings.riverWidth * 0.45f, riverSample.width);
                    const float widthRatio = localWidth / std::max(1.0f, settings.riverWidth);
                    const float depthScale = Lerp(0.82f, 1.18f, riverSample.longitudinalT) * std::sqrt(std::max(0.55f, widthRatio));
                    const float riverDepth01 = (settings.riverDepth * depthScale) / std::max(1.0f, settings.heightScale);
                    const float channelCore = 1.0f - SmoothStep(localWidth * 0.26f, localWidth * 0.84f, riverSample.distance);
                    const float channelBank = 1.0f - SmoothStep(localWidth * 0.84f, localWidth * 1.75f, riverSample.distance);
                    const float floodplain = 1.0f - SmoothStep(localWidth * 1.75f, localWidth * 2.65f, riverSample.distance);
                    height01 -= channelCore * (riverDepth01 * 1.95f + 0.016f);
                    height01 -= channelBank * (riverDepth01 * 0.72f + 0.012f);
                    height01 -= floodplain * (riverDepth01 * 0.10f + 0.003f);
                }
            }

            if (settings.hasOcean) {
                const float shorelineV = 1.0f - settings.oceanCoverage;
                const float shelfHalfWidth = RemapClamped(settings.coastalShelf, 0.0f, 1.0f, 0.035f, 0.11f);
                const float beachTransition = RemapClamped(settings.beachWidth, 0.0f, 1.0f, 0.025f, 0.08f);
                const float coastStartV = Clamp01(shorelineV - shelfHalfWidth);
                const float coastEndV = Clamp01(shorelineV + shelfHalfWidth * 0.65f);
                const float beachStartV = Clamp01(coastStartV - beachTransition);
                const float beachEndV = Clamp01(coastStartV + beachTransition * 0.55f);
                const float shorelineMask = SmoothStep(coastStartV, coastEndV, v);
                const float beachMask = SmoothStep(beachStartV, beachEndV, v) * (1.0f - shorelineMask);

                height01 = Lerp(height01, settings.seaLevel01 + 0.012f, beachMask * 0.72f);
                height01 = Lerp(height01, settings.seaLevel01 - 0.035f, shorelineMask);
            }

            terrainData.heightmap.SetSample(x, z, Clamp01(height01));
        }
    }

    result.terrainData = std::move(terrainData);
    return result;
}

} // namespace Moon
