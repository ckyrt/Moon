#pragma once

#include "BuildingTypes.h"
#include <vector>

namespace Moon {
namespace Building {

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
                              float totalHeight,
                              StairGeometry& outGeometry);
    
    void GenerateLStair(const StairConfig& config,
                       float totalHeight,
                       StairGeometry& outGeometry);
    
    void GenerateUStair(const StairConfig& config,
                       float totalHeight,
                       StairGeometry& outGeometry);
    
    void GenerateSpiralStair(const StairConfig& config,
                            float totalHeight,
                            StairGeometry& outGeometry);
    
    float GetFloorHeight(const BuildingDefinition& definition, int level) const;
    
    float CalculateFloorHeightDifference(const BuildingDefinition& definition,
                                        int fromLevel, int toLevel) const;
    
    void ValidateStairGeometry(StairGeometry& geometry);
    
    float m_maxStepHeight;
    float m_minStepDepth;
};

} // namespace Building
} // namespace Moon
