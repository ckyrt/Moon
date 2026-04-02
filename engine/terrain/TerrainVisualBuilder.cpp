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

struct TerrainSurfaceSample {
    Vector3 tint = Vector3(1.0f, 1.0f, 1.0f);
    float wetnessMask = 0.0f;
};

struct AtlasRect {
    float u0;
    float v0;
    float u1;
    float v1;
    float contentAspect;
    float pivotX;
};

float Hash01(float x, float y)
{
    const float h = std::sin(x * 127.1f + y * 311.7f) * 43758.5453123f;
    return h - std::floor(h);
}

float HashSigned(float x, float y)
{
    return Hash01(x, y) * 2.0f - 1.0f;
}

struct RiverChannelSample {
    float distance = 999999.0f;
    float width = 0.0f;
    float longitudinalT = 0.0f;
    bool valid = false;
};

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

RiverChannelSample ComputeRiverChannelSample(
    const Vector2& point,
    const std::vector<float>& polyline,
    const std::vector<float>& widthProfile)
{
    RiverChannelSample best;
    if (polyline.size() < 6 || widthProfile.empty()) {
        return best;
    }

    const size_t count = polyline.size() / 3;
    for (size_t i = 0; i + 1 < count; ++i) {
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
            best.longitudinalT = count > 1
                ? (static_cast<float>(i) + segmentT) / static_cast<float>(count - 1)
                : 0.0f;
            best.valid = true;
        }
    }

    return best;
}

RiverChannelSample ComputeRiverChannelSample(
    const Vector2& point,
    const TerrainGenerationResult& generation)
{
    RiverChannelSample best;
    for (size_t i = 0; i < generation.riverPolylines.size(); ++i) {
        const std::vector<float>& polyline = generation.riverPolylines[i];
        const std::vector<float>& widthProfile =
            i < generation.riverWidthProfiles.size() ? generation.riverWidthProfiles[i] : std::vector<float>{};
        const RiverChannelSample sample = ComputeRiverChannelSample(point, polyline, widthProfile);
        if (sample.valid && sample.distance < best.distance) {
            best = sample;
        }
    }
    return best;
}

TerrainSurfaceSample ComputeTerrainSurfaceSample(
    const Vector3& worldPosition,
    const Heightmap& heightmap,
    const TerrainGenerationResult& generation,
    const TerrainGenerationSettings& settings)
{
    const float normalizedHeight = Clamp01(worldPosition.y / std::max(0.001f, settings.heightScale));
    const float slopeScore = ComputeSlopeScore(heightmap, worldPosition.x, worldPosition.z, settings);
    const float slope01 = SmoothStep(3.0f, 18.0f, slopeScore);
    const RiverChannelSample riverSample = ComputeRiverChannelSample(Vector2(worldPosition.x, worldPosition.z), generation);
    const float beachProximity = settings.hasOcean
        ? 1.0f - SmoothStep(
            generation.seaLevelWorldY + settings.heightScale * 0.006f,
            generation.seaLevelWorldY + settings.heightScale * (0.028f + settings.beachWidth * 0.05f),
            worldPosition.y)
        : 0.0f;
    const float underwater = settings.hasOcean && worldPosition.y <= generation.seaLevelWorldY ? 1.0f : 0.0f;

    const float riverInfluenceWidth = riverSample.valid ? std::max(settings.riverWidth * 0.45f, riverSample.width) : settings.riverWidth;
    const float riverWetness = riverSample.valid
        ? 1.0f - SmoothStep(riverInfluenceWidth * 0.72f, riverInfluenceWidth * 2.45f, riverSample.distance)
        : 0.0f;
    const float shorelineWetness = settings.hasOcean
        ? 1.0f - SmoothStep(
            generation.seaLevelWorldY + settings.heightScale * 0.004f,
            generation.seaLevelWorldY + settings.heightScale * (0.045f + settings.beachWidth * 0.08f),
            worldPosition.y)
        : 0.0f;
    const float highland = SmoothStep(0.54f, 0.84f, normalizedHeight);
    const float grassiness = (1.0f - slope01) * (1.0f - highland) * (1.0f - riverWetness * 0.55f) * (1.0f - beachProximity);
    const float rocky = std::max(slope01, highland * 0.85f);
    const float dirtiness = Clamp01(1.0f - grassiness - rocky * 0.55f - beachProximity * 0.35f);
    const float sedimentMask = Clamp01(std::max(riverWetness * 0.85f, beachProximity * 0.95f));
    const float wetnessMask = Clamp01(std::max(std::max(riverWetness, shorelineWetness), underwater));

    const Vector3 grassColor(0.34f, 0.43f, 0.18f);
    const Vector3 dirtColor(0.50f, 0.40f, 0.24f);
    const Vector3 rockColor(0.63f, 0.61f, 0.56f);
    const Vector3 riverbankColor(0.58f, 0.47f, 0.28f);
    const Vector3 wetSoilColor(0.33f, 0.27f, 0.20f);
    const Vector3 beachColor(0.80f, 0.74f, 0.56f);
    const Vector3 shallowWaterColor(0.58f, 0.69f, 0.62f);

    Vector3 tint = grassColor;
    tint = Lerp(tint, dirtColor, Clamp01(dirtiness));
    tint = Lerp(tint, rockColor, Clamp01(rocky));
    tint = Lerp(tint, riverbankColor, sedimentMask * 0.65f);
    tint = Lerp(tint, beachColor, Clamp01(beachProximity));
    tint = Lerp(tint, wetSoilColor, wetnessMask * 0.52f);
    tint = Lerp(tint, shallowWaterColor, Clamp01(underwater));

    const float sunExposure = 0.94f + normalizedHeight * 0.14f;
    TerrainSurfaceSample sample;
    sample.tint = tint * sunExposure;
    sample.wetnessMask = wetnessMask;
    return sample;
}

