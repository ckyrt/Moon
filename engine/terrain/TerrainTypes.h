#pragma once

#include "Heightmap.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Moon {

enum class TerrainBrushOperation {
    Raise,
    Lower,
    Smooth,
    Flatten,
    PaintLayer
};

struct TerrainMaterialLayer {
    std::string id;
    std::string displayName;
    std::string materialAsset;
    float minHeight = 0.0f;
    float maxHeight = 1.0f;
    float maxSlopeDegrees = 90.0f;
};

struct TerrainChunkCoord {
    int x = 0;
    int z = 0;

    bool operator==(const TerrainChunkCoord& other) const {
        return x == other.x && z == other.z;
    }
};

struct TerrainChunkState {
    TerrainChunkCoord coord;
    bool heightDirty = true;
    bool materialDirty = true;
    bool vegetationDirty = true;
    uint32_t revision = 0;
};

struct TerrainProfile {
    std::string name = "DefaultTerrain";
    uint32_t chunkResolutionQuads = 63;
    float chunkWorldSize = 64.0f;
    float worldWidth = 0.0f;
    float worldDepth = 0.0f;
    float heightScale = 512.0f;
    float defaultNormalizedHeight = 0.5f;
    bool enableRuntimeEditing = true;
    bool reserveCaveSupport = true;
};

struct TerrainData {
    Heightmap heightmap;
    std::vector<TerrainMaterialLayer> materialLayers;
    uint32_t activeMaterialLayerCount = 0;
};

struct TerrainRuntimeState {
    bool enabled = true;
    uint32_t heightRevision = 0;
    uint32_t chunkCountX = 0;
    uint32_t chunkCountZ = 0;
    uint32_t dirtyChunkCount = 0;
};

} // namespace Moon
