#pragma once

#include "TerrainTypes.h"

namespace Moon {

class ITerrainSystem {
public:
    virtual ~ITerrainSystem() = default;

    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsEnabled() const = 0;

    virtual void SetProfile(const TerrainProfile& profile) = 0;
    virtual const TerrainProfile& GetProfile() const = 0;

    virtual void SetData(const TerrainData& data) = 0;
    virtual const TerrainData& GetData() const = 0;

    virtual void ResizeHeightmap(uint32_t width, uint32_t height, float fillValue) = 0;
    virtual float GetHeightSample(uint32_t x, uint32_t y) const = 0;
    virtual bool SetHeightSample(uint32_t x, uint32_t y, float value) = 0;

    virtual const TerrainRuntimeState& GetRuntimeState() const = 0;
    virtual const std::vector<TerrainChunkState>& GetChunks() const = 0;

    virtual void Update(float deltaTimeSeconds) = 0;
};

} // namespace Moon
