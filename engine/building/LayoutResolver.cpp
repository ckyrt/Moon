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
                        space.type = spaceJson.value("type", "");
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

bool LayoutResolver::Resolve(
    const SemanticBuilding& input,
    BuildingDefinition& output,
    std::string& error)
{
    MOON_LOG_INFO("LayoutResolver", "=== Starting Layout Resolution ===");
    MOON_LOG_INFO("LayoutResolver", "Building type: %s", input.buildingType.c_str());
    
    // Step 1: Calculate footprint
    if (!CalculateFootprint(input)) {
        error = "Failed to calculate footprint";
        return false;
    }
    
    // Step 2: Allocate spaces
    if (!AllocateSpaces(input)) {
        error = "Failed to allocate spaces";
        return false;
    }
    
    // Step 3: Generate rectangles
    if (!GenerateRectangles(input)) {
        error = "Failed to generate rectangles";
        return false;
    }
    
    // Step 4: Build output
    BuildOutput(output, input);
    
    MOON_LOG_INFO("LayoutResolver", "=== Layout Resolution Complete ===");
    return true;
}

bool LayoutResolver::CalculateFootprint(const SemanticBuilding& input)
{
    float totalArea = input.mass.footprintArea;
    int floors = input.mass.floors;
    
    if (totalArea <= 0.0f || floors <= 0) {
        MOON_LOG_ERROR("LayoutResolver", "Invalid mass constraints");
        return false;
    }
    
    // footprint_area in V11 is already the per-floor building footprint.
    float floorArea = totalArea;
    
    // Use 1.5:1 aspect ratio for residential buildings
    float aspectRatio = 1.5f;
    
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
        const auto orderedSpaces = BuildPlacementOrder(floor);
        
        MOON_LOG_INFO("LayoutResolver", "  Total preferred area: %.1f m²", totalPreferredArea);
        MOON_LOG_INFO("LayoutResolver", "  Available area: %.1f m²", availableArea);
        
        auto tryAllocateFloor = [&](float candidateScale,
                                    std::vector<AllocatedSpace>& outSpaces,
                                    float& outUsedHeight) -> bool {
            outSpaces.clear();

            outUsedHeight = 0.0f;

            for (const auto* spacePtr : orderedSpaces) {
                const auto& space = *spacePtr;
                AllocatedSpace allocated;
                allocated.spaceId = space.spaceId;
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

                GridPos2D position = {0.0f, 0.0f};
                bool placed = TryPlaceUsingStairAlignment(space, dims, outSpaces, position) ||
                              TryPlaceInCirculationBand(space, dims, outSpaces, position);

                if (!placed && RequiresExteriorWall(space)) {
                    placed = TryPlaceOnExteriorWall(space, dims, outSpaces, position);
                } else if (!placed) {
                    placed = TryPlaceOnExteriorWall(space, dims, outSpaces, position) ||
                             TryPlaceNearConnectedSpace(space, dims, outSpaces, position) ||
                             FindFirstFitPosition(dims, outSpaces, position);
                }

                if (!placed) {
                    outUsedHeight = m_footprint[1] + dims[1];
                    return false;
                }

                allocated.position = position;
                allocated.size = dims;
                outSpaces.push_back(allocated);

                outUsedHeight = std::max(outUsedHeight, allocated.position[1] + allocated.size[1]);
            }
            return outUsedHeight <= m_footprint[1] + 0.001f;
        };

        std::vector<AllocatedSpace> floorAllocated;
        float usedHeight = 0.0f;
        bool allocatedFloor = false;
        const float initialScaleFactor = scaleFactor;

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
            MOON_LOG_ERROR("LayoutResolver",
                           "  Failed to fit floor %d within footprint %.1f x %.1f m",
                           floor.level, m_footprint[0], m_footprint[1]);
            return false;
        }

        if (scaleFactor < initialScaleFactor - 0.001f) {
            MOON_LOG_INFO("LayoutResolver",
                          "  Adjusted scale factor from %.2f to %.2f to fit floor height %.1f m",
                          initialScaleFactor, scaleFactor, m_footprint[1]);
        } else {
            MOON_LOG_INFO("LayoutResolver", "  Scale factor: %.2f", scaleFactor);
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
    output.schema = "moon_building_v8";  // Output is V8/V10 format
    output.grid = m_gridSize;
    
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
        floor.floorHeight = 3.0f;  // Default
        
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
            
            const AllocatedSpace& allocated = *it;
            
            Space space;
            space.spaceId = spaceIdCounter++;
            
            // Create rect
            GridPos2D origin = SnapToGrid(allocated.position);
            GridSize2D size = SnapToGrid(allocated.size);
            
            Rect rect = CreateRect(semanticSpace.spaceId, origin, size);
            space.rects.push_back(rect);
            
            // Set properties
            space.properties.usage = StringToSpaceUsage(semanticSpace.type);
            space.properties.isOutdoor = (semanticSpace.type == "balcony" || 
                                         semanticSpace.type == "terrace");
            space.properties.hasStairs = (semanticSpace.type == "stairs");
            space.properties.ceilingHeight = semanticSpace.constraints.ceilingHeight;
            
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
    return space.type == "corridor" || space.type == "stairs" || space.type == "entrance";
}

bool LayoutResolver::RequiresExteriorWall(const SemanticSpace& space) const
{
    return space.constraints.exteriorAccess || space.constraints.naturalLight == "required";
}