void AppendGrassBlade(
    std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices,
    const Vector3& center,
    float yawRadians,
    float width,
    float height,
    const Vector3& color,
    const AtlasRect& uvRect)
{
    // Create cross-billboard (两个垂直相交的面片) for more realistic look
    const int quadCount = 2; // Two perpendicular quads
    
    for (int quadIndex = 0; quadIndex < quadCount; ++quadIndex) {
        const float angleOffset = quadIndex * (kPi * 0.5f); // 90度旋转
        const float currentYaw = yawRadians + angleOffset;
        
        const Vector3 right(std::cos(currentYaw), 0.0f, std::sin(currentYaw));
        const Vector3 up(0.0f, 1.0f, 0.0f);
        const float leftExtent = width * std::max(0.0f, std::min(1.0f, uvRect.pivotX));
        const float rightExtent = width - leftExtent;

        const Vector3 bottomLeft = center - right * leftExtent;
        const Vector3 bottomRight = center + right * rightExtent;
        const Vector3 topLeft = bottomLeft + up * height;
        const Vector3 topRight = bottomRight + up * height;
        const Vector3 normal = NormalizeSafe(Vector3::Cross(up, right), Vector3(0.0f, 0.0f, 1.0f));

        const uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        Vertex v0(bottomLeft, normal, color, 1.0f, Vector2(uvRect.u0, uvRect.v1));
        Vertex v1(bottomRight, normal, color, 1.0f, Vector2(uvRect.u1, uvRect.v1));
        Vertex v2(topRight, normal, color, 1.0f, Vector2(uvRect.u1, uvRect.v0));
        Vertex v3(topLeft, normal, color, 1.0f, Vector2(uvRect.u0, uvRect.v0));
        v0.colorA = 0.0f;
        v1.colorA = 0.0f;
        v2.colorA = 1.0f;
        v3.colorA = 1.0f;

        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);

        // Double-sided rendering
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
}

AtlasRect PickGrassAtlasRect(float seedX, float seedZ)
{
    // Use full texture for each grass blade (tiling grass texture)
    // Each blade can use a slightly different portion for variety
    static const AtlasRect kGrassRects[] = {
        {0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.50f},   // Full texture - variant 1
        {0.0f, 0.0f, 1.0f, 1.0f, 0.6f, 0.50f},   // Full texture - variant 2
        {0.0f, 0.0f, 1.0f, 1.0f, 0.55f, 0.50f},  // Full texture - variant 3
        {0.0f, 0.0f, 1.0f, 1.0f, 0.65f, 0.50f}   // Full texture - variant 4
    };

    const int index = static_cast<int>(Hash01(seedX * 0.37f, seedZ * 0.41f) * 3.99f);
    return kGrassRects[std::max(0, std::min(index, 3))];
}

