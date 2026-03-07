#include "BuildingPipeline.h"

namespace Moon {
namespace Building {

BuildingPipeline::BuildingPipeline() {
    // Set default parameters for generators
    m_wallGenerator.SetWallThickness(0.2f);
    m_wallGenerator.SetDefaultWallHeight(3.0f);
    
    m_doorGenerator.SetDefaultDoorSize(0.9f, 2.1f);
    
    m_stairGenerator.SetStepParameters(0.18f, 0.28f);
    
    m_facadeGenerator.SetWindowParameters(1.2f, 1.5f, 0.9f);
    
    m_layoutValidator.SetMinRoomSize(1.5f, 1.5f);
    m_layoutValidator.SetWallThickness(0.2f);
}

BuildingPipeline::~BuildingPipeline() {}

bool BuildingPipeline::ProcessBuilding(const std::string& jsonStr,
                                       GeneratedBuilding& outBuilding,
                                       std::string& outError) {
    // Step 1: Validate and parse JSON schema
    BuildingDefinition definition;
    if (!m_schemaValidator.ValidateAndParse(jsonStr, definition, outError)) {
        return false;
    }
    
    // Process the parsed definition
    return ProcessBuilding(definition, outBuilding, outError);
}

bool BuildingPipeline::ProcessBuilding(const BuildingDefinition& definition,
                                       GeneratedBuilding& outBuilding,
                                       std::string& outError) {
    // Step 2: Validate layout
    ValidationResult layoutResult = m_layoutValidator.Validate(definition);
    if (!layoutResult.valid) {
        outError = "Layout validation failed:\n";
        for (const auto& error : layoutResult.errors) {
            outError += "  - " + error + "\n";
        }
        return false;
    }
    
    // Log warnings if any
    if (!layoutResult.warnings.empty()) {
        // In a full implementation, would log these warnings
        // For now, continue processing
    }
    
    // Store definition
    outBuilding.definition = definition;
    
    // Step 3: Process masses and floors (implicit, data already in definition)
    if (!ProcessMassAndFloors(definition)) {
        outError = "Mass/Floor processing failed";
        return false;
    }
    
    // Step 4: Build space connectivity graph
    if (!BuildSpaceGraph(definition, outBuilding)) {
        outError = "Space graph construction failed";
        return false;
    }
    
    // Step 5: Generate walls
    if (!GenerateWalls(definition, outBuilding)) {
        outError = "Wall generation failed";
        return false;
    }
    
    // Step 6: Generate doors
    if (!GenerateDoors(definition, outBuilding)) {
        outError = "Door generation failed";
        return false;
    }
    
    // Step 7: Generate stairs
    if (!GenerateStairs(definition, outBuilding)) {
        outError = "Stair generation failed";
        return false;
    }
    
    // Step 8: Generate facade
    if (!GenerateFacade(definition, outBuilding)) {
        outError = "Facade generation failed";
        return false;
    }
    
    return true;
}

bool BuildingPipeline::ValidateOnly(const std::string& jsonStr,
                                    ValidationResult& outResult) {
    // Parse schema
    BuildingDefinition definition;
    std::string parseError;
    if (!m_schemaValidator.ValidateAndParse(jsonStr, definition, parseError)) {
        outResult.valid = false;
        outResult.errors.push_back(parseError);
        return false;
    }
    
    // Validate layout
    outResult = m_layoutValidator.Validate(definition);
    return outResult.valid;
}

bool BuildingPipeline::ProcessMassAndFloors(const BuildingDefinition& definition) {
    // Mass and floor data is already in the definition
    // This step would perform any additional processing if needed
    // For now, just validate that masses and floors exist
    if (definition.masses.empty()) {
        return false;
    }
    if (definition.floors.empty()) {
        return false;
    }
    return true;
}

bool BuildingPipeline::BuildSpaceGraph(const BuildingDefinition& definition,
                                       GeneratedBuilding& outBuilding) {
    m_spaceGraphBuilder.BuildGraph(definition, outBuilding.connections);
    return true;
}

bool BuildingPipeline::GenerateWalls(const BuildingDefinition& definition,
                                     GeneratedBuilding& outBuilding) {
    m_wallGenerator.GenerateWalls(definition, m_spaceGraphBuilder, outBuilding.walls);
    return true;
}

bool BuildingPipeline::GenerateDoors(const BuildingDefinition& definition,
                                     GeneratedBuilding& outBuilding) {
    // Build index for fast lookups (O(1) instead of O(n) scans)
    BuildingIndex index;
    index.Build(definition, &m_spaceGraphBuilder, &outBuilding.walls);
    
    m_doorGenerator.GenerateDoors(definition, m_spaceGraphBuilder, 
                                  outBuilding.walls, index, outBuilding.doors);
    return true;
}

bool BuildingPipeline::GenerateStairs(const BuildingDefinition& definition,
                                      GeneratedBuilding& outBuilding) {
    m_stairGenerator.GenerateStairs(definition, m_stairs);
    // Copy stairs to output building
    outBuilding.stairs = m_stairs;
    return true;
}

bool BuildingPipeline::GenerateFacade(const BuildingDefinition& definition,
                                      GeneratedBuilding& outBuilding) {
    // Build index for fast lookups
    BuildingIndex index;
    index.Build(definition, &m_spaceGraphBuilder, &outBuilding.walls);
    
    m_facadeGenerator.GenerateFacade(definition, outBuilding.walls, index,
                                     outBuilding.windows, m_facadeElements);
    return true;
}

} // namespace Building
} // namespace Moon
