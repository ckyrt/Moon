#include "LayoutResolver.h"
#include "../core/Logging/Logger.h"
#include "../../external/nlohmann/json.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <unordered_set>

using json = nlohmann::json;

namespace Moon {
namespace Building {

// ============================================================================
// SemanticBuildingParser Implementation
// ============================================================================

bool SemanticBuildingParser::ParseFromFile(
    const std::string& filePath,
    SemanticBuilding& building,
    std::string& error)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        error = "Failed to open file: " + filePath;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return ParseFromString(buffer.str(), building, error);
}

bool SemanticBuildingParser::ParseFromString(
    const std::string& jsonString,
    SemanticBuilding& building,
    std::string& error)
{
    try {
        json j = json::parse(jsonString);
        
        // Parse root fields
        building.schema = j.value("schema", "");
        building.grid = j.value("grid", 0.5f);
        building.buildingType = j.value("building_type", "");
        
        // Parse style
        if (j.contains("style")) {
            auto& style = j["style"];
            building.style.category = style.value("category", "");
            building.style.facade = style.value("facade", "");
            building.style.roof = style.value("roof", "");
            building.style.windowStyle = style.value("window_style", "");
            building.style.material = style.value("material", "");
        }
        
        // Parse mass
        if (j.contains("mass")) {
            auto& mass = j["mass"];
            building.mass.footprintArea = mass.value("footprint_area", 0.0f);
            building.mass.floors = mass.value("floors", 1);
            building.mass.totalHeight = mass.value("total_height", 0.0f);
        }
        
        // Parse program (floors)
        if (j.contains("program") && j["program"].contains("floors")) {
            auto& floorsArray = j["program"]["floors"];
            
            for (auto& floorJson : floorsArray) {
                SemanticFloor floor;
                floor.level = floorJson.value("level", 0);
                floor.name = floorJson.value("name", "");
                
                // Parse spaces
                if (floorJson.contains("spaces")) {
                    for (auto& spaceJson : floorJson["spaces"]) {
                        SemanticSpace space;
                        space.spaceId = spaceJson.value("space_id", "");
                        space.unitId = spaceJson.value("unit_id", "");
                        space.type = spaceJson.value("type", "");
                        space.zone = spaceJson.value("zone", "");
                        space.areaMin = spaceJson.value("area_min", 0.0f);
                        space.areaMax = spaceJson.value("area_max", 0.0f);
                        space.areaPreferred = spaceJson.value("area_preferred", 0.0f);
                        space.priority = spaceJson.value("priority", "medium");
                        
                        // Parse adjacency
                        if (spaceJson.contains("adjacency")) {
                            for (auto& adjJson : spaceJson["adjacency"]) {
                                Adjacency adj;
                                adj.to = adjJson.value("to", "");
                                adj.relationship = adjJson.value("relationship", "connected");
                                adj.importance = adjJson.value("importance", "optional");
                                space.adjacency.push_back(adj);
                            }
                        }
                        
                        // Parse constraints
                        if (spaceJson.contains("constraints")) {
                            auto& constr = spaceJson["constraints"];
                            space.constraints.aspectRatioMax = constr.value("aspect_ratio_max", 3.0f);
                            space.constraints.naturalLight = constr.value("natural_light", "none");
                            space.constraints.exteriorAccess = constr.value("exterior_access", false);
                            space.constraints.ceilingHeight = constr.value("ceiling_height", 3.0f);
                            space.constraints.minWidth = constr.value("min_width", 2.0f);
                            space.constraints.connectsToFloor = constr.value("connects_to_floor", -1);
                            space.constraints.connectsFromFloor = constr.value("connects_from_floor", -1);
                        }
                        
                        floor.spaces.push_back(space);
                    }
                }
                
                building.floors.push_back(floor);
            }
        }
        
        return true;
    }
    catch (const json::exception& e) {
        error = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

// ============================================================================
// LayoutResolver Implementation
// ============================================================================

LayoutResolver::LayoutResolver()
{
}

LayoutResolver::~LayoutResolver()
{
}

LayoutResolver::LayoutStrategy LayoutResolver::GetLayoutStrategy(const SemanticBuilding& input) const
{
    if (input.buildingType == "mall" || input.buildingType == "shopping_center" ||
        input.buildingType == "retail_center") {
        return LayoutStrategy::Mall;
    }
    if (input.buildingType == "office" || input.buildingType == "office_tower") {
        return LayoutStrategy::Tower;
    }
    if (input.buildingType == "apartment" || input.buildingType == "cbd_residential") {
        return LayoutStrategy::Apartment;
    }
    return LayoutStrategy::Villa;
}

float LayoutResolver::ComputeMallRingBand(float corridorArea, const AllocatedSpace& voidSpace) const
{
    const float perimeterTerm = voidSpace.size[0] + voidSpace.size[1];
    const float area = std::max(corridorArea, 1.0f);
    const float discriminant = std::sqrt(perimeterTerm * perimeterTerm + 4.0f * area);
    const float band = (-perimeterTerm + discriminant) / 4.0f;
    return std::max(1.5f, SnapToGrid({band, band})[0]);
}

bool LayoutResolver::Resolve(
    const SemanticBuilding& input,
    BuildingDefinition& output,
    std::string& error)
{
    return ResolveInternal(input, output, nullptr, false, error);
}

bool LayoutResolver::ResolveBestEffort(
    const SemanticBuilding& input,
    BuildingDefinition& output,
    BestEffortGenerationReport& report,
    std::string& error)
{
    report.skippedSpaces.clear();
    return ResolveInternal(input, output, &report, true, error);
}

bool LayoutResolver::ResolveInternal(
    const SemanticBuilding& input,
    BuildingDefinition& output,
    BestEffortGenerationReport* report,
    bool bestEffort,
    std::string& error)
{
    MOON_LOG_INFO("LayoutResolver", "=== Starting Layout Resolution ===");
    MOON_LOG_INFO("LayoutResolver", "Building type: %s", input.buildingType.c_str());

    m_bestEffortMode = bestEffort;
    m_bestEffortReport = report;
    m_allocatedSpaces.clear();
    m_reservedRects.clear();
    
    // Step 1: Calculate footprint
    if (!CalculateFootprint(input)) {
        error = "Failed to calculate footprint";
        m_bestEffortMode = false;
        m_bestEffortReport = nullptr;
        return false;
    }
    
    // Step 2: Allocate spaces
    if (!AllocateSpaces(input)) {
        error = "Failed to allocate spaces";
        m_bestEffortMode = false;
        m_bestEffortReport = nullptr;
        return false;
    }

    if (m_allocatedSpaces.empty()) {
        error = "Failed to allocate any spaces";
        m_bestEffortMode = false;
        m_bestEffortReport = nullptr;
        return false;
    }
    
    // Step 3: Generate rectangles
    if (!GenerateRectangles(input)) {
        error = "Failed to generate rectangles";
        m_bestEffortMode = false;
        m_bestEffortReport = nullptr;
        return false;
    }
    
    // Step 4: Build output
    BuildOutput(output, input);

    m_bestEffortMode = false;
    m_bestEffortReport = nullptr;
    
    MOON_LOG_INFO("LayoutResolver", "=== Layout Resolution Complete ===");
    return true;
}

void LayoutResolver::RecordBestEffortSkip(int floorLevel,
                                          const SemanticSpace& space,
                                          const std::string& reason)
{
    if (!m_bestEffortReport) {
        return;
    }

    BestEffortSkippedSpace issue;
    issue.floorLevel = floorLevel;
    issue.spaceId = space.spaceId;
    issue.spaceType = space.type;
    issue.reason = reason;
    m_bestEffortReport->usedBestEffort = true;
    m_bestEffortReport->skippedSpaces.push_back(issue);
}

bool LayoutResolver::CalculateFootprint(const SemanticBuilding& input)
{
    float totalArea = input.mass.footprintArea;
    int floors = input.mass.floors;
    
    if (totalArea <= 0.0f || floors <= 0) {
        MOON_LOG_ERROR("LayoutResolver", "Invalid mass constraints");
        return false;
    }
    
    // footprint_area is interpreted as the per-floor building footprint.
    float floorArea = totalArea;
    
    float aspectRatio = 1.5f;
    if (input.buildingType == "villa") {
        aspectRatio = 1.2f;
    } else if (input.buildingType == "apartment" || input.buildingType == "cbd_residential") {
        aspectRatio = 1.8f;
    } else if (input.buildingType == "office" || input.buildingType == "office_tower") {
        aspectRatio = 2.2f;
    } else if (input.buildingType == "mall" || input.buildingType == "shopping_center" ||
               input.buildingType == "retail_center") {
        aspectRatio = 3.0f;
    }
    
    m_footprint = CalculateOptimalDimensions(floorArea, aspectRatio);
    
    // Snap footprint to grid (CRITICAL for BuildingPipeline compatibility)
    m_footprint = SnapToGrid(m_footprint);
    
    MOON_LOG_INFO("LayoutResolver", "Footprint calculated: %.1f x %.1f m (%.1f m²)",
                  m_footprint[0], m_footprint[1], m_footprint[0] * m_footprint[1]);
    
    return true;
}

bool LayoutResolver::AllocateSpaces(const SemanticBuilding& input)
{
    MOON_LOG_INFO("LayoutResolver", "Allocating spaces for %zu floors", input.floors.size());
    
    m_allocatedSpaces.clear();
    
    for (const auto& floor : input.floors) {
        MOON_LOG_INFO("LayoutResolver", "Floor %d: %s - %zu spaces",
                      floor.level, floor.name.c_str(), floor.spaces.size());
        
        float totalPreferredArea = CalculateTotalArea(floor.spaces);
        float availableArea = m_footprint[0] * m_footprint[1];
        float scaleFactor = availableArea / totalPreferredArea;
        const auto orderedSpaces = BuildPlacementOrder(input, floor);
        const LayoutStrategy strategy = GetLayoutStrategy(input);
        
        MOON_LOG_INFO("LayoutResolver", "  Total preferred area: %.1f m²", totalPreferredArea);
        MOON_LOG_INFO("LayoutResolver", "  Available area: %.1f m²", availableArea);

        auto reserveAllocatedGeometry = [&](const SemanticSpace& semanticSpace,
                                           const AllocatedSpace& allocated,
                                           const std::vector<AllocatedSpace>& placedSpaces) {
            if (strategy == LayoutStrategy::Mall && semanticSpace.type == "corridor") {
                auto aroundVoid = std::find_if(semanticSpace.adjacency.begin(), semanticSpace.adjacency.end(),
                    [](const Adjacency& adjacency) {
                        return adjacency.relationship == "around";
                    });
                if (aroundVoid != semanticSpace.adjacency.end()) {
                    auto voidIt = std::find_if(placedSpaces.begin(), placedSpaces.end(),
                        [&](const AllocatedSpace& candidate) {
                            return candidate.spaceId == aroundVoid->to &&
                                   candidate.floorLevel == floor.level &&
                                   candidate.type == "void";
                        });
                    if (voidIt != placedSpaces.end()) {
                        const float band = ComputeMallRingBand(allocated.area, *voidIt);
                        const float left = std::max(0.0f, voidIt->position[0] - band);
                        const float right = std::min(m_footprint[0], voidIt->position[0] + voidIt->size[0] + band);
                        const float top = std::max(0.0f, voidIt->position[1] - band);
                        const float bottom = std::min(m_footprint[1], voidIt->position[1] + voidIt->size[1] + band);

                        m_reservedRects.push_back(CreateRect(semanticSpace.spaceId + "_top",
                            SnapToGrid({left, top}),
                            SnapToGrid({right - left, band})));
                        m_reservedRects.push_back(CreateRect(semanticSpace.spaceId + "_bottom",
                            SnapToGrid({left, std::max(0.0f, bottom - band)}),
                            SnapToGrid({right - left, band})));
                        m_reservedRects.push_back(CreateRect(semanticSpace.spaceId + "_left",
                            SnapToGrid({left, voidIt->position[1]}),
                            SnapToGrid({band, voidIt->size[1]})));
                        m_reservedRects.push_back(CreateRect(semanticSpace.spaceId + "_right",
                            SnapToGrid({std::max(0.0f, right - band), voidIt->position[1]}),
                            SnapToGrid({band, voidIt->size[1]})));
                        return;
                    }
                }
            }

            m_reservedRects.push_back(CreateRect(semanticSpace.spaceId,
                SnapToGrid(allocated.position),
                SnapToGrid(allocated.size)));
        };

        auto rectanglesShareBoundary = [&](const GridPos2D& aPos,
                                           const GridSize2D& aSize,
                                           const GridPos2D& bPos,
                                           const GridSize2D& bSize) {
            const float epsilon = 0.001f;

            const float aMinX = aPos[0];
            const float aMaxX = aPos[0] + aSize[0];
            const float aMinY = aPos[1];
            const float aMaxY = aPos[1] + aSize[1];
            const float bMinX = bPos[0];
            const float bMaxX = bPos[0] + bSize[0];
            const float bMinY = bPos[1];
            const float bMaxY = bPos[1] + bSize[1];

            const bool verticalTouch = std::abs(aMaxX - bMinX) <= epsilon || std::abs(bMaxX - aMinX) <= epsilon;
            const bool horizontalOverlap = std::min(aMaxY, bMaxY) - std::max(aMinY, bMinY) > epsilon;
            if (verticalTouch && horizontalOverlap) {
                return true;
            }

            const bool horizontalTouch = std::abs(aMaxY - bMinY) <= epsilon || std::abs(bMaxY - aMinY) <= epsilon;
            const bool verticalOverlap = std::min(aMaxX, bMaxX) - std::max(aMinX, bMinX) > epsilon;
            return horizontalTouch && verticalOverlap;
        };

        auto getReservedRectsForSpace = [&](const std::string& targetSpaceId) {
            std::vector<Rect> rects;
            for (const auto& reservedRect : m_reservedRects) {
                if (reservedRect.rectId == targetSpaceId || reservedRect.rectId.rfind(targetSpaceId + "_", 0) == 0) {
                    rects.push_back(reservedRect);
                }
            }
            return rects;
        };

        auto hasPlacedRequiredAdjacencyConstraint = [&](const SemanticSpace& semanticSpace,
                                                        const std::vector<AllocatedSpace>& placedSpaces) {
            auto isPlaced = [&](const std::string& targetSpaceId) {
                return std::any_of(placedSpaces.begin(), placedSpaces.end(),
                    [&](const AllocatedSpace& placed) {
                        return placed.spaceId == targetSpaceId;
                    });
            };

            for (const auto& adjacency : semanticSpace.adjacency) {
                if (adjacency.importance == "required" &&
                    (adjacency.relationship == "connected" || adjacency.relationship == "share_wall") &&
                    isPlaced(adjacency.to)) {
                    return true;
                }
            }

            for (const auto& otherSpace : floor.spaces) {
                if (otherSpace.spaceId == semanticSpace.spaceId) {
                    continue;
                }
                for (const auto& adjacency : otherSpace.adjacency) {
                    if (adjacency.to == semanticSpace.spaceId &&
                        adjacency.importance == "required" &&
                        (adjacency.relationship == "connected" || adjacency.relationship == "share_wall") &&
                        isPlaced(otherSpace.spaceId)) {
                        return true;
                    }
                }
            }

            return false;
        };

        auto satisfiesRequiredAdjacency = [&](const SemanticSpace& semanticSpace,
                                              const GridPos2D& candidatePosition,
                                              const GridSize2D& candidateSize,
                                              const std::vector<AllocatedSpace>& placedSpaces) {
            auto touchesSpace = [&](const std::string& targetSpaceId) {
                const auto targetRects = getReservedRectsForSpace(targetSpaceId);
                for (const auto& targetRect : targetRects) {
                    if (rectanglesShareBoundary(candidatePosition, candidateSize,
                                                targetRect.origin, targetRect.size)) {
                        return true;
                    }
                }
                return false;
            };

            for (const auto& adjacency : semanticSpace.adjacency) {
                if (adjacency.importance != "required" ||
                    (adjacency.relationship != "connected" && adjacency.relationship != "share_wall")) {
                    continue;
                }

                const bool placed = std::any_of(placedSpaces.begin(), placedSpaces.end(),
                    [&](const AllocatedSpace& allocated) {
                        return allocated.spaceId == adjacency.to;
                    });
                if (placed && !touchesSpace(adjacency.to)) {
                    return false;
                }
            }

            for (const auto& otherSpace : floor.spaces) {
                if (otherSpace.spaceId == semanticSpace.spaceId) {
                    continue;
                }
                const bool otherPlaced = std::any_of(placedSpaces.begin(), placedSpaces.end(),
                    [&](const AllocatedSpace& allocated) {
                        return allocated.spaceId == otherSpace.spaceId;
                    });
                if (!otherPlaced) {
                    continue;
                }

                for (const auto& adjacency : otherSpace.adjacency) {
                    if (adjacency.to == semanticSpace.spaceId &&
                        adjacency.importance == "required" &&
                        (adjacency.relationship == "connected" || adjacency.relationship == "share_wall") &&
                        !touchesSpace(otherSpace.spaceId)) {
                        return false;
                    }
                }
            }

            return true;
        };
        
        auto tryAllocateFloor = [&](float candidateScale,
                                    std::vector<AllocatedSpace>& outSpaces,
                                    float& outUsedHeight) -> bool {
            outSpaces.clear();
            m_reservedRects.clear();

            outUsedHeight = 0.0f;

            for (const auto* spacePtr : orderedSpaces) {
                const auto& space = *spacePtr;
                AllocatedSpace allocated;
                allocated.spaceId = space.spaceId;
                allocated.unitId = space.unitId;
                allocated.type = space.type;
                allocated.floorLevel = floor.level;

                float targetArea = space.areaPreferred > 0.0f ?
                    space.areaPreferred :
                    GetDefaultAreaForSpaceType(space.type);
                allocated.area = targetArea * candidateScale;

                if (space.areaMin > 0.0f && allocated.area < space.areaMin) {
                    allocated.area = space.areaMin;
                }
                if (space.areaMax > 0.0f && allocated.area > space.areaMax) {
                    allocated.area = space.areaMax;
                }

                float aspectRatio = GetTargetAspectRatio(space);
                GridSize2D dims = CalculateOptimalDimensions(allocated.area, aspectRatio);

                if (dims[0] < space.constraints.minWidth) {
                    dims[0] = space.constraints.minWidth;
                    dims[1] = allocated.area / dims[0];
                }
                if (dims[1] < space.constraints.minWidth) {
                    dims[1] = space.constraints.minWidth;
                    dims[0] = allocated.area / dims[1];
                }

                dims = SnapToGrid(dims);

                if (dims[0] > m_footprint[0] || dims[1] > m_footprint[1]) {
                    outUsedHeight = m_footprint[1] + dims[1];
                    return false;
                }

                const bool hasRequiredPlacedAdjacency = hasPlacedRequiredAdjacencyConstraint(space, outSpaces);

                GridPos2D position = {0.0f, 0.0f};
                bool placed = TryPlaceUsingStairAlignment(space, dims, outSpaces, position);

                if (!placed && hasRequiredPlacedAdjacency) {
                    placed = TryPlaceNearConnectedSpace(space, dims, outSpaces, position);
                }

                if (!placed) {
                    placed = TryPlaceInCirculationBand(input, space, dims, outSpaces, position);
                }

                if (!placed && !space.unitId.empty()) {
                    placed = TryPlaceInsideUnitCluster(space, dims, outSpaces, position);
                }

                if (!placed) {
                    placed = TryPlaceInMallRetailBand(input, space, dims, outSpaces, position);
                }

                if (!placed) {
                    placed = TryPlaceNearConnectedSpace(space, dims, outSpaces, position);
                }

                if (!placed) {
                    placed = TryPlaceNearCoreOrCirculation(input, space, dims, outSpaces, position);
                }

                if (!placed && RequiresExteriorWall(space)) {
                    placed = TryPlaceOnExteriorWall(space, dims, outSpaces, position);
                } else if (!placed) {
                    placed = TryPlaceOnExteriorWall(space, dims, outSpaces, position) ||
                             FindFirstFitPosition(dims, outSpaces, position);
                }

                if (placed && hasRequiredPlacedAdjacency &&
                    !satisfiesRequiredAdjacency(space, position, dims, outSpaces)) {
                    placed = false;
                }

                if (!placed) {
                    outUsedHeight = m_footprint[1] + dims[1];
                    return false;
                }

                allocated.position = position;
                allocated.size = dims;
                outSpaces.push_back(allocated);
                reserveAllocatedGeometry(space, allocated, outSpaces);

                outUsedHeight = std::max(outUsedHeight, allocated.position[1] + allocated.size[1]);
            }
            return outUsedHeight <= m_footprint[1] + 0.001f;
        };

        auto tryAllocateFloorBestEffort = [&](float candidateScale,
                                              std::vector<AllocatedSpace>& outSpaces,
                                              float& outUsedHeight) {
            outSpaces.clear();
            m_reservedRects.clear();

            outUsedHeight = 0.0f;

            for (const auto* spacePtr : orderedSpaces) {
                const auto& space = *spacePtr;
                AllocatedSpace allocated;
                allocated.spaceId = space.spaceId;
                allocated.unitId = space.unitId;
                allocated.type = space.type;
                allocated.floorLevel = floor.level;

                float targetArea = space.areaPreferred > 0.0f ?
                    space.areaPreferred :
                    GetDefaultAreaForSpaceType(space.type);
                allocated.area = targetArea * candidateScale;

                if (space.areaMin > 0.0f && allocated.area < space.areaMin) {
                    allocated.area = space.areaMin;
                }
                if (space.areaMax > 0.0f && allocated.area > space.areaMax) {
                    allocated.area = space.areaMax;
                }

                float aspectRatio = GetTargetAspectRatio(space);
                GridSize2D dims = CalculateOptimalDimensions(allocated.area, aspectRatio);

                if (dims[0] < space.constraints.minWidth) {
                    dims[0] = space.constraints.minWidth;
                    dims[1] = allocated.area / dims[0];
                }
                if (dims[1] < space.constraints.minWidth) {
                    dims[1] = space.constraints.minWidth;
                    dims[0] = allocated.area / dims[1];
                }

                dims = SnapToGrid(dims);

                if (dims[0] > m_footprint[0] || dims[1] > m_footprint[1]) {
                    RecordBestEffortSkip(floor.level, space, "space exceeds available footprint");
                    MOON_LOG_ERROR("LayoutResolver",
                                   "  Skipping floor %d space '%s' (%s): exceeds footprint %.1f x %.1f m",
                                   floor.level, space.spaceId.c_str(), space.type.c_str(),
                                   m_footprint[0], m_footprint[1]);
                    continue;
                }

                const bool hasRequiredPlacedAdjacency = hasPlacedRequiredAdjacencyConstraint(space, outSpaces);

                GridPos2D position = {0.0f, 0.0f};
                bool placed = TryPlaceUsingStairAlignment(space, dims, outSpaces, position);

                if (!placed && hasRequiredPlacedAdjacency) {
                    placed = TryPlaceNearConnectedSpace(space, dims, outSpaces, position);
                }

                if (!placed) {
                    placed = TryPlaceInCirculationBand(input, space, dims, outSpaces, position);
                }

                if (!placed && !space.unitId.empty()) {
                    placed = TryPlaceInsideUnitCluster(space, dims, outSpaces, position);
                }

                if (!placed) {
                    placed = TryPlaceInMallRetailBand(input, space, dims, outSpaces, position);
                }

                if (!placed) {
                    placed = TryPlaceNearConnectedSpace(space, dims, outSpaces, position);
                }

                if (!placed) {
                    placed = TryPlaceNearCoreOrCirculation(input, space, dims, outSpaces, position);
                }

                if (!placed && RequiresExteriorWall(space)) {
                    placed = TryPlaceOnExteriorWall(space, dims, outSpaces, position);
                } else if (!placed) {
                    placed = TryPlaceOnExteriorWall(space, dims, outSpaces, position) ||
                             FindFirstFitPosition(dims, outSpaces, position);
                }

                if (placed && hasRequiredPlacedAdjacency &&
                    !satisfiesRequiredAdjacency(space, position, dims, outSpaces)) {
                    placed = false;
                }

                if (!placed) {
                    RecordBestEffortSkip(floor.level, space, "space could not be placed without breaking layout constraints");
                    MOON_LOG_ERROR("LayoutResolver",
                                   "  Skipping floor %d space '%s' (%s): could not place within current layout",
                                   floor.level, space.spaceId.c_str(), space.type.c_str());
                    continue;
                }

                allocated.position = position;
                allocated.size = dims;
                outSpaces.push_back(allocated);
                reserveAllocatedGeometry(space, allocated, outSpaces);

                outUsedHeight = std::max(outUsedHeight, allocated.position[1] + allocated.size[1]);
            }
        };

        std::vector<AllocatedSpace> floorAllocated;
        float usedHeight = 0.0f;
        bool allocatedFloor = false;
        const float initialScaleFactor = scaleFactor;
        float resolvedScaleFactor = scaleFactor;

        for (int attempt = 0; attempt < 8; ++attempt) {
            if (tryAllocateFloor(scaleFactor, floorAllocated, usedHeight)) {
                allocatedFloor = true;
                break;
            }

            if (usedHeight <= 0.0f) {
                break;
            }

            const float fitRatio = m_footprint[1] / usedHeight;
            const float nextScaleFactor = scaleFactor * fitRatio * fitRatio * 0.98f;
            if (nextScaleFactor >= scaleFactor - 0.001f) {
                break;
            }
            scaleFactor = nextScaleFactor;
        }

        if (!allocatedFloor) {
            if (m_bestEffortMode) {
                resolvedScaleFactor = initialScaleFactor;
                tryAllocateFloorBestEffort(resolvedScaleFactor, floorAllocated, usedHeight);
                if (floorAllocated.empty()) {
                    MOON_LOG_ERROR("LayoutResolver",
                                   "  Floor %d could not place any spaces in best-effort mode",
                                   floor.level);
                } else {
                    MOON_LOG_WARN("LayoutResolver",
                                  "  Floor %d entered best-effort mode, skipped %zu spaces",
                                  floor.level,
                                  floor.spaces.size() - floorAllocated.size());
                }
                allocatedFloor = true;
            }
        }

        if (!allocatedFloor) {
            MOON_LOG_ERROR("LayoutResolver",
                           "  Failed to fit floor %d within footprint %.1f x %.1f m",
                           floor.level, m_footprint[0], m_footprint[1]);
            return false;
        }

        if (resolvedScaleFactor < initialScaleFactor - 0.001f) {
            MOON_LOG_INFO("LayoutResolver",
                          "  Adjusted scale factor from %.2f to %.2f to fit floor height %.1f m",
                          initialScaleFactor, resolvedScaleFactor, m_footprint[1]);
        } else {
            MOON_LOG_INFO("LayoutResolver", "  Scale factor: %.2f", resolvedScaleFactor);
        }

        for (const auto& allocated : floorAllocated) {
            m_allocatedSpaces.push_back(allocated);

            if (m_verbose) {
                MOON_LOG_INFO("LayoutResolver", "  Space '%s' (%s): %.1f m² at (%.1f, %.1f) size (%.1f x %.1f)",
                              allocated.spaceId.c_str(), allocated.type.c_str(),
                              allocated.area, allocated.position[0], allocated.position[1],
                              allocated.size[0], allocated.size[1]);
            }
        }
    }
    
    return true;
}

bool LayoutResolver::GenerateRectangles(const SemanticBuilding& input)
{
    MOON_LOG_INFO("LayoutResolver", "Generating rectangles for %zu allocated spaces",
                  m_allocatedSpaces.size());
    
    // Rectangles are already generated in AllocateSpaces
    // This step could be used for optimization/refinement
    
    return true;
}

void LayoutResolver::BuildOutput(BuildingDefinition& output, const SemanticBuilding& input)
{
    output.schema = "moon_building_resolved";
    output.grid = m_gridSize;
    output.masses.clear();
    output.floors.clear();
    
    // Copy style
    output.style.category = input.style.category;
    output.style.facade = input.style.facade;
    output.style.roof = input.style.roof;
    output.style.windowStyle = input.style.windowStyle;
    output.style.material = input.style.material;
    
    // Create mass
    Mass mass;
    mass.massId = "main_building";
    mass.origin = {0.0f, 0.0f};
    mass.size = m_footprint;
    mass.floors = input.mass.floors;
    output.masses.push_back(mass);
    
    // Create floors and spaces
    int spaceIdCounter = 1;
    for (const auto& semanticFloor : input.floors) {
        Floor floor;
        floor.level = semanticFloor.level;
        floor.massId = "main_building";
        floor.floorHeight = GetDefaultFloorHeight(input, semanticFloor.level);
        
        for (const auto& semanticSpace : semanticFloor.spaces) {
            // Find allocated space
            auto it = std::find_if(m_allocatedSpaces.begin(), m_allocatedSpaces.end(),
                                   [&](const AllocatedSpace& a) {
                                       return a.spaceId == semanticSpace.spaceId &&
                                              a.floorLevel == semanticFloor.level;
                                   });
            
            if (it == m_allocatedSpaces.end()) {
                continue;
            }

            if (semanticSpace.type == "void") {
                continue;
            }
            
            const AllocatedSpace& allocated = *it;
            
            Space space;
            space.spaceId = spaceIdCounter++;
            
            // Create rect
            GridPos2D origin = SnapToGrid(allocated.position);
            GridSize2D size = SnapToGrid(allocated.size);

            bool emittedSpecialRects = false;
            if (GetLayoutStrategy(input) == LayoutStrategy::Mall &&
                semanticSpace.type == "corridor") {
                auto aroundVoid = std::find_if(semanticSpace.adjacency.begin(), semanticSpace.adjacency.end(),
                    [](const Adjacency& adjacency) {
                        return adjacency.relationship == "around";
                    });
                if (aroundVoid != semanticSpace.adjacency.end()) {
                    auto voidIt = std::find_if(m_allocatedSpaces.begin(), m_allocatedSpaces.end(),
                        [&](const AllocatedSpace& candidate) {
                            return candidate.spaceId == aroundVoid->to &&
                                   candidate.floorLevel == semanticFloor.level &&
                                   candidate.type == "void";
                        });
                    if (voidIt != m_allocatedSpaces.end()) {
                        const float band = ComputeMallRingBand(allocated.area, *voidIt);
                        const float left = std::max(0.0f, voidIt->position[0] - band);
                        const float right = std::min(m_footprint[0], voidIt->position[0] + voidIt->size[0] + band);
                        const float top = std::max(0.0f, voidIt->position[1] - band);
                        const float bottom = std::min(m_footprint[1], voidIt->position[1] + voidIt->size[1] + band);

                        space.rects.push_back(CreateRect(semanticSpace.spaceId + "_top",
                            SnapToGrid({left, top}),
                            SnapToGrid({right - left, band})));
                        space.rects.push_back(CreateRect(semanticSpace.spaceId + "_bottom",
                            SnapToGrid({left, std::max(0.0f, bottom - band)}),
                            SnapToGrid({right - left, band})));
                        space.rects.push_back(CreateRect(semanticSpace.spaceId + "_left",
                            SnapToGrid({left, voidIt->position[1]}),
                            SnapToGrid({band, voidIt->size[1]})));
                        space.rects.push_back(CreateRect(semanticSpace.spaceId + "_right",
                            SnapToGrid({std::max(0.0f, right - band), voidIt->position[1]}),
                            SnapToGrid({band, voidIt->size[1]})));
                        emittedSpecialRects = true;
                    }
                }
            }

            if (!emittedSpecialRects) {
                Rect rect = CreateRect(semanticSpace.spaceId, origin, size);
                space.rects.push_back(rect);
            }
            
            // Set properties
            space.properties.usage = StringToSpaceUsage(semanticSpace.type);
            space.properties.isOutdoor = (semanticSpace.type == "balcony" || 
                                         semanticSpace.type == "terrace");
            space.properties.hasStairs = (semanticSpace.type == "stairs" || semanticSpace.type == "core");
            space.properties.ceilingHeight = semanticSpace.constraints.ceilingHeight > 0.0f
                ? semanticSpace.constraints.ceilingHeight
                : floor.floorHeight;

            if (semanticSpace.type == "stairs" || semanticSpace.type == "core") {
                space.stairsConfig.type = StairType::Straight;
                if (semanticSpace.constraints.connectsToFloor >= 0) {
                    space.stairsConfig.connectToLevel = semanticSpace.constraints.connectsToFloor;
                } else if (semanticFloor.level + 1 < input.mass.floors) {
                    space.stairsConfig.connectToLevel = semanticFloor.level + 1;
                } else {
                    space.stairsConfig.connectToLevel = semanticFloor.level;
                }
                space.stairsConfig.position = {
                    origin[0] + size[0] * 0.5f,
                    origin[1] + size[1] * 0.5f
                };
                space.stairsConfig.width = std::max(1.2f, std::min(size[0], size[1]));
            }
            
            floor.spaces.push_back(space);
        }
        
        output.floors.push_back(floor);
    }
    
    MOON_LOG_INFO("LayoutResolver", "Built output: %zu masses, %zu floors",
                  output.masses.size(), output.floors.size());
}

// ============================================================================
// Helper Functions
// ============================================================================

int LayoutResolver::GetPriorityWeight(const std::string& priority) const
{
    if (priority == "high") {
        return 3;
    }
    if (priority == "low") {
        return 1;
    }
    return 2;
}

bool LayoutResolver::IsCirculationSpace(const SemanticSpace& space) const
{
    return space.zone == "circulation" ||
           space.type == "corridor" ||
           space.type == "stairs" ||
           space.type == "entrance" ||
           space.type == "lobby" ||
           space.type == "elevator";
}

bool LayoutResolver::IsCoreSpace(const SemanticSpace& space) const
{
    return space.type == "core" || space.type == "elevator";
}

bool LayoutResolver::IsVoidSpace(const SemanticSpace& space) const
{
    return space.type == "void";
}

bool LayoutResolver::IsServiceSpace(const SemanticSpace& space) const
{
    return space.zone == "service" ||
           space.type == "mechanical" ||
           space.type == "storage" ||
           space.type == "bathroom";
}

bool LayoutResolver::RequiresExteriorWall(const SemanticSpace& space) const
{
    return space.constraints.exteriorAccess ||
           space.constraints.naturalLight == "required" ||
           space.type == "living" ||
           space.type == "bedroom" ||
           space.type == "office" ||
           space.type == "shop";
}

int LayoutResolver::GetZoneWeight(const SemanticSpace& space) const
{
    if (space.zone == "circulation") return 400;
    if (space.zone == "public") return 300;
    if (space.zone == "private") return 200;
    if (space.zone == "service") return 100;
    return 0;
}

float LayoutResolver::GetTargetAspectRatio(const SemanticSpace& space) const
{
    float target = 1.5f;

    if (space.type == "corridor") {
        target = 4.0f;
    } else if (space.type == "lobby") {
        target = 1.8f;
    } else if (space.type == "meeting_room") {
        target = 1.4f;
    } else if (space.type == "shop") {
        target = 1.4f;
    } else if (space.type == "void") {
        target = 1.0f;
    } else if (space.type == "mechanical") {
        target = 1.2f;
    } else if (space.type == "bathroom") {
        target = 1.2f;
    } else if (space.type == "stairs") {
        target = 1.4f;
    } else if (space.type == "entrance") {
        target = 1.3f;
    }

    if (space.constraints.aspectRatioMax > 0.0f) {
        target = std::min(target, std::max(1.0f, space.constraints.aspectRatioMax));
    }

    return target;
}

float LayoutResolver::GetDefaultFloorHeight(const SemanticBuilding& input, int floorLevel) const
{
    if (input.mass.totalHeight > 0.0f && input.mass.floors > 0) {
        return input.mass.totalHeight / static_cast<float>(input.mass.floors);
    }

    if (input.buildingType == "mall" || input.buildingType == "shopping_center" ||
        input.buildingType == "retail_center") {
        return floorLevel == 0 ? 5.0f : 4.5f;
    }
    if (input.buildingType == "office" || input.buildingType == "office_tower") {
        return floorLevel == 0 ? 4.5f : 3.8f;
    }
    if (input.buildingType == "apartment" || input.buildingType == "cbd_residential") {
        return floorLevel == 0 ? 3.5f : 3.0f;
    }

    return 3.0f;
}

std::vector<const SemanticSpace*> LayoutResolver::BuildPlacementOrder(const SemanticBuilding& input,
                                                                      const SemanticFloor& floor) const
{
    const LayoutStrategy strategy = GetLayoutStrategy(input);
    std::unordered_map<std::string, int> inboundRequiredConnections;
    for (const auto& sourceSpace : floor.spaces) {
        for (const auto& adjacency : sourceSpace.adjacency) {
            if (adjacency.importance == "required" &&
                (adjacency.relationship == "connected" || adjacency.relationship == "share_wall")) {
                ++inboundRequiredConnections[adjacency.to];
            }
        }
    }

    std::vector<const SemanticSpace*> remaining;
    remaining.reserve(floor.spaces.size());
    for (const auto& space : floor.spaces) {
        remaining.push_back(&space);
    }

    auto baseScore = [&](const SemanticSpace& space) -> float {
        float score = static_cast<float>(GetPriorityWeight(space.priority) * 1000);
        score += space.areaPreferred > 0.0f ? space.areaPreferred : GetDefaultAreaForSpaceType(space.type);
        score += static_cast<float>(GetZoneWeight(space));

        if (IsCoreSpace(space)) {
            score += 1000.0f;
        }
        if (IsVoidSpace(space)) {
            score += 900.0f;
        }

        if (space.type == "corridor" || space.type == "lobby") {
            score += 550.0f;
        }
        if (space.type == "stairs") {
            score += 400.0f;
        }
        if (space.type == "entrance") {
            score += 250.0f;
        }
        if (space.constraints.connectsFromFloor >= 0) {
            score += 600.0f;
        }
        if (space.constraints.connectsToFloor >= 0) {
            score += 250.0f;
        }
        if (RequiresExteriorWall(space)) {
            score += 120.0f;
        }
        if (!space.unitId.empty()) {
            score += 90.0f;
        }
        auto inboundIt = inboundRequiredConnections.find(space.spaceId);
        if (inboundIt != inboundRequiredConnections.end()) {
            score += static_cast<float>(inboundIt->second) * 1500.0f;
        }

        if (strategy == LayoutStrategy::Tower) {
            if (floor.level == 0 && (space.type == "lobby" || space.type == "entrance")) {
                score += 700.0f;
            }
            if (space.type == "office" || space.type == "meeting_room") {
                score += 220.0f;
            }
            if (space.type == "mechanical") {
                score += 120.0f;
            }
        } else if (strategy == LayoutStrategy::Mall) {
            if (space.type == "void") {
                score += 11300.0f;
            }
            if (space.type == "corridor" || space.type == "lobby") {
                score += space.type == "corridor" ? 8650.0f : 7650.0f;
            }
            if (space.type == "shop") {
                score += 4260.0f;
            }
            if (IsCoreSpace(space)) {
                score += 2100.0f;
            }
        } else if (strategy == LayoutStrategy::Apartment) {
            if (space.type == "corridor" || space.type == "lobby") {
                score += 450.0f;
            }
            if (!space.unitId.empty()) {
                score += 180.0f;
            }
        } else if (strategy == LayoutStrategy::Villa) {
            if (floor.level == 0 && (space.type == "living" || space.type == "kitchen" ||
                                     space.type == "entrance")) {
                score += 180.0f;
            }
            if (floor.level > 0 && space.type == "bedroom") {
                score += 120.0f;
            }
            if (IsCoreSpace(space)) {
                score -= 600.0f;
            }
        }

        return score;
    };

    std::vector<const SemanticSpace*> ordered;
    ordered.reserve(remaining.size());
    std::unordered_set<std::string> placedIds;

    while (!remaining.empty()) {
        auto bestIt = remaining.begin();
        float bestScore = -1.0f;

        for (auto it = remaining.begin(); it != remaining.end(); ++it) {
            const SemanticSpace& candidate = **it;
            float score = baseScore(candidate);

            for (const auto& adjacency : candidate.adjacency) {
                if (placedIds.find(adjacency.to) == placedIds.end()) {
                    if (adjacency.importance == "required") {
                        if (adjacency.relationship == "connected" ||
                            adjacency.relationship == "share_wall") {
                            score -= 2200.0f;
                        } else if (adjacency.relationship == "around") {
                            score -= 1800.0f;
                        }
                    }
                    continue;
                }

                if (adjacency.relationship == "connected") {
                    score += adjacency.importance == "required" ? 500.0f : 180.0f;
                } else if (adjacency.relationship == "nearby") {
                    score += 80.0f;
                } else if (adjacency.relationship == "share_wall") {
                    score += 120.0f;
                } else if (adjacency.relationship == "around") {
                    score += 140.0f;
                }
            }

            if (!candidate.unitId.empty()) {
                for (const auto& placedId : placedIds) {
                    auto existing = std::find_if(floor.spaces.begin(), floor.spaces.end(),
                        [&](const SemanticSpace& space) {
                            return space.spaceId == placedId && space.unitId == candidate.unitId;
                        });
                    if (existing != floor.spaces.end()) {
                        score += 320.0f;
                    }
                }
            }

            if (score > bestScore) {
                bestScore = score;
                bestIt = it;
            }
        }

        ordered.push_back(*bestIt);
        placedIds.insert((*bestIt)->spaceId);
        remaining.erase(bestIt);
    }

    return ordered;
}

bool LayoutResolver::RectanglesOverlap(const GridPos2D& aPos, const GridSize2D& aSize,
                                       const GridPos2D& bPos, const GridSize2D& bSize) const
{
    const float epsilon = 0.001f;

    return !(aPos[0] + aSize[0] <= bPos[0] + epsilon ||
             bPos[0] + bSize[0] <= aPos[0] + epsilon ||
             aPos[1] + aSize[1] <= bPos[1] + epsilon ||
             bPos[1] + bSize[1] <= aPos[1] + epsilon);
}

bool LayoutResolver::FitsWithoutOverlap(const GridPos2D& position,
                                        const GridSize2D& size,
                                        const std::vector<AllocatedSpace>& /*placedSpaces*/) const
{
    const float epsilon = 0.001f;

    if (position[0] < -epsilon || position[1] < -epsilon) {
        return false;
    }
    if (position[0] + size[0] > m_footprint[0] + epsilon ||
        position[1] + size[1] > m_footprint[1] + epsilon) {
        return false;
    }

    for (const auto& reservedRect : m_reservedRects) {
        if (RectanglesOverlap(position, size, reservedRect.origin, reservedRect.size)) {
            return false;
        }
    }

    return true;
}

bool LayoutResolver::FindFirstFitPosition(const GridSize2D& size,
                                          const std::vector<AllocatedSpace>& placedSpaces,
                                          GridPos2D& outPosition) const
{
    for (float y = 0.0f; y <= m_footprint[1] - size[1] + 0.001f; y += m_gridSize) {
        for (float x = 0.0f; x <= m_footprint[0] - size[0] + 0.001f; x += m_gridSize) {
            GridPos2D candidate = SnapToGrid({x, y});
            if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
                outPosition = candidate;
                return true;
            }
        }
    }

    return false;
}

bool LayoutResolver::TryPlaceUsingStairAlignment(const SemanticSpace& space,
                                                 const GridSize2D& size,
                                                 const std::vector<AllocatedSpace>& placedSpaces,
                                                 GridPos2D& outPosition) const
{
    if ((space.type != "stairs" && space.type != "core") || space.constraints.connectsFromFloor < 0) {
        return false;
    }

    for (const auto& allocated : m_allocatedSpaces) {
        if (allocated.type != space.type || allocated.floorLevel != space.constraints.connectsFromFloor) {
            continue;
        }

        GridPos2D candidate = SnapToGrid(allocated.position);
        if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
            outPosition = candidate;
            return true;
        }
    }

    return false;
}

bool LayoutResolver::TryPlaceInCirculationBand(const SemanticBuilding& input,
                                               const SemanticSpace& space,
                                               const GridSize2D& size,
                                               const std::vector<AllocatedSpace>& placedSpaces,
                                               GridPos2D& outPosition) const
{
    const LayoutStrategy strategy = GetLayoutStrategy(input);
    const bool isRetailTypology = strategy == LayoutStrategy::Mall;

    if (IsCoreSpace(space)) {
        if (strategy == LayoutStrategy::Mall) {
            const std::vector<GridPos2D> retailCoreCandidates = {
                SnapToGrid({0.0f, 0.0f}),
                SnapToGrid({std::max(0.0f, m_footprint[0] - size[0]), 0.0f}),
                SnapToGrid({0.0f, std::max(0.0f, m_footprint[1] - size[1])}),
                SnapToGrid({std::max(0.0f, m_footprint[0] - size[0]), std::max(0.0f, m_footprint[1] - size[1])})
            };

            for (const auto& candidate : retailCoreCandidates) {
                if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
                    outPosition = candidate;
                    return true;
                }
            }
        } else if (strategy == LayoutStrategy::Apartment || strategy == LayoutStrategy::Tower) {
            const float centerX = std::max(0.0f, std::round(((m_footprint[0] - size[0]) * 0.5f) / m_gridSize) * m_gridSize);
            const float centerY = std::max(0.0f, std::round(((m_footprint[1] - size[1]) * 0.5f) / m_gridSize) * m_gridSize);
            GridPos2D candidate = SnapToGrid({centerX, centerY});
            if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
                outPosition = candidate;
                return true;
            }
        } else {
            const std::vector<GridPos2D> villaCoreCandidates = {
                SnapToGrid({0.0f, 0.0f}),
                SnapToGrid({std::max(0.0f, m_footprint[0] - size[0]), 0.0f}),
                SnapToGrid({0.0f, std::max(0.0f, m_footprint[1] - size[1])})
            };
            for (const auto& candidate : villaCoreCandidates) {
                if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
                    outPosition = candidate;
                    return true;
                }
            }
        }
        return false;
    }

    if (IsVoidSpace(space)) {
        if (strategy == LayoutStrategy::Mall) {
            const float centerX = std::max(0.0f, std::round(((m_footprint[0] - size[0]) * 0.5f) / m_gridSize) * m_gridSize);
            const float centerY = std::max(0.0f, std::round(((m_footprint[1] - size[1]) * 0.5f) / m_gridSize) * m_gridSize);
            GridPos2D candidate = SnapToGrid({centerX, centerY});
            if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
                outPosition = candidate;
                return true;
            }
        } else {
            const float centerX = std::max(0.0f, std::round(((m_footprint[0] - size[0]) * 0.5f) / m_gridSize) * m_gridSize);
            const float centerY = std::max(0.0f, std::round(((m_footprint[1] - size[1]) * 0.5f) / m_gridSize) * m_gridSize);
            GridPos2D candidate = SnapToGrid({centerX, centerY});
            if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
                outPosition = candidate;
                return true;
            }
        }
        return false;
    }

    if (!IsCirculationSpace(space)) {
        return false;
    }

    std::vector<GridPos2D> candidates;

    if (isRetailTypology && space.type == "corridor") {
        auto voidIt = std::find_if(placedSpaces.begin(), placedSpaces.end(),
                                   [](const AllocatedSpace& placed) {
                                       return placed.type == "void";
                                   });
        if (voidIt != placedSpaces.end()) {
            const float corridorArea = space.areaPreferred > 0.0f ? space.areaPreferred : GetDefaultAreaForSpaceType(space.type);
            const float band = ComputeMallRingBand(corridorArea, *voidIt);
            candidates.push_back(SnapToGrid({voidIt->position[0], std::max(0.0f, voidIt->position[1] - band)}));
            candidates.push_back(SnapToGrid({voidIt->position[0], std::min(m_footprint[1] - band, voidIt->position[1] + voidIt->size[1])}));
            candidates.push_back(SnapToGrid({std::max(0.0f, voidIt->position[0] - band), voidIt->position[1]}));
            candidates.push_back(SnapToGrid({std::min(m_footprint[0] - band, voidIt->position[0] + voidIt->size[0]), voidIt->position[1]}));
        }
    }

    if (space.type == "corridor" || space.type == "lobby") {
        if (strategy == LayoutStrategy::Mall && space.type == "corridor") {
            auto voidIt = std::find_if(placedSpaces.begin(), placedSpaces.end(),
                                       [](const AllocatedSpace& placed) {
                                           return placed.type == "void";
                                       });
            if (voidIt != placedSpaces.end()) {
                const float corridorArea = space.areaPreferred > 0.0f ? space.areaPreferred : GetDefaultAreaForSpaceType(space.type);
                const float band = ComputeMallRingBand(corridorArea, *voidIt);
                for (float x = voidIt->position[0]; x <= voidIt->position[0] + voidIt->size[0] + 0.001f; x += m_gridSize) {
                    candidates.push_back(SnapToGrid({x, std::max(0.0f, voidIt->position[1] - band)}));
                    candidates.push_back(SnapToGrid({x, std::min(m_footprint[1] - band, voidIt->position[1] + voidIt->size[1])}));
                }
                for (float y = voidIt->position[1]; y <= voidIt->position[1] + voidIt->size[1] + 0.001f; y += m_gridSize) {
                    candidates.push_back(SnapToGrid({std::max(0.0f, voidIt->position[0] - band), y}));
                    candidates.push_back(SnapToGrid({std::min(m_footprint[0] - band, voidIt->position[0] + voidIt->size[0]), y}));
                }
            }
        } else if (strategy == LayoutStrategy::Mall && space.type == "lobby") {
            const float frontY = 0.0f;
            const float centerX = std::max(0.0f, std::round(((m_footprint[0] - size[0]) * 0.5f) / m_gridSize) * m_gridSize);
            candidates.push_back(SnapToGrid({centerX, frontY}));
            candidates.push_back(SnapToGrid({0.0f, frontY}));
            candidates.push_back(SnapToGrid({std::max(0.0f, m_footprint[0] - size[0]), frontY}));
        } else {
            const float centerY = std::max(0.0f, std::round(((m_footprint[1] - size[1]) * 0.5f) / m_gridSize) * m_gridSize);
            for (float x = 0.0f; x <= m_footprint[0] - size[0] + 0.001f; x += m_gridSize) {
                candidates.push_back(SnapToGrid({x, centerY}));
            }
        }
    } else if (space.type == "stairs" || space.type == "elevator") {
        auto corridorIt = std::find_if(placedSpaces.begin(), placedSpaces.end(),
                                       [](const AllocatedSpace& placed) {
                                           return placed.type == "corridor" || placed.type == "lobby" || placed.type == "core";
                                       });
        if (corridorIt != placedSpaces.end()) {
            candidates.push_back(SnapToGrid({corridorIt->position[0] + corridorIt->size[0], corridorIt->position[1]}));
            candidates.push_back(SnapToGrid({std::max(0.0f, corridorIt->position[0] - size[0]), corridorIt->position[1]}));
        }

        const float centerX = std::max(0.0f, std::round(((m_footprint[0] - size[0]) * 0.5f) / m_gridSize) * m_gridSize);
        const float centerY = std::max(0.0f, std::round(((m_footprint[1] - size[1]) * 0.5f) / m_gridSize) * m_gridSize);
        candidates.push_back(SnapToGrid({centerX, centerY}));
        candidates.push_back(SnapToGrid({centerX, std::max(0.0f, centerY - size[1] - m_gridSize)}));
    } else if (space.type == "entrance") {
        const float frontY = 0.0f;
        const float centerX = std::max(0.0f, std::round(((m_footprint[0] - size[0]) * 0.5f) / m_gridSize) * m_gridSize);
        candidates.push_back(SnapToGrid({0.0f, frontY}));
        candidates.push_back(SnapToGrid({centerX, frontY}));
        candidates.push_back(SnapToGrid({std::max(0.0f, m_footprint[0] - size[0]), frontY}));
    }

    for (const auto& candidate : candidates) {
        if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
            outPosition = candidate;
            return true;
        }
    }

    return false;
}

