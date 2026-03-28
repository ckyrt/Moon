#pragma once

#include "ProceduralTerrainGenerator.h"

#include <memory>

namespace Moon {

class Mesh;

class TerrainVisualBuilder {
public:
    static std::shared_ptr<Mesh> BuildTerrainMesh(const TerrainGenerationResult& generation, const TerrainGenerationSettings& settings);
    static std::shared_ptr<Mesh> BuildRiverMesh(const TerrainGenerationResult& generation, const TerrainGenerationSettings& settings);
    static std::shared_ptr<Mesh> BuildOceanMesh(const TerrainGenerationResult& generation, const TerrainGenerationSettings& settings);
    static std::shared_ptr<Mesh> BuildGrassMesh(const TerrainData& terrainData, const TerrainGenerationResult& generation, const TerrainGenerationSettings& settings);
    static std::shared_ptr<Mesh> BuildShrubMesh(const TerrainData& terrainData, const TerrainGenerationResult& generation, const TerrainGenerationSettings& settings);
    static std::shared_ptr<Mesh> BuildPrecipitationMesh();
};

} // namespace Moon
