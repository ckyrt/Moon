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

std::vector<float> BuildRiverPolyline(const TerrainGenerationSettings& settings)
{
    std::vector<float> polyline;
    const int controlPointCount = 11;
    polyline.reserve(static_cast<size_t>(controlPointCount) * 3);

    for (int i = 0; i < controlPointCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(controlPointCount - 1);
        const float worldZ = -settings.worldDepth * 0.5f + t * settings.worldDepth;
        const float bendPrimary = std::sin(t * 2.5f * kPi + 0.35f) * settings.worldWidth * 0.12f;
        const float bendSecondary = std::sin(t * 6.0f * kPi + 0.8f) * settings.worldWidth * 0.03f;
        const float worldX = bendPrimary + bendSecondary;

        polyline.push_back(worldX);
        polyline.push_back(0.0f);
        polyline.push_back(worldZ);
    }

    return polyline;
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

} // namespace

TerrainGenerationResult ProceduralTerrainGenerator::CreateOpenWorldLandscape(const TerrainGenerationSettings& settings)
{
    TerrainGenerationResult result;
    result.riverWidth = settings.riverWidth;
    result.riverDepth = settings.riverDepth;
    result.riverPolyline = BuildRiverPolyline(settings);

    TerrainData terrainData;
    terrainData.heightmap.Resize(settings.resolution, settings.resolution, settings.baseHeight01);
    terrainData.materialLayers = {
        {"grass", "Grass", "", 0.0f, 0.62f, 35.0f},
        {"dirt", "Dirt", "", 0.18f, 0.75f, 48.0f},
        {"rock", "Rock", "", 0.45f, 1.0f, 90.0f}
    };
    terrainData.activeMaterialLayerCount = static_cast<uint32_t>(terrainData.materialLayers.size());

    for (uint32_t z = 0; z < settings.resolution; ++z) {
        for (uint32_t x = 0; x < settings.resolution; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(settings.resolution - 1);
            const float v = static_cast<float>(z) / static_cast<float>(settings.resolution - 1);
            const float worldX = (u - 0.5f) * settings.worldWidth;
            const float worldZ = (v - 0.5f) * settings.worldDepth;

            const float broad = FractalNoise(u * 2.5f, v * 2.5f, settings.seed + 11u, 4, 2.1f, 0.5f);
            const float detail = FractalNoise(u * 8.0f, v * 8.0f, settings.seed + 29u, 3, 2.2f, 0.45f);
            const float ridged = 1.0f - std::abs(FractalNoise(u * 3.4f, v * 3.4f, settings.seed + 71u, 4, 2.0f, 0.5f));

            const float northMountains = SmoothStep(0.56f, 0.98f, 1.0f - v);
            const float southHills = SmoothStep(0.48f, 0.92f, v);
            const float westRise = SmoothStep(0.55f, 1.0f, 1.0f - u) * 0.35f;
            const float eastRise = SmoothStep(0.70f, 1.0f, u) * 0.18f;
            const float centerPlainMask = 1.0f - SmoothStep(0.18f, 0.52f, std::sqrt(worldX * worldX + worldZ * worldZ) / (settings.worldWidth * 0.5f));

            float height01 = settings.baseHeight01;
            height01 += broad * 0.18f;
            height01 += detail * 0.05f;
            height01 += ridged * (0.20f * northMountains + 0.07f * southHills);
            height01 += westRise * 0.14f;
            height01 += eastRise * 0.08f;
            height01 -= centerPlainMask * 0.08f;

            const float riverDistance = ComputeRiverDistance(Vector2(worldX, worldZ), result.riverPolyline);
            const float riverMask = 1.0f - SmoothStep(settings.riverWidth * 0.55f, settings.riverWidth * 2.4f, riverDistance);
            height01 -= riverMask * 0.14f;

            const float shorelineMask = SmoothStep(settings.worldDepth * 0.40f, settings.worldDepth * 0.52f, std::abs(worldZ));
            height01 -= shorelineMask * 0.05f;

            terrainData.heightmap.SetSample(x, z, Clamp01(height01));
        }
    }

    result.terrainData = std::move(terrainData);
    return result;
}

} // namespace Moon
