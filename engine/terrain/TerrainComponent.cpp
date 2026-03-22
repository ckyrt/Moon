#include "TerrainComponent.h"

namespace Moon {

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
