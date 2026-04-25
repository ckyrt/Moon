#include "BuildingPipeline.h"
#include "BuildingGeometryUtils.h"
#include "LayoutResolver.h"
#include "../core/Assets/AssetPaths.h"
#include "../core/Geometry/MeshGenerator.h"
#include "../core/Mesh/Mesh.h"
#include "../massing/MassRuleParser.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace {

bool RectsMatch(const Moon::Building::Rect& a, const Moon::Building::Rect& b) {
    return std::abs(a.origin[0] - b.origin[0]) < 0.001f &&
           std::abs(a.origin[1] - b.origin[1]) < 0.001f &&
           std::abs(a.size[0] - b.size[0]) < 0.001f &&
           std::abs(a.size[1] - b.size[1]) < 0.001f;
}

std::shared_ptr<Moon::Mesh> CreateMassBoxMesh(const Moon::Building::Mass& mass,
                                              const Moon::Building::BuildingDefinition& definition) {
    float totalHeight = 0.0f;
    for (const auto& floor : definition.floors) {
        if (floor.massId == mass.massId) {
            totalHeight = std::max(totalHeight,
                Moon::Building::GetFloorBaseHeight(definition, floor.level) + floor.floorHeight);
        }
    }
    if (totalHeight <= 0.0f) {
        totalHeight = std::max(3.0f, static_cast<float>(mass.floors) * 3.2f);
    }

    std::shared_ptr<Moon::Mesh> mesh(Moon::MeshGenerator::CreateCube(1.0f, Moon::Vector3(1, 1, 1)));
    std::vector<Moon::Vertex> vertices = mesh->GetVertices();
    const float centerX = mass.origin[0] + mass.size[0] * 0.5f;
    const float centerY = totalHeight * 0.5f;
    const float centerZ = mass.origin[1] + mass.size[1] * 0.5f;
    for (auto& vertex : vertices) {
        vertex.position.x = centerX + vertex.position.x * mass.size[0];
        vertex.position.y = centerY + vertex.position.y * totalHeight;
        vertex.position.z = centerZ + vertex.position.z * mass.size[1];
    }
    mesh->SetVertices(std::move(vertices));
    return mesh;
}

bool DoesFloorNeedVerticalOpening(const Moon::Building::VerticalTransport& transport, int floorLevel) {
    if (transport.external) {
        return false;
    }

    return floorLevel > transport.floorFrom && floorLevel <= transport.floorTo;
}

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        return std::string();
    }
    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (contents.size() >= 3 &&
        static_cast<unsigned char>(contents[0]) == 0xEF &&
        static_cast<unsigned char>(contents[1]) == 0xBB &&
        static_cast<unsigned char>(contents[2]) == 0xBF) {
        contents.erase(0, 3);
    }
    return contents;
}

void ParseOptionalSemanticInputs(const std::string& jsonStr,
                                 Moon::Building::BuildingFormInput& outFormInput,
                                 Moon::Building::BuildingLayoutInput& outLayoutInput,
                                 const Moon::Building::BuildingFormInput*& outFormPtr,
                                 const Moon::Building::BuildingLayoutInput*& outLayoutPtr) {
    Moon::Building::SemanticBuilding semanticBuilding;
    std::string semanticError;
    outFormPtr = nullptr;
    outLayoutPtr = nullptr;
    if (!Moon::Building::SemanticBuildingParser::ParseFromString(jsonStr, semanticBuilding, semanticError)) {
        return;
    }

    outFormInput = Moon::Building::ExtractBuildingFormInput(semanticBuilding);
    outLayoutInput = Moon::Building::ExtractBuildingLayoutInput(semanticBuilding);
    outFormPtr = &outFormInput;
    outLayoutPtr = &outLayoutInput;
}

std::string FormatValidationErrors(const Moon::Building::ValidationResult& result) {
    std::string error = "Layout validation failed:\n";
    for (const auto& item : result.errors) {
        error += "  - " + item + "\n";
    }
    return error;
}

void AddTransportVoidsToFloorPlates(std::vector<Moon::Building::FloorPlate>& plates,
                                    const std::vector<Moon::Building::VerticalTransport>& transports) {
    for (auto& plate : plates) {
        for (const auto& transport : transports) {
            if (!DoesFloorNeedVerticalOpening(transport, plate.floorLevel)) {
                continue;
            }

            const Moon::Building::Rect& openingRect = Moon::Building::GetTransportOpeningRect(transport);
            const auto existingVoid = std::find_if(plate.voids.begin(), plate.voids.end(),
                [&](const Moon::Building::Rect& voidRect) { return RectsMatch(voidRect, openingRect); });
            if (existingVoid == plate.voids.end()) {
                plate.voids.push_back(openingRect);
            }
        }
    }
}

