#include "TerrainComponent.h"

#include "../core/Scene/SceneNode.h"
#include "../core/Scene/Transform.h"

#include <algorithm>
#include <cmath>

namespace Moon {

namespace {

float Clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

float Lerp(float a, float b, float t) {
    return a * (1.0f - t) + b * t;
}

}

TerrainComponent::TerrainComponent(SceneNode* owner)
    : Component(owner) {
}

void TerrainComponent::SetProfile(const TerrainProfile& profile) {
    m_system.SetProfile(profile);
}

const TerrainProfile& TerrainComponent::GetProfile() const {
    return m_system.GetProfile();
}

void TerrainComponent::SetData(const TerrainData& data) {
    m_system.SetData(data);
}

const TerrainData& TerrainComponent::GetData() const {
    return m_system.GetData();
}

void TerrainComponent::ResizeHeightmap(uint32_t width, uint32_t height, float fillValue) {
    m_system.ResizeHeightmap(width, height, fillValue);
}

float TerrainComponent::GetHeightSample(uint32_t x, uint32_t y) const {
    return m_system.GetHeightSample(x, y);
}

bool TerrainComponent::SetHeightSample(uint32_t x, uint32_t y, float value) {
    return m_system.SetHeightSample(x, y, value);
}

bool TerrainComponent::SampleWorldHeightAndNormal(const Vector3& worldPosition, float& outHeight, Vector3& outNormal) const {
    const TerrainData& data = m_system.GetData();
    const TerrainProfile& profile = m_system.GetProfile();
    const TerrainRuntimeState& runtimeState = m_system.GetRuntimeState();
    if (data.heightmap.IsEmpty() || runtimeState.chunkCountX == 0 || runtimeState.chunkCountZ == 0) {
        return false;
    }

    Transform* terrainTransform = m_owner ? m_owner->GetTransform() : nullptr;
    const Vector3 terrainOrigin = terrainTransform ? terrainTransform->GetWorldPosition() : Vector3(0.0f, 0.0f, 0.0f);

    const float worldWidth = profile.worldWidth > 0.0f
        ? profile.worldWidth
        : static_cast<float>(runtimeState.chunkCountX) * profile.chunkWorldSize;
    const float worldDepth = profile.worldDepth > 0.0f
        ? profile.worldDepth
        : static_cast<float>(runtimeState.chunkCountZ) * profile.chunkWorldSize;
    if (worldWidth <= 0.0f || worldDepth <= 0.0f) {
        return false;
    }

    const float localX = worldPosition.x - terrainOrigin.x;
    const float localZ = worldPosition.z - terrainOrigin.z;
    const float normalizedX = Clamp01((localX / worldWidth) + 0.5f);
    const float normalizedZ = Clamp01((localZ / worldDepth) + 0.5f);

    const float sampleX = normalizedX * static_cast<float>(data.heightmap.GetWidth() - 1);
    const float sampleZ = normalizedZ * static_cast<float>(data.heightmap.GetHeight() - 1);
    const int x0 = static_cast<int>(std::floor(sampleX));
    const int z0 = static_cast<int>(std::floor(sampleZ));
    const int x1 = std::min(x0 + 1, static_cast<int>(data.heightmap.GetWidth() - 1));
    const int z1 = std::min(z0 + 1, static_cast<int>(data.heightmap.GetHeight() - 1));
    const float tx = sampleX - static_cast<float>(x0);
    const float tz = sampleZ - static_cast<float>(z0);

    const float h00 = data.heightmap.GetSample(static_cast<uint32_t>(x0), static_cast<uint32_t>(z0));
    const float h10 = data.heightmap.GetSample(static_cast<uint32_t>(x1), static_cast<uint32_t>(z0));
    const float h01 = data.heightmap.GetSample(static_cast<uint32_t>(x0), static_cast<uint32_t>(z1));
    const float h11 = data.heightmap.GetSample(static_cast<uint32_t>(x1), static_cast<uint32_t>(z1));
    const float hx0 = Lerp(h00, h10, tx);
    const float hx1 = Lerp(h01, h11, tx);
    const float normalizedHeight = Lerp(hx0, hx1, tz);

    const float texelWorldX = worldWidth / std::max(1u, data.heightmap.GetWidth() - 1);
    const float texelWorldZ = worldDepth / std::max(1u, data.heightmap.GetHeight() - 1);

    const auto sampleNormalized = [&](int sx, int sz) {
        const int clampedX = std::max(0, std::min(sx, static_cast<int>(data.heightmap.GetWidth() - 1)));
        const int clampedZ = std::max(0, std::min(sz, static_cast<int>(data.heightmap.GetHeight() - 1)));
        return data.heightmap.GetSample(static_cast<uint32_t>(clampedX), static_cast<uint32_t>(clampedZ));
    };

    const float hl = sampleNormalized(x0 - 1, z0) * profile.heightScale;
    const float hr = sampleNormalized(x1 + 1, z0) * profile.heightScale;
    const float hd = sampleNormalized(x0, z0 - 1) * profile.heightScale;
    const float hu = sampleNormalized(x0, z1 + 1) * profile.heightScale;

    outHeight = terrainOrigin.y + normalizedHeight * profile.heightScale;
    outNormal = Vector3(-(hr - hl), texelWorldX + texelWorldZ, -(hu - hd)).Normalized();
    return true;
}

const TerrainRuntimeState& TerrainComponent::GetRuntimeState() const {
    return m_system.GetRuntimeState();
}

const std::vector<TerrainChunkState>& TerrainComponent::GetChunks() const {
    return m_system.GetChunks();
}

TerrainSystem& TerrainComponent::GetSystem() {
    return m_system;
}

const TerrainSystem& TerrainComponent::GetSystem() const {
    return m_system;
}

void TerrainComponent::Update(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }

    m_system.SetEnabled(true);
    m_system.Update(deltaTime);
}

} // namespace Moon
