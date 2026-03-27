#include "TerrainVisualBuilder.h"

#include "../core/Geometry/MeshGenerator.h"
#include "../core/Mesh/Mesh.h"
#include "../core/Math/Vector2.h"
#include "../core/Math/Vector3.h"

#include <cmath>
#include <memory>
#include <vector>

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

float Lerp(float a, float b, float t)
{
    return a * (1.0f - t) + b * t;
}

float SmoothStep(float edge0, float edge1, float x)
{
    const float t = Clamp01((x - edge0) / std::max(0.0001f, edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

Vector3 NormalizeSafe(const Vector3& value, const Vector3& fallback)
{
    const float length = value.Length();
    if (length <= 0.0001f) {
        return fallback;
    }
    return Vector3(value.x / length, value.y / length, value.z / length);
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t)
{
    return a * (1.0f - t) + b * t;
}

float SampleHeight(const Heightmap& heightmap, float x, float z, const TerrainGenerationSettings& settings)
{
    const float normalizedX = Clamp01((x / settings.worldWidth) + 0.5f);
    const float normalizedZ = Clamp01((z / settings.worldDepth) + 0.5f);

    const float sampleX = normalizedX * static_cast<float>(heightmap.GetWidth() - 1);
    const float sampleZ = normalizedZ * static_cast<float>(heightmap.GetHeight() - 1);
    const int x0 = static_cast<int>(std::floor(sampleX));
    const int z0 = static_cast<int>(std::floor(sampleZ));
    const int x1 = std::min(x0 + 1, static_cast<int>(heightmap.GetWidth() - 1));
    const int z1 = std::min(z0 + 1, static_cast<int>(heightmap.GetHeight() - 1));

    const float tx = sampleX - static_cast<float>(x0);
    const float tz = sampleZ - static_cast<float>(z0);

    const float h00 = heightmap.GetSample(static_cast<uint32_t>(x0), static_cast<uint32_t>(z0));
    const float h10 = heightmap.GetSample(static_cast<uint32_t>(x1), static_cast<uint32_t>(z0));
    const float h01 = heightmap.GetSample(static_cast<uint32_t>(x0), static_cast<uint32_t>(z1));
    const float h11 = heightmap.GetSample(static_cast<uint32_t>(x1), static_cast<uint32_t>(z1));

    const float hx0 = Lerp(h00, h10, tx);
    const float hx1 = Lerp(h01, h11, tx);
    return Lerp(hx0, hx1, tz) * settings.heightScale;
}

float ComputeSlopeScore(const Heightmap& heightmap, float x, float z, const TerrainGenerationSettings& settings)
{
    const float step = settings.worldWidth / static_cast<float>(settings.resolution - 1);
    const float hl = SampleHeight(heightmap, x - step, z, settings);
    const float hr = SampleHeight(heightmap, x + step, z, settings);
    const float hd = SampleHeight(heightmap, x, z - step, settings);
    const float hu = SampleHeight(heightmap, x, z + step, settings);

    const float dx = std::abs(hr - hl);
    const float dz = std::abs(hu - hd);
    return std::sqrt(dx * dx + dz * dz);
}

float ComputeRiverDistance(const Vector2& point, const std::vector<float>& polyline)
{
    if (polyline.size() < 6) {
        return 999999.0f;
    }

    float minDistance = 999999.0f;
    const size_t count = polyline.size() / 3;
    for (size_t i = 0; i + 1 < count; ++i) {
        const Vector2 a(polyline[i * 3 + 0], polyline[i * 3 + 2]);
        const Vector2 b(polyline[(i + 1) * 3 + 0], polyline[(i + 1) * 3 + 2]);
        const Vector2 ab = b - a;
        const float lengthSq = Vector2::Dot(ab, ab);
        float t = 0.0f;
        if (lengthSq > 0.0001f) {
            t = Clamp01(Vector2::Dot(point - a, ab) / lengthSq);
        }
        const Vector2 projection = a + ab * t;
        minDistance = std::min(minDistance, (point - projection).Length());
    }
    return minDistance;
}

float ComputeRiverDistance(const Vector2& point, const std::vector<std::vector<float>>& riverPolylines)
{
    float minDistance = 999999.0f;
    for (const std::vector<float>& polyline : riverPolylines) {
        minDistance = std::min(minDistance, ComputeRiverDistance(point, polyline));
    }
    return minDistance;
}

Vector3 ComputeTerrainTint(
    const Vector3& worldPosition,
    const Heightmap& heightmap,
    const TerrainGenerationResult& generation,
    const TerrainGenerationSettings& settings)
{
    const float normalizedHeight = Clamp01(worldPosition.y / std::max(0.001f, settings.heightScale));
    const float slopeScore = ComputeSlopeScore(heightmap, worldPosition.x, worldPosition.z, settings);
    const float slope01 = SmoothStep(3.0f, 18.0f, slopeScore);
    const float riverDistance = ComputeRiverDistance(Vector2(worldPosition.x, worldPosition.z), generation.riverPolylines);
    const float beachProximity = settings.hasOcean
        ? 1.0f - SmoothStep(
            generation.seaLevelWorldY + settings.heightScale * 0.006f,
            generation.seaLevelWorldY + settings.heightScale * (0.028f + settings.beachWidth * 0.05f),
            worldPosition.y)
        : 0.0f;
    const float underwater = settings.hasOcean && worldPosition.y <= generation.seaLevelWorldY ? 1.0f : 0.0f;

    const float riverWetness = 1.0f - SmoothStep(settings.riverWidth * 0.8f, settings.riverWidth * 2.3f, riverDistance);
    const float highland = SmoothStep(0.54f, 0.84f, normalizedHeight);
    const float grassiness = (1.0f - slope01) * (1.0f - highland) * (1.0f - riverWetness * 0.55f) * (1.0f - beachProximity);
    const float rocky = std::max(slope01, highland * 0.85f);
    const float dirtiness = Clamp01(1.0f - grassiness - rocky * 0.55f - beachProximity * 0.35f);

    const Vector3 grassColor(0.34f, 0.43f, 0.18f);
    const Vector3 dirtColor(0.50f, 0.40f, 0.24f);
    const Vector3 rockColor(0.63f, 0.61f, 0.56f);
    const Vector3 riverbedColor(0.74f, 0.70f, 0.52f);
    const Vector3 beachColor(0.80f, 0.74f, 0.56f);
    const Vector3 shallowWaterColor(0.58f, 0.69f, 0.62f);

    Vector3 tint = grassColor;
    tint = Lerp(tint, dirtColor, Clamp01(dirtiness));
    tint = Lerp(tint, rockColor, Clamp01(rocky));
    tint = Lerp(tint, riverbedColor, Clamp01(riverWetness));
    tint = Lerp(tint, beachColor, Clamp01(beachProximity));
    tint = Lerp(tint, shallowWaterColor, Clamp01(underwater));

    const float sunExposure = 0.94f + normalizedHeight * 0.14f;
    return tint * sunExposure;
}

void AppendGrassBlade(
    std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices,
    const Vector3& center,
    float yawRadians,
    float width,
    float height,
    const Vector3& color)
{
    const float halfWidth = width * 0.5f;
    const Vector3 right(std::cos(yawRadians), 0.0f, std::sin(yawRadians));
    const Vector3 up(0.0f, 1.0f, 0.0f);

    const Vector3 bottomLeft = center - right * halfWidth;
    const Vector3 bottomRight = center + right * halfWidth;
    const Vector3 topLeft = bottomLeft + up * height;
    const Vector3 topRight = bottomRight + up * height;
    const Vector3 normal = NormalizeSafe(Vector3::Cross(up, right), Vector3(0.0f, 0.0f, 1.0f));

    const uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

    Vertex v0(bottomLeft, normal, color, 1.0f, Vector2(0.0f, 1.0f));
    Vertex v1(bottomRight, normal, color, 1.0f, Vector2(1.0f, 1.0f));
    Vertex v2(topRight, normal, color, 1.0f, Vector2(1.0f, 0.0f));
    Vertex v3(topLeft, normal, color, 1.0f, Vector2(0.0f, 0.0f));

    vertices.push_back(v0);
    vertices.push_back(v1);
    vertices.push_back(v2);
    vertices.push_back(v3);

    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 1);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 3);

    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 1);
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 3);
    indices.push_back(baseIndex + 2);
}

} // namespace

