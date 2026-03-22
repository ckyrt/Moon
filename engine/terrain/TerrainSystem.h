#pragma once

#include "ITerrainSystem.h"

namespace Moon {

class TerrainSystem final : public ITerrainSystem {
public:
    TerrainSystem() = default;
    ~TerrainSystem() override = default;

    void SetEnabled(bool enabled) override;
    bool IsEnabled() const override;

    void SetProfile(const TerrainProfile& profile) override;
    const TerrainProfile& GetProfile() const override;

    void SetData(const TerrainData& data) override;
    const TerrainData& GetData() const override;

    void ResizeHeightmap(uint32_t width, uint32_t height, float fillValue) override;
    float GetHeightSample(uint32_t x, uint32_t y) const override;
    bool SetHeightSample(uint32_t x, uint32_t y, float value) override;

    const TerrainRuntimeState& GetRuntimeState() const override;
    const std::vector<TerrainChunkState>& GetChunks() const override;

    void Update(float deltaTimeSeconds) override;

private:
    void RebuildChunkLayout();
    void MarkAllChunksDirty();
    void MarkChunkDirty(uint32_t chunkIndex);
    void RefreshRuntimeState();
    uint32_t GetChunkIndexForSample(uint32_t x, uint32_t y) const;
    static uint32_t ComputeChunkCount(uint32_t sampleCount, uint32_t chunkResolutionQuads);

    TerrainProfile m_profile;
    TerrainData m_data;
    TerrainRuntimeState m_runtimeState;
    std::vector<TerrainChunkState> m_chunks;
};

} // namespace Moon