float LayoutResolver::GetTargetAspectRatio(const SemanticSpace& space) const
{
    float target = 1.5f;

    if (space.type == "corridor") {
        target = 4.0f;
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

std::vector<const SemanticSpace*> LayoutResolver::BuildPlacementOrder(const SemanticFloor& floor) const
{
    std::vector<const SemanticSpace*> remaining;
    remaining.reserve(floor.spaces.size());
    for (const auto& space : floor.spaces) {
        remaining.push_back(&space);
    }

    auto baseScore = [&](const SemanticSpace& space) -> float {
        float score = static_cast<float>(GetPriorityWeight(space.priority) * 1000);
        score += space.areaPreferred > 0.0f ? space.areaPreferred : GetDefaultAreaForSpaceType(space.type);

        if (space.type == "corridor") {
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
                    continue;
                }

                if (adjacency.relationship == "connected") {
                    score += adjacency.importance == "required" ? 500.0f : 180.0f;
                } else if (adjacency.relationship == "nearby") {
                    score += 80.0f;
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
                                        const std::vector<AllocatedSpace>& placedSpaces) const
{
    const float epsilon = 0.001f;

    if (position[0] < -epsilon || position[1] < -epsilon) {
        return false;
    }
    if (position[0] + size[0] > m_footprint[0] + epsilon ||
        position[1] + size[1] > m_footprint[1] + epsilon) {
        return false;
    }

    for (const auto& placed : placedSpaces) {
        if (RectanglesOverlap(position, size, placed.position, placed.size)) {
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
    if (space.type != "stairs" || space.constraints.connectsFromFloor < 0) {
        return false;
    }

    for (const auto& allocated : m_allocatedSpaces) {
        if (allocated.type != "stairs" || allocated.floorLevel != space.constraints.connectsFromFloor) {
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

bool LayoutResolver::TryPlaceInCirculationBand(const SemanticSpace& space,
                                               const GridSize2D& size,
                                               const std::vector<AllocatedSpace>& placedSpaces,
                                               GridPos2D& outPosition) const
{
    if (!IsCirculationSpace(space)) {
        return false;
    }

    std::vector<GridPos2D> candidates;

    if (space.type == "corridor") {
        const float centerY = std::max(0.0f, std::round(((m_footprint[1] - size[1]) * 0.5f) / m_gridSize) * m_gridSize);
        for (float x = 0.0f; x <= m_footprint[0] - size[0] + 0.001f; x += m_gridSize) {
            candidates.push_back(SnapToGrid({x, centerY}));
        }
    } else if (space.type == "stairs") {
        auto corridorIt = std::find_if(placedSpaces.begin(), placedSpaces.end(),
                                       [](const AllocatedSpace& placed) {
                                           return placed.type == "corridor";
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
    auto tryAroundAnchor = [&](const AllocatedSpace& anchor) -> bool {
        const std::vector<GridPos2D> candidates = {
            SnapToGrid({anchor.position[0] + anchor.size[0], anchor.position[1]}),
            SnapToGrid({anchor.position[0], anchor.position[1] + anchor.size[1]}),
            SnapToGrid({anchor.position[0] - size[0], anchor.position[1]}),
            SnapToGrid({anchor.position[0], anchor.position[1] - size[1]})
        };

        for (const auto& candidate : candidates) {
            if (FitsWithoutOverlap(candidate, size, placedSpaces)) {
                outPosition = candidate;
                return true;
            }
        }

        return false;
    };

    for (const auto& adjacency : space.adjacency) {
        if (adjacency.relationship != "connected" || adjacency.importance == "optional") {
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

// ============================================================================
// BuildingDefinitionSerializer Implementation
// ============================================================================

namespace {
    // Helper function to snap float values to 0.5m grid
    inline float SnapToGrid(float value, float gridSize = 0.5f) {
        return std::round(value / gridSize) * gridSize;
    }
}

std::string BuildingDefinitionSerializer::Serialize(const BuildingDefinition& building)
{
    json j;
    
    // Schema and grid
    j["schema"] = building.schema;
    j["grid"] = building.grid;
    
    // Style
    j["style"] = {
        {"category", building.style.category},
        {"facade", building.style.facade},
        {"roof", building.style.roof},
        {"window_style", building.style.windowStyle},
        {"material", building.style.material}
    };
    
    // Masses
    j["masses"] = json::array();
    for (const auto& mass : building.masses) {
        j["masses"].push_back({
            {"mass_id", mass.massId},
            {"origin", {SnapToGrid(mass.origin[0]), SnapToGrid(mass.origin[1])}},
            {"size", {SnapToGrid(mass.size[0]), SnapToGrid(mass.size[1])}},
            {"floors", mass.floors}
        });
    }
    
    // Floors
    j["floors"] = json::array();
    for (const auto& floor : building.floors) {
        json floorJson;
        floorJson["level"] = floor.level;
        floorJson["mass_id"] = floor.massId;
        floorJson["floor_height"] = SnapToGrid(floor.floorHeight);
        
        floorJson["spaces"] = json::array();
        for (const auto& space : floor.spaces) {
            json spaceJson;
            spaceJson["space_id"] = space.spaceId;
            
            // Rects
            spaceJson["rects"] = json::array();
            for (const auto& rect : space.rects) {
                spaceJson["rects"].push_back({
                    {"rect_id", rect.rectId},
                    {"origin", {SnapToGrid(rect.origin[0]), SnapToGrid(rect.origin[1])}},
                    {"size", {SnapToGrid(rect.size[0]), SnapToGrid(rect.size[1])}}
                });
            }
            
            // Properties
            spaceJson["properties"] = {
                {"usage_hint", SpaceUsageToString(space.properties.usage)},
                {"is_outdoor", space.properties.isOutdoor},
                {"has_stairs", space.properties.hasStairs},
                {"ceiling_height", SnapToGrid(space.properties.ceilingHeight)}
            };
            
            floorJson["spaces"].push_back(spaceJson);
        }
        
        j["floors"].push_back(floorJson);
    }
    
    return j.dump(2);  // Pretty print with 2 space indent
}

} // namespace Building
} // namespace Moon
