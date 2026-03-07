#pragma once

#include "BuildingTypes.h"
#include <vector>

namespace Moon {
namespace Building {

/**
 * @brief Landing platform data
 */
struct LandingPlatform {
    GridPos2D position;         // Center position
    float width;                // Platform width
    float depth;                // Platform depth
    float height;               // Height above base level
    float rotation;             // Rotation in degrees
};

/**
 * @brief Individual step data
 */
struct StepData {
    GridPos2D position;         // Step center position
    float height;               // Height above base level
    float rotation;             // Rotation in degrees (for spiral)
};

/**
 * @brief Stair geometry data
 */
struct StairGeometry {
    StairConfig config;
    GridPos2D position;         // Base position
    int fromLevel;              // Starting floor level
    int toLevel;                // Target floor level
    float totalHeight;          // Actual height difference between floors
    float rotation;             // Base rotation in degrees
    int numSteps;
    float stepHeight;           // Actual step rise
    float stepDepth;            // Actual step depth/run
    float stairLength;          // Total length along main direction
    float stairWidth;           // Stair width
    
    // Detailed geometry
    std::vector<StepData> steps;            // Individual step positions
    std::vector<LandingPlatform> landings;  // Landing platforms
    
    // Validation flags
    bool meetsCodeRequirements;  // Does it meet building codes?
    std::string validationNotes; // Any warnings or issues
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