AtlasRect PickShrubAtlasRect(float seedX, float seedZ)
{
    // Use full texture for each shrub blade (tiling grass texture)
    static const AtlasRect kShrubRects[] = {
        {0.0f, 0.0f, 1.0f, 1.0f, 0.4f, 0.50f},   // Full texture - variant 1
        {0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.50f},   // Full texture - variant 2
        {0.0f, 0.0f, 1.0f, 1.0f, 0.45f, 0.50f},  // Full texture - variant 3
        {0.0f, 0.0f, 1.0f, 1.0f, 0.55f, 0.50f}   // Full texture - variant 4
    };

    const int index = static_cast<int>(Hash01(seedX * 0.29f + 7.0f, seedZ * 0.33f + 3.0f) * 3.99f);
    return kShrubRects[std::max(0, std::min(index, 3))];
}

void AppendGrassCluster(
    std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices,
    const Vector3& center,
    float seedX,
    float seedZ,
    float clusterRadius,
    float baseWidth,
    float baseHeight,
    const Vector3& baseColor,
    int bladeCount,
    bool tallAtlas)
{
    for (int bladeIndex = 0; bladeIndex < bladeCount; ++bladeIndex) {
        const float fi = static_cast<float>(bladeIndex);
        const float angle = Hash01(seedX + fi * 11.0f, seedZ + fi * 7.0f) * kPi * 2.0f;
        const float radius = clusterRadius * std::sqrt(Hash01(seedX + fi * 3.0f, seedZ + fi * 5.0f));
        const Vector3 offset(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
        const float yaw = Hash01(seedX + fi * 13.0f, seedZ + fi * 17.0f) * kPi * 2.0f;
        const float width = baseWidth * Lerp(0.72f, 1.24f, Hash01(seedX + fi * 19.0f, seedZ + fi * 23.0f));
        const float height = baseHeight * Lerp(0.68f, 1.28f, Hash01(seedX + fi * 29.0f, seedZ + fi * 31.0f));
        const float hueShift = HashSigned(seedX + fi * 37.0f, seedZ + fi * 41.0f) * 0.05f;
        const float shade = Lerp(0.82f, 1.12f, Hash01(seedX + fi * 43.0f, seedZ + fi * 47.0f));
        const Vector3 color(
            Clamp01((baseColor.x + hueShift * 0.55f) * shade),
            Clamp01((baseColor.y + hueShift) * shade),
            Clamp01((baseColor.z - hueShift * 0.35f) * shade));
        const AtlasRect atlasRect = tallAtlas
            ? PickShrubAtlasRect(seedX + fi * 0.7f, seedZ + fi * 0.9f)
            : PickGrassAtlasRect(seedX + fi * 0.7f, seedZ + fi * 0.9f);
        const float silhouetteScale = tallAtlas ? 0.92f : 0.42f;
        const float tunedWidth = height * atlasRect.contentAspect * width * silhouetteScale;
        AppendGrassBlade(vertices, indices, center + offset, yaw, tunedWidth, height, color, atlasRect);
    }
}

void AppendPrecipitationQuad(
    std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices,
    const Vector3& center,
    float yawRadians,
    const Vector3& randomData,
    float layerAlpha)
{
    const uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
    const Vector3 encodedNormal0(-1.0f, -1.0f, yawRadians);
    const Vector3 encodedNormal1(1.0f, -1.0f, yawRadians);
    const Vector3 encodedNormal2(1.0f, 1.0f, yawRadians);
    const Vector3 encodedNormal3(-1.0f, 1.0f, yawRadians);
    const Vector3 encodedColor(randomData.x, randomData.y, randomData.z);

    Vertex v0(center, encodedNormal0, encodedColor, layerAlpha, Vector2(0.0f, 1.0f));
    Vertex v1(center, encodedNormal1, encodedColor, layerAlpha, Vector2(1.0f, 1.0f));
    Vertex v2(center, encodedNormal2, encodedColor, layerAlpha, Vector2(1.0f, 0.0f));
    Vertex v3(center, encodedNormal3, encodedColor, layerAlpha, Vector2(0.0f, 0.0f));

    v0.position.y = center.y;
    v1.position.y = center.y;
    v2.position.y = center.y;
    v3.position.y = center.y;
    v0.colorA = layerAlpha;
    v1.colorA = layerAlpha;
    v2.colorA = layerAlpha;
    v3.colorA = layerAlpha;

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
        const TerrainSurfaceSample sample =
            ComputeTerrainSurfaceSample(vertex.position, terrainData.heightmap, generation, settings);
        vertex.colorR = sample.tint.x;
        vertex.colorG = sample.tint.y;
        vertex.colorB = sample.tint.z;
        vertex.colorA = sample.wetnessMask;
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

    for (size_t riverIndex = 0; riverIndex < generation.riverPolylines.size(); ++riverIndex) {
        const std::vector<float>& riverPolyline = generation.riverPolylines[riverIndex];
        const size_t pointCount = riverPolyline.size() / 3;
        if (pointCount < 2) {
            continue;
        }

        const uint32_t indexOffset = static_cast<uint32_t>(vertices.size());
        const std::vector<float>& widthProfile =
            riverIndex < generation.riverWidthProfiles.size()
            ? generation.riverWidthProfiles[riverIndex]
            : std::vector<float>{};
        size_t builtPointCount = 0;

        for (size_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
            const size_t base = pointIndex * 3;
            const Vector3 p(riverPolyline[base + 0], riverPolyline[base + 1], riverPolyline[base + 2]);
            const float localWidth = !widthProfile.empty()
                ? widthProfile[std::min(pointIndex, widthProfile.size() - 1)]
                : generation.riverWidth;
            const float halfWidth = localWidth * 0.5f;

            Vector3 dir;
            if (pointIndex == 0) {
                const size_t nextBase = 3;
                dir = Vector3(
                    riverPolyline[nextBase + 0] - p.x,
                    riverPolyline[nextBase + 1] - p.y,
                    riverPolyline[nextBase + 2] - p.z);
            } else if (pointIndex == pointCount - 1) {
                const size_t prevBase = (pointIndex - 1) * 3;
                dir = Vector3(
                    p.x - riverPolyline[prevBase + 0],
                    p.y - riverPolyline[prevBase + 1],
                    p.z - riverPolyline[prevBase + 2]);
            } else {
                const size_t prevBase = (pointIndex - 1) * 3;
                const size_t nextBase = (pointIndex + 1) * 3;
                dir = Vector3(
                    riverPolyline[nextBase + 0] - riverPolyline[prevBase + 0],
                    riverPolyline[nextBase + 1] - riverPolyline[prevBase + 1],
                    riverPolyline[nextBase + 2] - riverPolyline[prevBase + 2]);
            }

            dir.y = 0.0f;
            dir = NormalizeSafe(dir, Vector3(0.0f, 0.0f, 1.0f));
            Vector3 right = NormalizeSafe(Vector3::Cross(Vector3(0.0f, 1.0f, 0.0f), dir), Vector3(1.0f, 0.0f, 0.0f));

            const float centerHeight = SampleHeight(generation.terrainData.heightmap, p.x, p.z, settings);
            const float bankInset = halfWidth * 0.82f;
            const float leftHeight = SampleHeight(generation.terrainData.heightmap, p.x - right.x * bankInset, p.z - right.z * bankInset, settings);
            const float rightHeight = SampleHeight(generation.terrainData.heightmap, p.x + right.x * bankInset, p.z + right.z * bankInset, settings);
            const float bankHeight = std::min(leftHeight, rightHeight);
            const float widthRatio = localWidth / std::max(1.0f, generation.riverWidth);
            const float depthScale = Lerp(0.82f, 1.18f, pointCount > 1 ? static_cast<float>(pointIndex) / static_cast<float>(pointCount - 1) : 0.0f)
                * std::sqrt(std::max(0.55f, widthRatio));
            const float localDepth = generation.riverDepth * depthScale;
            if (settings.hasOcean && pointIndex > 0) {
                const float oceanStopHeight = generation.seaLevelWorldY + localDepth * 0.10f;
                if (centerHeight <= oceanStopHeight) {
                    break;
                }
            }
            const float desiredSurface = centerHeight + localDepth;
            const float minSurface = centerHeight + localDepth * 0.30f;
            const float maxSurface = bankHeight - localDepth * 0.10f;
            const float waterSurfaceY = std::max(minSurface, std::min(desiredSurface, maxSurface));

            Vertex leftVertex(
                Vector3(p.x - right.x * halfWidth, waterSurfaceY, p.z - right.z * halfWidth),
                Vector3(0.0f, 1.0f, 0.0f),
                Vector3(1.0f, 1.0f, 1.0f),
                0.0f,
                Vector2(0.0f, pointCount > 1 ? static_cast<float>(pointIndex) / static_cast<float>(pointCount - 1) : 0.0f));
            Vertex rightVertex = leftVertex;
            rightVertex.position = Vector3(p.x + right.x * halfWidth, waterSurfaceY, p.z + right.z * halfWidth);
            rightVertex.uv.x = 1.0f;

            vertices.push_back(leftVertex);
            vertices.push_back(rightVertex);
            ++builtPointCount;
        }

        if (builtPointCount < 2) {
            vertices.resize(indexOffset);
            continue;
        }

        for (size_t segmentIndex = 0; segmentIndex + 1 < builtPointCount; ++segmentIndex) {
            const uint32_t i0 = indexOffset + static_cast<uint32_t>(segmentIndex * 2 + 0);
            const uint32_t i1 = indexOffset + static_cast<uint32_t>(segmentIndex * 2 + 1);
            const uint32_t i2 = indexOffset + static_cast<uint32_t>((segmentIndex + 1) * 2 + 0);
            const uint32_t i3 = indexOffset + static_cast<uint32_t>((segmentIndex + 1) * 2 + 1);

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
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
        96,
        96,
        Vector3(1.0f, 1.0f, 1.0f));

    if (!raw) {
        return nullptr;
    }

    std::shared_ptr<Mesh> mesh(raw);
    std::vector<Vertex> vertices = mesh->GetVertices();
    for (Vertex& vertex : vertices) {
        vertex.position.y = generation.seaLevelWorldY;
        vertex.colorA = 0.0f;
    }
    mesh->SetVertices(std::move(vertices));
    return mesh;
}

std::shared_ptr<Mesh> TerrainVisualBuilder::BuildGrassMesh(const TerrainData& terrainData, const TerrainGenerationResult& generation, const TerrainGenerationSettings& settings)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Increased reservation for cross-billboard (2 quads per blade)
    vertices.reserve(static_cast<size_t>(settings.grassClusterBudget) * 80);
    indices.reserve(static_cast<size_t>(settings.grassClusterBudget) * 240);

    const Heightmap& heightmap = terrainData.heightmap;
    const uint32_t grid = static_cast<uint32_t>(std::sqrt(static_cast<float>(settings.grassClusterBudget)) * 1.6f);
    const uint32_t cells = std::max(8u, grid);
    const float cellWidth = settings.worldWidth / static_cast<float>(cells);
    const float cellDepth = settings.worldDepth / static_cast<float>(cells);

    uint32_t clusterCount = 0;
    for (uint32_t z = 0; z < cells && clusterCount < settings.grassClusterBudget; ++z) {
        for (uint32_t x = 0; x < cells && clusterCount < settings.grassClusterBudget; ++x) {
            const float baseSeedX = static_cast<float>(x) + 17.0f;
            const float baseSeedZ = static_cast<float>(z) + 31.0f;
            const int attempts = 1 + (Hash01(baseSeedX, baseSeedZ) > 0.55f ? 1 : 0);
            for (int attempt = 0; attempt < attempts && clusterCount < settings.grassClusterBudget; ++attempt) {
                const float seedOffset = static_cast<float>(attempt) * 53.0f;
                const float u = (static_cast<float>(x) + Hash01(baseSeedX + seedOffset, baseSeedZ)) / static_cast<float>(cells);
                const float v = (static_cast<float>(z) + Hash01(baseSeedX, baseSeedZ + seedOffset)) / static_cast<float>(cells);
                const float worldX = (u - 0.5f) * settings.worldWidth;
                const float worldZ = (v - 0.5f) * settings.worldDepth;
                const float baseY = SampleHeight(heightmap, worldX, worldZ, settings);
                const float normalizedHeight = baseY / std::max(0.001f, settings.heightScale);
                const float slopeScore = ComputeSlopeScore(heightmap, worldX, worldZ, settings);
                const float slopeMask = 1.0f - SmoothStep(5.0f, 13.0f, slopeScore);
                const float riverDistance = ComputeRiverDistance(Vector2(worldX, worldZ), generation.riverPolylines);
                const float riverMask = generation.riverPolylines.empty()
                    ? 1.0f
                    : SmoothStep(settings.riverWidth * 1.15f, settings.riverWidth * 3.6f, riverDistance);
                const float lowlandMask = SmoothStep(0.14f, 0.22f, normalizedHeight);
                const float highlandMask = 1.0f - SmoothStep(0.58f, 0.78f, normalizedHeight);
                const bool nearBeach =
                    settings.hasOcean &&
                    baseY <= generation.seaLevelWorldY + settings.heightScale * (0.035f + settings.beachWidth * 0.04f);
                const TerrainSurfaceSample surfaceSample =
                    ComputeTerrainSurfaceSample(Vector3(worldX, baseY, worldZ), heightmap, generation, settings);
                const float wetnessMask = 1.0f - SmoothStep(0.38f, 0.86f, surfaceSample.wetnessMask);
                const float macroNoise = Hash01(worldX * 0.013f + 9.0f, worldZ * 0.013f + 3.0f);
                const float patchNoise = Hash01(worldX * 0.041f + 5.0f, worldZ * 0.041f + 7.0f);
                const float coverage = slopeMask * riverMask * lowlandMask * highlandMask * wetnessMask;
                const float density = coverage * Lerp(0.72f, 1.18f, macroNoise) + patchNoise * 0.18f;

                if (nearBeach || density < 0.42f) {
                    continue;
                }

                const Vector3 center(worldX, baseY + 0.05f, worldZ);
                const float patchHeight = (0.46f + settings.grassHeight * 1.10f) * Lerp(0.84f, 1.16f, macroNoise);
                const float patchWidth = Lerp(0.72f, 1.02f, patchNoise);
                const float clusterRadius = std::min(cellWidth, cellDepth) * Lerp(0.16f, 0.30f, macroNoise);
                const int bladeCount = density > 0.85f ? 7 : (density > 0.62f ? 6 : 5);
                const Vector3 grassColor(
                    Clamp01(surfaceSample.tint.x * 0.72f + 0.05f),
                    Clamp01(surfaceSample.tint.y * 1.12f + 0.14f),
                    Clamp01(surfaceSample.tint.z * 0.66f + 0.03f));

                AppendGrassCluster(
                    vertices,
                    indices,
                    center,
                    worldX,
                    worldZ,
                    clusterRadius,
                    patchWidth,
                    patchHeight,
                    grassColor,
                    bladeCount,
                    false);

                ++clusterCount;
            }
        }
    }

    std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    return mesh;
}

std::shared_ptr<Mesh> TerrainVisualBuilder::BuildShrubMesh(const TerrainData& terrainData, const TerrainGenerationResult& generation, const TerrainGenerationSettings& settings)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const uint32_t shrubBudget = static_cast<uint32_t>(180.0f + settings.shrubDensity * 1400.0f);
    vertices.reserve(static_cast<size_t>(shrubBudget) * 20);
    indices.reserve(static_cast<size_t>(shrubBudget) * 60);

    const Heightmap& heightmap = terrainData.heightmap;
    const uint32_t cells = std::max(10u, static_cast<uint32_t>(std::sqrt(static_cast<float>(shrubBudget)) * 1.35f));

    uint32_t clusterCount = 0;
    for (uint32_t z = 0; z < cells && clusterCount < shrubBudget; ++z) {
        for (uint32_t x = 0; x < cells && clusterCount < shrubBudget; ++x) {
            const float jitterX = std::sin(static_cast<float>(x * 23 + z * 17)) * 0.42f;
            const float jitterZ = std::cos(static_cast<float>(x * 19 + z * 29)) * 0.42f;
            const float u = (static_cast<float>(x) + 0.5f + jitterX) / static_cast<float>(cells);
            const float v = (static_cast<float>(z) + 0.5f + jitterZ) / static_cast<float>(cells);
            const float worldX = (u - 0.5f) * settings.worldWidth;
            const float worldZ = (v - 0.5f) * settings.worldDepth;

            const float baseY = SampleHeight(heightmap, worldX, worldZ, settings);
            const float normalizedHeight = baseY / std::max(0.001f, settings.heightScale);
            const float slopeScore = ComputeSlopeScore(heightmap, worldX, worldZ, settings);
            const float riverDistance = ComputeRiverDistance(Vector2(worldX, worldZ), generation.riverPolylines);
            const bool nearBeach =
                settings.hasOcean &&
                baseY <= generation.seaLevelWorldY + settings.heightScale * (0.05f + settings.beachWidth * 0.04f);

            if (normalizedHeight < 0.18f || normalizedHeight > 0.68f) {
                continue;
            }
            if (slopeScore > 10.5f) {
                continue;
            }
            if (riverDistance < settings.riverWidth * 1.5f) {
                continue;
            }
            if (nearBeach) {
                continue;
            }

            const float moistureBand = 1.0f - SmoothStep(settings.riverWidth * 1.6f, settings.riverWidth * 4.5f, riverDistance);
            const float hillsideBand = SmoothStep(0.24f, 0.58f, normalizedHeight) * (1.0f - SmoothStep(0.62f, 0.78f, normalizedHeight));
            const float placementMask = moistureBand * 0.45f + hillsideBand * 0.75f - slopeScore * 0.025f;
            if (placementMask < 0.22f) {
                continue;
            }

            const Vector3 center(worldX, baseY + 0.18f, worldZ);
            const float patchHeight = 1.1f + 1.2f * Hash01(worldX * 0.023f + 11.0f, worldZ * 0.023f + 13.0f) + placementMask * 0.8f;
            const float patchWidth = 0.55f + 0.45f * Hash01(worldX * 0.037f + 17.0f, worldZ * 0.037f + 19.0f);
            const TerrainSurfaceSample surfaceSample =
                ComputeTerrainSurfaceSample(Vector3(worldX, baseY, worldZ), heightmap, generation, settings);
            const Vector3 shrubColor(
                Clamp01(surfaceSample.tint.x * 0.60f + 0.03f),
                Clamp01(surfaceSample.tint.y * 0.92f + 0.07f + placementMask * 0.10f),
                Clamp01(surfaceSample.tint.z * 0.58f + 0.03f + moistureBand * 0.05f));

            AppendGrassCluster(
                vertices,
                indices,
                center,
                worldX * 0.7f,
                worldZ * 0.7f,
                patchWidth * 0.32f,
                patchWidth * 0.38f,
                patchHeight,
                shrubColor,
                5,
                true);

            ++clusterCount;
        }
    }

    std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    return mesh;
}

