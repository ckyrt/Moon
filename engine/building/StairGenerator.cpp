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
            
            // Calculate actual height difference between floors
            int fromLevel = floor.level;
            int toLevel = space.stairsConfig.connectToLevel;
            float actualHeight = CalculateFloorHeightDifference(definition, fromLevel, toLevel);
            
            if (actualHeight <= 0.0f) {
                // Invalid stair connection
                continue;
            }
            
            // Generate stair geometry based on type
            StairGeometry geometry;
            geometry.config = space.stairsConfig;
            geometry.position = space.stairsConfig.position;
            geometry.fromLevel = fromLevel;
            geometry.toLevel = toLevel;
            geometry.totalHeight = actualHeight;
            geometry.rotation = 0.0f; // Default, could be from config
            geometry.stairWidth = space.stairsConfig.width;
            
            switch (space.stairsConfig.type) {
                case StairType::Straight:
                    GenerateStraightStair(space.stairsConfig, actualHeight, geometry);
                    break;
                case StairType::L:
                    GenerateLStair(space.stairsConfig, actualHeight, geometry);
                    break;
                case StairType::U:
                    GenerateUStair(space.stairsConfig, actualHeight, geometry);
                    break;
                case StairType::Spiral:
                    GenerateSpiralStair(space.stairsConfig, actualHeight, geometry);
                    break;
            }
            
            // Validate geometry
            ValidateStairGeometry(geometry);
            
            outStairs.push_back(geometry);
        }
    }
}

void StairGenerator::GenerateStraightStair(const StairConfig& config,
                                           float totalHeight,
                                           StairGeometry& outGeometry) {
    // Calculate number of steps
    int numSteps = static_cast<int>(std::ceil(totalHeight / m_maxStepHeight));
    
    // Calculate actual step dimensions
    float stepHeight = totalHeight / numSteps;
    float stepDepth = m_minStepDepth;
    
    outGeometry.numSteps = numSteps;
    outGeometry.stepHeight = stepHeight;
    outGeometry.stepDepth = stepDepth;
    outGeometry.stairLength = numSteps * stepDepth;
    
    // Generate individual step positions
    outGeometry.steps.clear();
    for (int i = 0; i < numSteps; ++i) {
        StepData step;
        step.position[0] = config.position[0];
        step.position[1] = config.position[1] + i * stepDepth;
        step.height = i * stepHeight;
        step.rotation = 0.0f;
        outGeometry.steps.push_back(step);
    }
    
    // No landings for straight stairs (simple implementation)
    outGeometry.landings.clear();
}

void StairGenerator::GenerateLStair(const StairConfig& config,
                                    float totalHeight,
                                    StairGeometry& outGeometry) {
    // L-shaped stair with landing at mid-height
    int numSteps = static_cast<int>(std::ceil(totalHeight / m_maxStepHeight));
    
    float stepHeight = totalHeight / numSteps;
    float stepDepth = m_minStepDepth;
    
    outGeometry.numSteps = numSteps;
    outGeometry.stepHeight = stepHeight;
    outGeometry.stepDepth = stepDepth;
    
    int stepsBeforeLanding = numSteps / 2;
    int stepsAfterLanding = numSteps - stepsBeforeLanding;
    float landingHeight = stepsBeforeLanding * stepHeight;
    
    // Calculate total length considering the L-shape
    float firstRunLength = stepsBeforeLanding * stepDepth;
    float secondRunLength = stepsAfterLanding * stepDepth;
    outGeometry.stairLength = firstRunLength + secondRunLength + config.width; // Approximate
    
    // Generate steps for first run (along initial direction)
    outGeometry.steps.clear();
    for (int i = 0; i < stepsBeforeLanding; ++i) {
        StepData step;
        step.position[0] = config.position[0];
        step.position[1] = config.position[1] + i * stepDepth;
        step.height = i * stepHeight;
        step.rotation = 0.0f;
        outGeometry.steps.push_back(step);
    }
    
    // Add landing platform
    LandingPlatform landing;
    landing.position[0] = config.position[0];
    landing.position[1] = config.position[1] + firstRunLength;
    landing.width = config.width;
    landing.depth = config.width; // Square landing typically
    landing.height = landingHeight;
    landing.rotation = 0.0f;
    outGeometry.landings.push_back(landing);
    
    // Generate steps for second run (perpendicular direction)
    for (int i = 0; i < stepsAfterLanding; ++i) {
        StepData step;
        step.position[0] = config.position[0] + i * stepDepth; // Now along X
        step.position[1] = config.position[1] + firstRunLength + config.width;
        step.height = landingHeight + i * stepHeight;
        step.rotation = 90.0f; // Rotated 90 degrees
        outGeometry.steps.push_back(step);
    }
}

