#pragma once

#include "BuildingTypes.h"
#include "SchemaValidator.h"
#include "LayoutValidator.h"
#include "SpaceGraphBuilder.h"
#include "WallGenerator.h"
#include "DoorGenerator.h"
#include "StairGenerator.h"
#include "FacadeGenerator.h"
#include <string>
#include <memory>

namespace Moon {
namespace Building {

/**
 * @brief Building Pipeline
 * Main class that orchestrates the entire building generation pipeline
 * 
 * Pipeline stages:
 * 1. AI JSON Generation (external)
 * 2. Schema Validator
 * 3. Layout Validator
 * 4. Mass Processor (implicit)
 * 5. Floor Generator (implicit)
 * 6. Space Graph Builder
 * 7. Wall Generator
 * 8. Door Generator
 * 9. Stair Generator
 * 10. Facade Generator
 * 11. Mesh Builder (future)
 * 12. Renderer (external)
 */
class BuildingPipeline {
public:
    BuildingPipeline();
    ~BuildingPipeline();

    /**
     * @brief Process JSON and generate building
     * @param jsonStr Input JSON string (moon_building_v8 format)
     * @param outBuilding Output generated building data
     * @param outError Error message if processing fails
     * @return true if successful, false otherwise
     */
    bool ProcessBuilding(const std::string& jsonStr,
                        GeneratedBuilding& outBuilding,
                        std::string& outError);

    /**
     * @brief Process building definition
     * @param definition Pre-parsed building definition
     * @param outBuilding Output generated building data
     * @param outError Error message if processing fails
     * @return true if successful, false otherwise
     */
    bool ProcessBuilding(const BuildingDefinition& definition,
                        GeneratedBuilding& outBuilding,
                        std::string& outError);

    /**
     * @brief Validate building definition without generating
     * @param jsonStr Input JSON string
     * @param outResult Validation result
     * @return true if valid, false otherwise
     */
    bool ValidateOnly(const std::string& jsonStr,
                     ValidationResult& outResult);

    /**
     * @brief Get schema validator
     */
    SchemaValidator& GetSchemaValidator() { return m_schemaValidator; }

    /**
     * @brief Get layout validator
     */
    LayoutValidator& GetLayoutValidator() { return m_layoutValidator; }

private:
    bool ProcessMassAndFloors(const BuildingDefinition& definition);
    bool BuildSpaceGraph(const BuildingDefinition& definition,
                        GeneratedBuilding& outBuilding);
    bool GenerateWalls(const BuildingDefinition& definition,
                      GeneratedBuilding& outBuilding);
    bool GenerateDoors(const BuildingDefinition& definition,
                      GeneratedBuilding& outBuilding);
    bool GenerateStairs(const BuildingDefinition& definition,
                       GeneratedBuilding& outBuilding);
    bool GenerateFacade(const BuildingDefinition& definition,
                       GeneratedBuilding& outBuilding);

    SchemaValidator m_schemaValidator;
    LayoutValidator m_layoutValidator;
    SpaceGraphBuilder m_spaceGraphBuilder;
    WallGenerator m_wallGenerator;
    DoorGenerator m_doorGenerator;
    StairGenerator m_stairGenerator;
    FacadeGenerator m_facadeGenerator;

    std::vector<StairGeometry> m_stairs;
    std::vector<FacadeElement> m_facadeElements;
};

} // namespace Building
} // namespace Moon