std::shared_ptr<Mesh> TerrainVisualBuilder::BuildTerrainMesh(const TerrainGenerationResult& generation, const TerrainGenerationSettings& settings)
{
    const TerrainData& terrainData = generation.terrainData;
    Mesh* raw = MeshGenerator::CreateTerrainFromHeightmap(
        static_cast<int>(terrainData.heightmap.GetWidth()),
        static_cast<int>(terrainData.heightmap.GetHeight()),
        terrainData.heightmap.GetSamples().data(),
        settings.worldWidth,
        settings.worldDepth,
        settings.heightScale,
        true);

    if (!raw) {
        return nullptr;
    }

    std::shared_ptr<Mesh> mesh(raw);
    std::vector<Vertex> vertices = mesh->GetVertices();
    for (Vertex& vertex : vertices) {
        const Vector3 tint = ComputeTerrainTint(vertex.position, terrainData.heightmap, generation, settings);
        vertex.colorR = tint.x;
        vertex.colorG = tint.y;
        vertex.colorB = tint.z;
        vertex.colorA = 1.0f;
    }

    mesh->SetVertices(std::move(vertices));
    return mesh;
}

std::shared_ptr<Mesh> TerrainVisualBuilder::BuildRiverMesh(const TerrainGenerationResult& generation, const TerrainGenerationSettings& settings)
{
    if (generation.riverPolylines.empty()) {
        return nullptr;
    }

    std::shared_ptr<Mesh> merged = std::make_shared<Mesh>();
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (const std::vector<float>& riverPolyline : generation.riverPolylines) {
        if (riverPolyline.empty()) {
            continue;
        }

        Mesh* raw = MeshGenerator::CreateRiverFromPolyline(
            riverPolyline,
            generation.riverWidth,
            generation.riverDepth,
            generation.terrainData.heightmap.GetSamples().data(),
            static_cast<int>(generation.terrainData.heightmap.GetWidth()),
            settings.worldWidth,
            settings.worldDepth);

        if (!raw) {
            continue;
        }

        std::shared_ptr<Mesh> riverMesh(raw);
        const uint32_t indexOffset = static_cast<uint32_t>(vertices.size());
        const std::vector<Vertex>& riverVertices = riverMesh->GetVertices();
        const std::vector<uint32_t>& riverIndices = riverMesh->GetIndices();
        vertices.insert(vertices.end(), riverVertices.begin(), riverVertices.end());
        for (uint32_t index : riverIndices) {
            indices.push_back(index + indexOffset);
        }
    }

    merged->SetVertices(std::move(vertices));
    merged->SetIndices(std::move(indices));
    return merged;
}