bool LayoutResolver::TryPlaceInsideUnitCluster(const SemanticSpace& space,
                                               const GridSize2D& size,
                                               const std::vector<AllocatedSpace>& placedSpaces,
                                               GridPos2D& outPosition) const
{
    if (space.unitId.empty()) {
        return false;
    }

    std::vector<const AllocatedSpace*> anchors;
    for (const auto& placed : placedSpaces) {
        if (placed.unitId == space.unitId) {
            anchors.push_back(&placed);
        }
    }

    for (const auto* anchor : anchors) {
        const std::vector<GridPos2D> candidates = {
            SnapToGrid({anchor->position[0] + anchor->size[0], anchor->position[1]}),
            SnapToGrid({anchor->position[0], anchor->position[1] + anchor->size[1]}),
            SnapToGrid({std::max(0.0f, anchor->position[0] - size[0]), anchor->position[1]}),
            SnapToGrid({anchor->position[0], std::max(0.0f, anchor->position[1] - size[1])})
        };

        for (const auto& candidate : candidates) {
            if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
                outPosition = candidate;
                return true;
            }
        }
    }

    return false;
}

bool LayoutResolver::TryPlaceInMallRetailBand(const SemanticBuilding& input,
                                              const SemanticSpace& space,
                                              const GridSize2D& size,
                                              const std::vector<AllocatedSpace>& placedSpaces,
                                              GridPos2D& outPosition) const
{
    if (GetLayoutStrategy(input) != LayoutStrategy::Mall || space.type != "shop") {
        return false;
    }

    const AllocatedSpace* voidAnchor = nullptr;
    const AllocatedSpace* corridorAnchor = nullptr;

    for (const auto& adjacency : space.adjacency) {
        auto connectedIt = std::find_if(placedSpaces.begin(), placedSpaces.end(),
            [&](const AllocatedSpace& placed) {
                return placed.spaceId == adjacency.to;
            });
        if (connectedIt != placedSpaces.end() &&
            (connectedIt->type == "corridor" || connectedIt->type == "lobby")) {
            corridorAnchor = &(*connectedIt);
            break;
        }
    }

    for (const auto& placed : placedSpaces) {
        if (placed.type == "void") {
            voidAnchor = &placed;
            break;
        }
    }

    if (voidAnchor == nullptr || corridorAnchor == nullptr) {
        return false;
    }

    const float band = ComputeMallRingBand(corridorAnchor->area, *voidAnchor);
    std::vector<GridPos2D> candidates;

    const float topY = std::max(0.0f, voidAnchor->position[1] - band - size[1]);
    const float bottomY = std::min(m_footprint[1] - size[1], voidAnchor->position[1] + voidAnchor->size[1] + band);
    const float leftX = std::max(0.0f, voidAnchor->position[0] - band - size[0]);
    const float rightX = std::min(m_footprint[0] - size[0], voidAnchor->position[0] + voidAnchor->size[0] + band);

    for (float x = std::max(0.0f, voidAnchor->position[0] - band);
         x <= std::min(m_footprint[0] - size[0], voidAnchor->position[0] + voidAnchor->size[0] + band - size[0]) + 0.001f;
         x += m_gridSize) {
        candidates.push_back(SnapToGrid({x, topY}));
        candidates.push_back(SnapToGrid({x, bottomY}));
    }
    for (float y = voidAnchor->position[1];
         y <= std::min(m_footprint[1] - size[1], voidAnchor->position[1] + voidAnchor->size[1] - size[1]) + 0.001f;
         y += m_gridSize) {
        candidates.push_back(SnapToGrid({leftX, y}));
        candidates.push_back(SnapToGrid({rightX, y}));
    }

    for (const auto& candidate : candidates) {
        if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
            outPosition = candidate;
            return true;
        }
    }

    return false;
}