void RemoveColumnsInsideTransportShafts(Moon::Building::GeneratedBuilding& generated) {
    generated.supportColumns.erase(
        std::remove_if(
            generated.supportColumns.begin(),
            generated.supportColumns.end(),
            [&](const Moon::Building::SupportColumn& column) {
                return std::any_of(
                    generated.verticalTransports.begin(),
                    generated.verticalTransports.end(),
                    [&](const Moon::Building::VerticalTransport& transport) {
                        return column.floorTo >= transport.floorFrom &&
                               column.floorFrom <= transport.floorTo &&
                               Moon::Building::RectContainsPoint(transport.shaftRect, column.center);
                    });
            }),
        generated.supportColumns.end());
}

void RemoveSolidFloorMeshesForVoidedPlates(Moon::Building::GeneratedBuilding& generated) {
    std::unordered_set<int> voidedLevels;
    for (const auto& plate : generated.floorPlates) {
        if (!plate.voids.empty()) {
            voidedLevels.insert(plate.floorLevel);
        }
    }

    if (voidedLevels.empty()) {
        return;
    }

    generated.floorPlateMeshes.erase(
        std::remove_if(
            generated.floorPlateMeshes.begin(),
            generated.floorPlateMeshes.end(),
            [&](const Moon::Building::GeneratedMeshPart& part) {
                constexpr const char* prefix = "floor_plate_mesh_";
                if (part.partId.rfind(prefix, 0) != 0) {
                    return false;
                }

                const std::string suffix = part.partId.substr(std::char_traits<char>::length(prefix));
                const int floorLevel = std::atoi(suffix.c_str());
                return voidedLevels.count(floorLevel) > 0;
            }),
        generated.floorPlateMeshes.end());
}

} // namespace

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
    BuildingDefinition definition;
    if (!m_schemaValidator.ValidateAndParse(jsonStr, definition, outError)) {
        return false;
    }

    BuildingFormInput parsedFormInput;
    BuildingLayoutInput parsedLayoutInput;
    const BuildingFormInput* formInput = nullptr;
    const BuildingLayoutInput* layoutInput = nullptr;
    ParseOptionalSemanticInputs(jsonStr, parsedFormInput, parsedLayoutInput, formInput, layoutInput);

    return ProcessBuildingInternal(definition, formInput, layoutInput, outBuilding, outError);
}

bool BuildingPipeline::ProcessBuilding(const BuildingDefinition& definition,
                                       GeneratedBuilding& outBuilding,
                                       std::string& outError) {
    return ProcessBuildingInternal(definition, nullptr, nullptr, outBuilding, outError);
}

