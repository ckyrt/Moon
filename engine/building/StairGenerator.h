#pragma once

#include "BuildingTypes.h"
#include <vector>

namespace Moon {
namespace Building {

/**
 * @brief Stair geometry data
 */
struct StairGeometry {
    StairConfig config;
    GridPos2D position;
    float totalHeight;
    int numSteps;
    float stepHeight;
    float stepDepth;
    std::vector<GridPos2D> landingPositions;
};

/**
 * @brief Stair Generator
 * Generates stair geometry from stair configurations
 * Calculates:
 * - Step height and depth
 * - Number of steps
 * - Landing positions
 * - Railing placement
 */
class StairGenerator {
public:
    StairGenerator();
    ~StairGenerator();

    /**
     * @brief Generate stair geometry from building definition
     * @param definition Building definition
     * @param outStairs Output stair geometry
     */
    void GenerateStairs(const BuildingDefinition& definition,
                       std::vector<StairGeometry>& outStairs);

    /**
     * @brief Set stair parameters
     */
    void SetStepParameters(float maxStepHeight, float minStepDepth);

private:
    void GenerateStraightStair(const StairConfig& config, 
                              float floorHeight,
                              StairGeometry& outGeometry);
    
    void GenerateLStair(const StairConfig& config,
                       float floorHeight,
                       StairGeometry& outGeometry);
    
    void GenerateUStair(const StairConfig& config,
                       float floorHeight,
                       StairGeometry& outGeometry);
    
    void GenerateSpiralStair(const StairConfig& config,
                            float floorHeight,
                            StairGeometry& outGeometry);
    
    float GetFloorHeight(const BuildingDefinition& definition, int level) const;
    
    float m_maxStepHeight;
    float m_minStepDepth;
};

} // namespace Building
} // namespace Moon