bool LayoutResolver::TryPlaceOnExteriorWall(const SemanticSpace& space,
                                            const GridSize2D& size,
                                            const std::vector<AllocatedSpace>& placedSpaces,
                                            GridPos2D& outPosition) const
{
    const bool hardRequirement = RequiresExteriorWall(space);
    const bool softPreference = !hardRequirement && space.constraints.naturalLight == "preferred";
    if (!hardRequirement && !softPreference) {
        return false;
    }

    std::vector<GridPos2D> candidates;
    candidates.reserve(128);

    for (float x = 0.0f; x <= m_footprint[0] - size[0] + 0.001f; x += m_gridSize) {
        candidates.push_back(SnapToGrid({x, 0.0f}));
        candidates.push_back(SnapToGrid({x, std::max(0.0f, m_footprint[1] - size[1])}));
    }
    for (float y = 0.0f; y <= m_footprint[1] - size[1] + 0.001f; y += m_gridSize) {
        candidates.push_back(SnapToGrid({0.0f, y}));
        candidates.push_back(SnapToGrid({std::max(0.0f, m_footprint[0] - size[0]), y}));
    }

    auto scoreCandidate = [&](const GridPos2D& candidate) -> float {
        float score = 0.0f;

        for (const auto& adjacency : space.adjacency) {
            if (adjacency.relationship != "connected" && adjacency.relationship != "nearby") {
                continue;
            }

            auto it = std::find_if(placedSpaces.begin(), placedSpaces.end(),
                                   [&](const AllocatedSpace& placed) {
                                       return placed.spaceId == adjacency.to;
                                   });
            if (it == placedSpaces.end()) {
                continue;
            }

            const float dx = candidate[0] - it->position[0];
            const float dy = candidate[1] - it->position[1];
            const float distance = std::abs(dx) + std::abs(dy);
            score -= distance;
            if (adjacency.importance == "required") {
                score -= distance;
            }
        }

        return score;
    };

    std::sort(candidates.begin(), candidates.end(),
              [&](const GridPos2D& lhs, const GridPos2D& rhs) {
                  return scoreCandidate(lhs) > scoreCandidate(rhs);
              });

    for (const auto& candidate : candidates) {
        if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
            outPosition = candidate;
            return true;
        }
    }

    return false;
}

