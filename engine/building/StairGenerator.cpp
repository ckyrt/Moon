#include "StairGenerator.h"
#include <cmath>

namespace Moon {
namespace Building {

StairGenerator::StairGenerator()
    : m_maxStepHeight(0.18f)    // 18cm max step height (building code standard)
    , m_minStepDepth(0.28f)     // 28cm min step depth (building code standard)
{
}

StairGenerator::~StairGenerator() {}

void StairGenerator::SetStepParameters(float maxStepHeight, float minStepDepth) {
    m_maxStepHeight = maxStepHeight;
    m_minStepDepth = minStepDepth;
}

void StairGenerator::GenerateStairs(const BuildingDefinition& definition,
                                    std::vector<StairGeometry>& outStairs) {
    outStairs.clear();
    
    for (const auto& floor : definition.floors) {
        for (const auto& space : floor.spaces) {
            if (!space.hasStairs) continue;
            
            // Get floor height
            float floorHeight = floor.floorHeight;
            
            // Generate stair geometry based on type
            StairGeometry geometry;
            geometry.config = space.stairsConfig;
            geometry.position = space.stairsConfig.position;
            geometry.totalHeight = floorHeight;
            
            switch (space.stairsConfig.type) {
                case StairType::Straight:
                    GenerateStraightStair(space.stairsConfig, floorHeight, geometry);
                    break;
                case StairType::L:
                    GenerateLStair(space.stairsConfig, floorHeight, geometry);
                    break;
                case StairType::U:
                    GenerateUStair(space.stairsConfig, floorHeight, geometry);
                    break;
                case StairType::Spiral:
                    GenerateSpiralStair(space.stairsConfig, floorHeight, geometry);
                    break;
            }
            
            outStairs.push_back(geometry);
        }
    }
}

void StairGenerator::GenerateStraightStair(const StairConfig& config,
                                           float floorHeight,
                                           StairGeometry& outGeometry) {
    // Calculate number of steps
    int numSteps = static_cast<int>(std::ceil(floorHeight / m_maxStepHeight));
    
    // Calculate actual step height
    float stepHeight = floorHeight / numSteps;
    float stepDepth = m_minStepDepth;
    
    outGeometry.numSteps = numSteps;
    outGeometry.stepHeight = stepHeight;
    outGeometry.stepDepth = stepDepth;
    
    // No landings for straight stairs (simple implementation)
    outGeometry.landingPositions.clear();
}

void StairGenerator::GenerateLStair(const StairConfig& config,
                                    float floorHeight,
                                    StairGeometry& outGeometry) {
    // L-shaped stair with landing at mid-height
    int numSteps = static_cast<int>(std::ceil(floorHeight / m_maxStepHeight));
    
    float stepHeight = floorHeight / numSteps;
    float stepDepth = m_minStepDepth;
    
    outGeometry.numSteps = numSteps;
    outGeometry.stepHeight = stepHeight;
    outGeometry.stepDepth = stepDepth;
    
    // Add landing at mid-height
    float landingHeight = floorHeight * 0.5f;
    int landingStep = numSteps / 2;
    
    GridPos2D landingPos;
    landingPos[0] = config.position[0];
    landingPos[1] = config.position[1] + stepDepth * landingStep;
    outGeometry.landingPositions.push_back(landingPos);
}

void StairGenerator::GenerateUStair(const StairConfig& config,
                                    float floorHeight,
                                    StairGeometry& outGeometry) {
    // U-shaped stair with landing at mid-height
    int numSteps = static_cast<int>(std::ceil(floorHeight / m_maxStepHeight));
    
    float stepHeight = floorHeight / numSteps;
    float stepDepth = m_minStepDepth;
    
    outGeometry.numSteps = numSteps;
    outGeometry.stepHeight = stepHeight;
    outGeometry.stepDepth = stepDepth;
    
    // Add landing at mid-height
    GridPos2D landingPos;
    landingPos[0] = config.position[0];
    landingPos[1] = config.position[1] + config.width;
    outGeometry.landingPositions.push_back(landingPos);
}

void StairGenerator::GenerateSpiralStair(const StairConfig& config,
                                         float floorHeight,
                                         StairGeometry& outGeometry) {
    // Spiral stair - more complex, using simple calculation
    // Each step rotates around center point
    int numSteps = static_cast<int>(std::ceil(floorHeight / m_maxStepHeight));
    
    float stepHeight = floorHeight / numSteps;
    
    outGeometry.numSteps = numSteps;
    outGeometry.stepHeight = stepHeight;
    outGeometry.stepDepth = 0.25f; // Fixed for spiral
    
    // No landings for spiral stairs
    outGeometry.landingPositions.clear();
}

float StairGenerator::GetFloorHeight(const BuildingDefinition& definition, int level) const {
    for (const auto& floor : definition.floors) {
        if (floor.level == level) {
            return floor.floorHeight;
        }
    }
    return 3.0f; // Default
}

} // namespace Building
} // namespace Moon
