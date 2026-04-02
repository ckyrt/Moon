#pragma once

#include "../core/Scene/Component.h"
#include "../core/Math/Vector3.h"
#include "TerrainSystem.h"

namespace Moon {

class TerrainComponent : public Component {
public:
    explicit TerrainComponent(SceneNode* owner);
    ~TerrainComponent() override = default;

    void SetProfile(const TerrainProfile& profile);
    const TerrainProfile& GetProfile() const;

    void SetData(const TerrainData& data);
    const TerrainData& GetData() const;

    void ResizeHeightmap(uint32_t width, uint32_t height, float fillValue);
    float GetHeightSample(uint32_t x, uint32_t y) const;
    bool SetHeightSample(uint32_t x, uint32_t y, float value);
    bool SampleWorldHeightAndNormal(const Vector3& worldPosition, float& outHeight, Vector3& outNormal) const;

    const TerrainRuntimeState& GetRuntimeState() const;
    const std::vector<TerrainChunkState>& GetChunks() const;

    TerrainSystem& GetSystem();
    const TerrainSystem& GetSystem() const;

    void Update(float deltaTime) override;

private:
    TerrainSystem m_system;
};

} // namespace Moon