bool LayoutResolver::TryPlaceNearConnectedSpace(const SemanticSpace& space,
                                                const GridSize2D& size,
                                                const std::vector<AllocatedSpace>& placedSpaces,
                                                GridPos2D& outPosition) const
{
    struct AnchorRect {
        GridPos2D position;
        GridSize2D size;
    };

    auto getAnchorRects = [&](const AllocatedSpace& anchor) {
        std::vector<AnchorRect> anchorRects;
        for (const auto& reservedRect : m_reservedRects) {
            if (reservedRect.rectId == anchor.spaceId || reservedRect.rectId.rfind(anchor.spaceId + "_", 0) == 0) {
                anchorRects.push_back({reservedRect.origin, reservedRect.size});
            }
        }

        if (anchorRects.empty()) {
            anchorRects.push_back({anchor.position, anchor.size});
        }

        return anchorRects;
    };

    auto scoreCandidate = [&](const GridPos2D& candidate,
                              const AllocatedSpace& anchor,
                              const AnchorRect& anchorRect) -> float {
        float score = 0.0f;

        if (candidate[0] <= 0.001f || candidate[1] <= 0.001f ||
            candidate[0] + size[0] >= m_footprint[0] - 0.001f ||
            candidate[1] + size[1] >= m_footprint[1] - 0.001f) {
            score += 50.0f;
        }

        if (anchor.type == "corridor" || anchor.type == "lobby" || anchor.type == "core") {
            score += 100.0f;
        }

        const float centerX = candidate[0] + size[0] * 0.5f;
        const float centerY = candidate[1] + size[1] * 0.5f;
        const float anchorCenterX = anchorRect.position[0] + anchorRect.size[0] * 0.5f;
        const float anchorCenterY = anchorRect.position[1] + anchorRect.size[1] * 0.5f;
        score -= std::abs(centerX - anchorCenterX) + std::abs(centerY - anchorCenterY);

        return score;
    };

    auto tryAroundAnchor = [&](const AllocatedSpace& anchor) -> bool {
        std::vector<GridPos2D> candidates;

        for (const auto& anchorRect : getAnchorRects(anchor)) {
            candidates.push_back(SnapToGrid({anchorRect.position[0] + anchorRect.size[0], anchorRect.position[1]}));
            candidates.push_back(SnapToGrid({anchorRect.position[0], anchorRect.position[1] + anchorRect.size[1]}));
            candidates.push_back(SnapToGrid({anchorRect.position[0] - size[0], anchorRect.position[1]}));
            candidates.push_back(SnapToGrid({anchorRect.position[0], anchorRect.position[1] - size[1]}));

            const float horizontalStart = anchorRect.position[0] - size[0] + m_gridSize;
            const float horizontalEnd = anchorRect.position[0] + anchorRect.size[0] - m_gridSize;
            for (float x = horizontalStart; x <= horizontalEnd + 0.001f; x += m_gridSize) {
                candidates.push_back(SnapToGrid({x, anchorRect.position[1] - size[1]}));
                candidates.push_back(SnapToGrid({x, anchorRect.position[1] + anchorRect.size[1]}));
            }

            const float verticalStart = anchorRect.position[1] - size[1] + m_gridSize;
            const float verticalEnd = anchorRect.position[1] + anchorRect.size[1] - m_gridSize;
            for (float y = verticalStart; y <= verticalEnd + 0.001f; y += m_gridSize) {
                candidates.push_back(SnapToGrid({anchorRect.position[0] - size[0], y}));
                candidates.push_back(SnapToGrid({anchorRect.position[0] + anchorRect.size[0], y}));
            }
        }

        float bestScore = -std::numeric_limits<float>::infinity();
        bool foundCandidate = false;
        GridPos2D bestCandidate = {0.0f, 0.0f};

        for (const auto& candidate : candidates) {
            if (!FitsWithoutOverlap(candidate, size, placedSpaces)) {
                continue;
            }

            float candidateScore = -std::numeric_limits<float>::infinity();
            for (const auto& anchorRect : getAnchorRects(anchor)) {
                candidateScore = std::max(candidateScore, scoreCandidate(candidate, anchor, anchorRect));
            }

            if (!foundCandidate || candidateScore > bestScore) {
                bestScore = candidateScore;
                bestCandidate = candidate;
                foundCandidate = true;
            }
        }

        if (foundCandidate) {
            outPosition = bestCandidate;
            return true;
        }

        return false;
    };

    for (const auto& adjacency : space.adjacency) {
        if ((adjacency.relationship != "connected" && adjacency.relationship != "nearby" &&
             adjacency.relationship != "around" && adjacency.relationship != "share_wall") ||
            adjacency.importance == "optional") {
            continue;
        }

        auto it = std::find_if(placedSpaces.begin(), placedSpaces.end(),
                               [&](const AllocatedSpace& placed) {
                                   return placed.spaceId == adjacency.to;
                               });
        if (it != placedSpaces.end() && tryAroundAnchor(*it)) {
            return true;
        }
    }

    return false;
}