bool BuildingPipeline::ProcessBuildingInternal(const BuildingDefinition& definition,
                                              const BuildingFormInput* formInput,
                                              const BuildingLayoutInput* layoutInput,
                                              GeneratedBuilding& outBuilding,
                                              std::string& outError) {
    BuildingDefinition workingDefinition = definition;

    ValidationResult layoutResult = m_layoutValidator.Validate(workingDefinition);
    if (!layoutResult.valid) {
        outError = FormatValidationErrors(layoutResult);
        return false;
    }

    outBuilding.definition = workingDefinition;
    outBuilding.verticalTransports = workingDefinition.verticalTransports;

    if (workingDefinition.masses.empty() || workingDefinition.floors.empty()) {
        outError = "Mass/Floor processing failed";
        return false;
    }

    if (!GenerateStructuralPlan(workingDefinition, outBuilding)) {
        outError = "Structural planning failed";
        return false;
    }

    if (!GenerateEnvelopeMeshes(workingDefinition, outBuilding, outError)) {
        return false;
    }

    ApplyMassDrivenSemanticLayout(workingDefinition, formInput, layoutInput, outBuilding, outError);
    if (!outError.empty()) {
        return false;
    }
    outBuilding.definition = workingDefinition;
    RemoveSolidFloorMeshesForVoidedPlates(outBuilding);
    
    if (!BuildSpaceGraph(workingDefinition, outBuilding)) {
        outError = "Space graph construction failed";
        return false;
    }

    if (!GenerateWalls(workingDefinition, outBuilding)) {
        outError = "Wall generation failed";
        return false;
    }

    if (!GenerateDoors(workingDefinition, outBuilding)) {
        outError = "Door generation failed";
        return false;
    }

    if (!GenerateStairs(workingDefinition, outBuilding)) {
        outError = "Stair generation failed";
        return false;
    }

    if (!GenerateFacade(workingDefinition, outBuilding)) {
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

bool BuildingPipeline::GenerateStructuralPlan(const BuildingDefinition& definition,
                                             GeneratedBuilding& outBuilding) {
    std::vector<FloorPlate> slicedFloorPlates;
    std::string sliceError;
    if (!m_massFloorPlateGenerator.Generate(definition, slicedFloorPlates, sliceError)) {
        return false;
    }

    std::string structuralError;
    return m_structuralPlanGenerator.Generate(definition, slicedFloorPlates, outBuilding, structuralError);
}

void BuildingPipeline::ApplyMassDrivenSemanticLayout(BuildingDefinition& definition,
                                                     const BuildingFormInput* formInput,
                                                     const BuildingLayoutInput* layoutInput,
                                                     GeneratedBuilding& generated,
                                                     std::string& outError) const {
    if (generated.floorPlates.empty()) {
        return;
    }

    if (!layoutInput) {
        return;
    }

    ResolvedBuildingLayout resolvedLayout;
    m_semanticFloorLayoutGenerator.Generate(
        definition,
        formInput,
        layoutInput,
        generated.floorPlates,
        generated.verticalCores,
        definition.floors,
        &resolvedLayout,
        &generated.programBlocks,
        outError);

    if (!outError.empty()) {
        return;
    }

    std::vector<VerticalTransport> resolvedVerticalTransports;
    BuildVerticalTransportsFromResolvedLayout(resolvedLayout, resolvedVerticalTransports);
    if (!resolvedVerticalTransports.empty()) {
        definition.verticalTransports = std::move(resolvedVerticalTransports);
    }
    generated.verticalTransports = definition.verticalTransports;
    AddTransportVoidsToFloorPlates(generated.floorPlates, generated.verticalTransports);
    RemoveColumnsInsideTransportShafts(generated);

    std::string resolvedLayoutJson;
    if (!SerializeResolvedBuildingLayout(formInput, resolvedLayout, resolvedLayoutJson, outError)) {
        return;
    }
    generated.resolvedLayoutJson = std::move(resolvedLayoutJson);
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
    std::vector<StairGeometry> stairs;
    m_stairGenerator.GenerateStairs(definition, stairs);
    outBuilding.stairs = std::move(stairs);
    return true;
}

bool BuildingPipeline::GenerateFacade(const BuildingDefinition& definition,
                                      GeneratedBuilding& outBuilding) {
    BuildingIndex index;
    index.Build(definition, &m_spaceGraphBuilder, &outBuilding.walls);

    std::vector<FacadeElement> facadeElements;
    m_facadeGenerator.GenerateFacade(definition, outBuilding.walls, index,
                                     outBuilding.windows, facadeElements);
    return true;
}

bool BuildingPipeline::GenerateEnvelopeMeshes(const BuildingDefinition& definition,
                                              GeneratedBuilding& outBuilding,
                                              std::string& outError) {
    outBuilding.envelopeMeshes.clear();

    for (const auto& mass : definition.masses) {
        if (mass.massingRuleAsset.empty()) {
            GeneratedMeshPart part;
            part.partId = mass.massId + "_envelope_box";
            part.material = "envelope_shell";
            part.mesh = CreateMassBoxMesh(mass, definition);
            outBuilding.envelopeMeshes.push_back(std::move(part));
            continue;
        }

        const std::filesystem::path rulePath =
            std::filesystem::path(Moon::Assets::BuildAssetPath("massing")) / mass.massingRuleAsset;
        const std::string ruleJson = ReadTextFile(rulePath);
        if (ruleJson.empty()) {
            outError = "Failed to read massing envelope rule: " + rulePath.string();
            return false;
        }

        Moon::Massing::RuleSet ruleSet;
        if (!Moon::Massing::MassRuleParser::ParseFromString(ruleJson, ruleSet, outError)) {
            outError = "Failed to parse massing envelope rule: " + outError;
            return false;
        }

        Moon::Massing::MassBuildResult buildResult;
        if (!Moon::Massing::MassMeshBuilder::Build(ruleSet, buildResult, outError)) {
            outError = "Failed to build massing envelope mesh: " + outError;
            return false;
        }

        for (size_t i = 0; i < buildResult.items.size(); ++i) {
            GeneratedMeshPart part;
            part.partId = mass.massId + "_envelope_" + std::to_string(i);
            part.material = "envelope_shell";
            part.mesh = buildResult.items[i].mesh;
            outBuilding.envelopeMeshes.push_back(std::move(part));
        }
    }

    return true;
}

} // namespace Building
} // namespace Moon