std::shared_ptr<Mesh> TerrainVisualBuilder::BuildOceanMesh(const TerrainGenerationResult& generation, const TerrainGenerationSettings& settings)
{
    if (!settings.hasOcean) {
        return nullptr;
    }

    Mesh* raw = MeshGenerator::CreatePlane(
        settings.worldWidth * 1.02f,
        settings.worldDepth * 1.02f,
        8,
        8,
        Vector3(1.0f, 1.0f, 1.0f));

    if (!raw) {
        return nullptr;
    }

    std::shared_ptr<Mesh> mesh(raw);
    std::vector<Vertex> vertices = mesh->GetVertices();
    for (Vertex& vertex : vertices) {
        vertex.position.y = generation.seaLevelWorldY;
    }
    mesh->SetVertices(std::move(vertices));
    return mesh;
}

std::shared_ptr<Mesh> TerrainVisualBuilder::BuildGrassMesh(const TerrainData& terrainData, const TerrainGenerationResult& generation, const TerrainGenerationSettings& settings)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(static_cast<size_t>(settings.grassClusterBudget) * 24);
    indices.reserve(static_cast<size_t>(settings.grassClusterBudget) * 72);

    const Heightmap& heightmap = terrainData.heightmap;
    const uint32_t grid = static_cast<uint32_t>(std::sqrt(static_cast<float>(settings.grassClusterBudget)) * 1.2f);
    const uint32_t cells = std::max(8u, grid);

    uint32_t clusterCount = 0;
    for (uint32_t z = 0; z < cells && clusterCount < settings.grassClusterBudget; ++z) {
        for (uint32_t x = 0; x < cells && clusterCount < settings.grassClusterBudget; ++x) {
            const float jitterX = std::sin(static_cast<float>(x * 17 + z * 13)) * 0.35f;
            const float jitterZ = std::cos(static_cast<float>(x * 11 + z * 19)) * 0.35f;
            const float u = (static_cast<float>(x) + 0.5f + jitterX) / static_cast<float>(cells);
            const float v = (static_cast<float>(z) + 0.5f + jitterZ) / static_cast<float>(cells);
            const float worldX = (u - 0.5f) * settings.worldWidth;
            const float worldZ = (v - 0.5f) * settings.worldDepth;

            const float normalizedHeight = SampleHeight(heightmap, worldX, worldZ, settings) / std::max(0.001f, settings.heightScale);
            const float worldY = normalizedHeight * settings.heightScale;
            const float slopeScore = ComputeSlopeScore(heightmap, worldX, worldZ, settings);
            const float riverDistance = ComputeRiverDistance(Vector2(worldX, worldZ), generation.riverPolylines);
            const bool nearBeach =
                settings.hasOcean &&
                worldY <= generation.seaLevelWorldY + settings.heightScale * (0.035f + settings.beachWidth * 0.03f);

            if (normalizedHeight < 0.20f || normalizedHeight > 0.58f) {
                continue;
            }
            if (slopeScore > 12.0f) {
                continue;
            }
            if (riverDistance < settings.riverWidth * 1.1f) {
                continue;
            }
            if (nearBeach) {
                continue;
            }

            const float baseY = SampleHeight(heightmap, worldX, worldZ, settings);
            const Vector3 center(worldX, baseY + 0.08f, worldZ);
            const float patchHeight = 1.2f + 0.8f * std::sin(static_cast<float>(x * 5 + z * 3));
            const float patchWidth = 0.28f + 0.08f * std::cos(static_cast<float>(x * 7 + z * 9));
            const Vector3 grassColor(
                0.22f + 0.05f * std::sin(static_cast<float>(x)),
                0.48f + 0.10f * std::cos(static_cast<float>(z)),
                0.16f);

            const float yaw0 = static_cast<float>(x * 13 + z * 29) * 0.17f;
            const float yaw1 = yaw0 + kPi * 0.5f;
            const float yaw2 = yaw0 + kPi * 0.25f;

            AppendGrassBlade(vertices, indices, center, yaw0, patchWidth, patchHeight, grassColor);
            AppendGrassBlade(vertices, indices, center, yaw1, patchWidth, patchHeight * 0.95f, grassColor);
            AppendGrassBlade(vertices, indices, center, yaw2, patchWidth * 0.85f, patchHeight * 0.82f, grassColor);

            ++clusterCount;
        }
    }

    std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    return mesh;
}

} // namespace Moon