bool LayoutResolver::TryPlaceNearCoreOrCirculation(const SemanticBuilding& input,
                                                   const SemanticSpace& space,
                                                   const GridSize2D& size,
                                                   const std::vector<AllocatedSpace>& placedSpaces,
                                                   GridPos2D& outPosition) const
{
    const bool isOfficeFamily = input.buildingType == "office" || input.buildingType == "office_tower";
    const bool isResidentialTower = input.buildingType == "cbd_residential" || input.buildingType == "apartment";
    const bool isMallRetail = input.buildingType == "mall" || input.buildingType == "shopping_center" ||
        input.buildingType == "retail_center";

    bool shouldPreferCirculation = false;
    if (isOfficeFamily) {
        shouldPreferCirculation = space.zone == "public" && space.unitId.empty() &&
            (space.type == "office" || space.type == "meeting_room" || space.type == "shop");
    } else if (isResidentialTower) {
        shouldPreferCirculation = space.zone == "public" && space.unitId.empty();
    } else if (isMallRetail) {
        shouldPreferCirculation = space.type == "shop" || space.type == "bathroom";
    }

    if (!shouldPreferCirculation) {
        return false;
    }

    auto tryAroundAnchor = [&](const AllocatedSpace& anchor) -> bool {
        const std::vector<GridPos2D> candidates = {
            SnapToGrid({anchor.position[0] + anchor.size[0], anchor.position[1]}),
            SnapToGrid({anchor.position[0], anchor.position[1] + anchor.size[1]}),
            SnapToGrid({std::max(0.0f, anchor.position[0] - size[0]), anchor.position[1]}),
            SnapToGrid({anchor.position[0], std::max(0.0f, anchor.position[1] - size[1])})
        };

        for (const auto& candidate : candidates) {
            if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
                outPosition = candidate;
                return true;
            }
        }

        return false;
    };

    std::vector<const AllocatedSpace*> anchors;
    for (const auto& placed : placedSpaces) {
        if (placed.type == "core" || placed.type == "corridor" ||
            placed.type == "lobby" || placed.type == "entrance") {
            anchors.push_back(&placed);
        }
    }

    std::sort(anchors.begin(), anchors.end(),
              [](const AllocatedSpace* lhs, const AllocatedSpace* rhs) {
                  auto anchorWeight = [](const AllocatedSpace* anchor) {
                      if (anchor->type == "corridor") return 4;
                      if (anchor->type == "lobby") return 3;
                      if (anchor->type == "core") return 2;
                      return 1;
                  };
                  return anchorWeight(lhs) > anchorWeight(rhs);
              });

    for (const auto* anchor : anchors) {
        if (tryAroundAnchor(*anchor)) {
            return true;
        }
    }

    return false;
}