void StairGenerator::GenerateUStair(const StairConfig& config,
                                    float totalHeight,
                                    StairGeometry& outGeometry) {
    // U-shaped stair with landing at mid-height
    int numSteps = static_cast<int>(std::ceil(totalHeight / m_maxStepHeight));
    
    float stepHeight = totalHeight / numSteps;
    float stepDepth = m_minStepDepth;
    
    outGeometry.numSteps = numSteps;
    outGeometry.stepHeight = stepHeight;
    outGeometry.stepDepth = stepDepth;
    
    int stepsBeforeLanding = numSteps / 2;
    int stepsAfterLanding = numSteps - stepsBeforeLanding;
    float landingHeight = stepsBeforeLanding * stepHeight;
    
    float firstRunLength = stepsBeforeLanding * stepDepth;
    float secondRunLength = stepsAfterLanding * stepDepth;
    outGeometry.stairLength = firstRunLength + secondRunLength;
    
    // Generate steps for first run (going up)
    outGeometry.steps.clear();
    for (int i = 0; i < stepsBeforeLanding; ++i) {
        StepData step;
        step.position[0] = config.position[0];
        step.position[1] = config.position[1] + i * stepDepth;
        step.height = i * stepHeight;
        step.rotation = 0.0f;
        outGeometry.steps.push_back(step);
    }
    
    // Add landing platform
    LandingPlatform landing;
    landing.position[0] = config.position[0];
    landing.position[1] = config.position[1] + firstRunLength;
    landing.width = config.width;
    landing.depth = config.width;
    landing.height = landingHeight;
    landing.rotation = 0.0f;
    outGeometry.landings.push_back(landing);
    
    // Generate steps for second run (coming back parallel)
    for (int i = 0; i < stepsAfterLanding; ++i) {
        StepData step;
        step.position[0] = config.position[0] + config.width; // Offset parallel
        step.position[1] = config.position[1] + firstRunLength - i * stepDepth; // Going back
        step.height = landingHeight + i * stepHeight;
        step.rotation = 180.0f; // Opposite direction
        outGeometry.steps.push_back(step);
    }
}

void StairGenerator::GenerateSpiralStair(const StairConfig& config,
                                         float totalHeight,
                                         StairGeometry& outGeometry) {
    // Spiral stair - each step rotates around center point
    int numSteps = static_cast<int>(std::ceil(totalHeight / m_maxStepHeight));
    
    float stepHeight = totalHeight / numSteps;
    
    // Spiral parameters
    float radius = config.width / 2.0f; // Use stair width as diameter
    float degreesPerStep = 360.0f / 12.0f; // 30 degrees per step typical
    float totalRotation = numSteps * degreesPerStep;
    
    outGeometry.numSteps = numSteps;
    outGeometry.stepHeight = stepHeight;
    outGeometry.stepDepth = radius * 0.5f; // Arc length approximation
    outGeometry.stairLength = radius * 2.0f; // Diameter
    
    // Generate individual steps rotating around center
    outGeometry.steps.clear();
    const float PI = 3.14159265f;
    for (int i = 0; i < numSteps; ++i) {
        StepData step;
        float angle = i * degreesPerStep * PI / 180.0f;
        step.position[0] = config.position[0] + radius * std::cos(angle);
        step.position[1] = config.position[1] + radius * std::sin(angle);
        step.height = i * stepHeight;
        step.rotation = i * degreesPerStep;
        outGeometry.steps.push_back(step);
    }
    
    // No landings for spiral stairs typically
    outGeometry.landings.clear();
}

float StairGenerator::GetFloorHeight(const BuildingDefinition& definition, int level) const {
    for (const auto& floor : definition.floors) {
        if (floor.level == level) {
            return floor.floorHeight;
        }
    }
    return 3.0f; // Default
}

float StairGenerator::CalculateFloorHeightDifference(const BuildingDefinition& definition,
                                                      int fromLevel, int toLevel) const {
    if (toLevel <= fromLevel) {
        return 0.0f; // Invalid connection
    }
    
    float totalHeight = 0.0f;
    for (int level = fromLevel; level < toLevel; ++level) {
        totalHeight += GetFloorHeight(definition, level);
    }
    
    return totalHeight;
}

void StairGenerator::ValidateStairGeometry(StairGeometry& geometry) {
    geometry.meetsCodeRequirements = true;
    geometry.validationNotes.clear();
    
    // Check step height
    if (geometry.stepHeight > m_maxStepHeight + 0.01f) {
        geometry.meetsCodeRequirements = false;
        geometry.validationNotes += "Step height exceeds maximum (" + 
            std::to_string(geometry.stepHeight) + " > " + 
            std::to_string(m_maxStepHeight) + "); ";
    }
    
    if (geometry.stepHeight < 0.10f) {
        geometry.meetsCodeRequirements = false;
        geometry.validationNotes += "Step height too small (< 10cm); ";
    }
    
    // Check step depth
    if (geometry.stepDepth < m_minStepDepth - 0.01f) {
        geometry.meetsCodeRequirements = false;
        geometry.validationNotes += "Step depth below minimum (" + 
            std::to_string(geometry.stepDepth) + " < " + 
            std::to_string(m_minStepDepth) + "); ";
    }
    
    // Check total steps
    if (geometry.numSteps < 2) {
        geometry.meetsCodeRequirements = false;
        geometry.validationNotes += "Too few steps; ";
    }
    
    if (geometry.numSteps > 50) {
        geometry.validationNotes += "Warning: Very many steps (" + 
            std::to_string(geometry.numSteps) + "), consider adding landing; ";
    }
    
    // Verify step data consistency
    if (geometry.steps.size() != static_cast<size_t>(geometry.numSteps)) {
        geometry.meetsCodeRequirements = false;
        geometry.validationNotes += "Step count mismatch; ";
    }
}

} // namespace Building
} // namespace Moon