std::shared_ptr<Mesh> TerrainVisualBuilder::BuildPrecipitationMesh()
{
    constexpr uint32_t kParticleCount = 4200;
    constexpr float kSpawnRadius = 160.0f;
    constexpr float kSpawnHeight = 84.0f;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(static_cast<size_t>(kParticleCount) * 8);
    indices.reserve(static_cast<size_t>(kParticleCount) * 12);

    for (uint32_t i = 0; i < kParticleCount; ++i) {
        const float fi = static_cast<float>(i);
        const float angle = std::fmod(fi * 2.3999632f, kPi * 2.0f);
        const float radius = std::sqrt((fi + 0.5f) / static_cast<float>(kParticleCount)) * kSpawnRadius;
        const float centerX = std::cos(angle) * radius;
        const float centerZ = std::sin(angle) * radius;
        const float centerY = std::fmod(fi * 7.173f, 1.0f) * kSpawnHeight;
        const float random0 = std::fmod(fi * 0.6180339f, 1.0f);
        const float random1 = std::fmod(fi * 0.4142135f + 0.17f, 1.0f);
        const float random2 = std::fmod(fi * 0.7320508f + 0.31f, 1.0f);
        const float yawA = angle;
        const float yawB = angle + kPi * 0.5f;
        const Vector3 center(centerX, centerY, centerZ);
        const Vector3 randomData(random0, random1, random2);

        AppendPrecipitationQuad(vertices, indices, center, yawA, randomData, 1.0f);
        AppendPrecipitationQuad(vertices, indices, center, yawB, randomData, 0.92f);
        AppendPrecipitationQuad(vertices, indices, center, yawA + kPi * 0.25f, randomData, 0.84f);
    }

    std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    return mesh;
}

} // namespace Moon