float LayoutResolver::GetDefaultAreaForSpaceType(const std::string& spaceType) const
{
    // 建筑标准：合理的房间面积范围（单位：平方米）
    // 参考国际建筑标准和住宅设计规范
    
    // 主要生活空间
    if (spaceType == "living")          return 30.0f;  // 客厅：25-40 m²
    if (spaceType == "dining")          return 15.0f;  // 餐厅：12-20 m²
    if (spaceType == "kitchen")         return 12.0f;  // 厨房：8-15 m²
    
    // 卧室
    if (spaceType == "bedroom")         return 15.0f;  // 标准卧室：12-18 m²
    if (spaceType == "master_bedroom")  return 20.0f;  // 主卧：18-25 m²
    
    // 卫生间
    if (spaceType == "bathroom")        return 5.0f;   // 卫生间：4-8 m²
    if (spaceType == "toilet")          return 3.0f;   // 独立厕所：2-4 m²
    
    // 功能空间
    if (spaceType == "entrance")        return 6.0f;   // 入口：4-8 m²
    if (spaceType == "corridor")        return 8.0f;   // 走廊：5-12 m²
    if (spaceType == "stairs")          return 6.0f;   // 楼梯：4-8 m²
    if (spaceType == "storage")         return 4.0f;   // 储藏室：3-6 m²
    if (spaceType == "closet")          return 3.0f;   // 衣柜间：2-4 m²
    
    // 工作学习空间
    if (spaceType == "office")          return 12.0f;  // 办公室：10-15 m²
    if (spaceType == "meeting_room")    return 20.0f;
    if (spaceType == "lobby")           return 40.0f;
    if (spaceType == "core")            return 60.0f;
    if (spaceType == "void")            return 120.0f;
    if (spaceType == "shop")            return 80.0f;
    if (spaceType == "mechanical")      return 12.0f;
    if (spaceType == "elevator")        return 8.0f;
    if (spaceType == "study")           return 10.0f;  // 书房：8-12 m²
    
    // 休闲娱乐空间
    if (spaceType == "family_room")     return 20.0f;  // 家庭活动室：15-25 m²
    if (spaceType == "media_room")      return 18.0f;  // 影音室：15-22 m²
    if (spaceType == "gym")             return 15.0f;  // 健身房：12-20 m²
    if (spaceType == "game_room")       return 18.0f;  // 游戏室：15-22 m²
    
    // 服务空间
    if (spaceType == "laundry")         return 6.0f;   // 洗衣房：4-8 m²
    if (spaceType == "utility")         return 5.0f;   // 杂物间：3-6 m²
    if (spaceType == "pantry")          return 4.0f;   // 食品储藏室：3-5 m²
    
    // 室外空间
    if (spaceType == "balcony")         return 6.0f;   // 阳台：4-10 m²
    if (spaceType == "terrace")         return 12.0f;  // 露台：8-20 m²
    if (spaceType == "patio")           return 15.0f;  // 庭院：10-25 m²
    
    // 车库
    if (spaceType == "garage")          return 18.0f;  // 车库（单车位）：15-20 m²
    if (spaceType == "carport")         return 15.0f;  // 车棚：12-18 m²
    
    // 默认值（未识别的房间类型）
    MOON_LOG_WARN("LayoutResolver", "Unknown space type '%s', using default area 10 m²", 
                  spaceType.c_str());
    return 10.0f;
}

float LayoutResolver::CalculateTotalArea(const std::vector<SemanticSpace>& spaces) const
{
    float total = 0.0f;
    for (const auto& space : spaces) {
        if (space.areaPreferred > 0.0f) {
            total += space.areaPreferred;
        } else {
            // 如果没有指定面积，使用默认值
            float defaultArea = GetDefaultAreaForSpaceType(space.type);
            total += defaultArea;
        }
    }
    return total;
}

GridSize2D LayoutResolver::CalculateOptimalDimensions(float area, float aspectRatio) const
{
    // Given area and desired aspect ratio (width/height)
    // area = width * height
    // aspectRatio = width / height
    // Therefore: height = sqrt(area / aspectRatio)
    //            width = area / height
    
    float height = std::sqrt(area / aspectRatio);
    float width = area / height;
    
    return {width, height};
}

GridPos2D LayoutResolver::SnapToGrid(const GridPos2D& v) const
{
    return {
        std::round(v[0] / m_gridSize) * m_gridSize,
        std::round(v[1] / m_gridSize) * m_gridSize
    };
}

Rect LayoutResolver::CreateRect(const std::string& rectId, const GridPos2D& origin, const GridSize2D& size) const
{
    Rect rect;
    rect.rectId = rectId;
    rect.origin = origin;
    rect.size = size;
    return rect;
}

} // namespace Building
} // namespace Moon
