#include "SemanticBuildingResolver.h"

#include "LayoutResolver.h"
#include "SpaceGraphBuilder.h"

#include <sstream>
#include <unordered_map>
#include <vector>

namespace {

bool RectMatchesSemanticSpaceId(const Moon::Building::Rect& rect, const std::string& semanticSpaceId) {
    return rect.rectId == semanticSpaceId || rect.rectId.rfind(semanticSpaceId + "_", 0) == 0;
}

const Moon::Building::Floor* FindResolvedFloor(const Moon::Building::BuildingDefinition& resolvedBuilding, int level) {
    for (const auto& floor : resolvedBuilding.floors) {
        if (floor.level == level) {
            return &floor;
        }
    }
    return nullptr;
}

const Moon::Building::Space* FindResolvedSpace(const Moon::Building::Floor& floor, const std::string& semanticSpaceId) {
    for (const auto& space : floor.spaces) {
        for (const auto& rect : space.rects) {
            if (RectMatchesSemanticSpaceId(rect, semanticSpaceId)) {
                return &space;
            }
        }
    }
    return nullptr;
}

bool HasMallRingCorridorRects(const Moon::Building::Space& space, const std::string& semanticSpaceId) {
    bool hasTop = false;
    bool hasBottom = false;
    bool hasLeft = false;
    bool hasRight = false;

    for (const auto& rect : space.rects) {
        hasTop = hasTop || rect.rectId == semanticSpaceId + "_top";
        hasBottom = hasBottom || rect.rectId == semanticSpaceId + "_bottom";
        hasLeft = hasLeft || rect.rectId == semanticSpaceId + "_left";
        hasRight = hasRight || rect.rectId == semanticSpaceId + "_right";
    }

    return hasTop && hasBottom && hasLeft && hasRight;
}

std::string DescribeResolvedSpace(const Moon::Building::Space& space) {
    std::ostringstream oss;
    oss << "spaceId=" << space.spaceId << " rects=[";
    for (size_t index = 0; index < space.rects.size(); ++index) {
        const auto& rect = space.rects[index];
        if (index > 0) {
            oss << "; ";
        }
        oss << rect.rectId << "@(" << rect.origin[0] << "," << rect.origin[1] << ")"
            << " size(" << rect.size[0] << "x" << rect.size[1] << ")";
    }
    oss << "]";
    return oss.str();
}

std::string DescribeResolvedFloor(const Moon::Building::Floor& floor) {
    std::ostringstream oss;
    oss << "floor=" << floor.level << " spaces={";
    for (size_t index = 0; index < floor.spaces.size(); ++index) {
        if (index > 0) {
            oss << " | ";
        }
        oss << DescribeResolvedSpace(floor.spaces[index]);
    }
    oss << "}";
    return oss.str();
}

} // namespace

namespace Moon {
namespace Building {

bool SemanticBuildingResolver::Resolve(const SemanticBuilding& semanticBuilding,
                                       BuildingDefinition& outDefinition,
                                       std::string& outError) const {
    LayoutResolver resolver;
    resolver.SetGridSize(semanticBuilding.grid);
    return resolver.Resolve(semanticBuilding, outDefinition, outError);
}

bool SemanticBuildingResolver::ValidateResolvedConsistency(const SemanticBuilding& semanticBuilding,
                                                           const BuildingDefinition& resolvedBuilding,
                                                           std::string& outError) const {
    SpaceGraphBuilder graphBuilder;
    std::vector<SpaceConnection> connections;
    graphBuilder.BuildGraph(resolvedBuilding, connections);

    for (const auto& semanticFloor : semanticBuilding.floors) {
        const Floor* resolvedFloor = FindResolvedFloor(resolvedBuilding, semanticFloor.level);
        if (resolvedFloor == nullptr) {
            outError = "Resolved building is missing floor level " + std::to_string(semanticFloor.level);
            return false;
        }

        std::unordered_map<std::string, const Space*> resolvedSpacesBySemanticId;
        for (const auto& semanticSpace : semanticFloor.spaces) {
            if (semanticSpace.type == "void") {
                continue;
            }

            const Space* resolvedSpace = FindResolvedSpace(*resolvedFloor, semanticSpace.spaceId);
            if (resolvedSpace == nullptr) {
                outError = "Resolved layout is missing semantic space '" + semanticSpace.spaceId +
                    "' on floor " + std::to_string(semanticFloor.level);
                return false;
            }

            resolvedSpacesBySemanticId.emplace(semanticSpace.spaceId, resolvedSpace);

            for (const auto& adjacency : semanticSpace.adjacency) {
                if (adjacency.relationship != "around") {
                    continue;
                }

                if ((semanticSpace.type == "corridor" || semanticSpace.type == "lobby") &&
                    !HasMallRingCorridorRects(*resolvedSpace, semanticSpace.spaceId)) {
                    outError = "Resolved mall circulation space '" + semanticSpace.spaceId +
                        "' lost its ring-corridor geometry on floor " + std::to_string(semanticFloor.level);
                    return false;
                }
            }
        }

        for (const auto& semanticSpace : semanticFloor.spaces) {
            if (semanticSpace.type == "void") {
                continue;
            }

            auto fromIt = resolvedSpacesBySemanticId.find(semanticSpace.spaceId);
            if (fromIt == resolvedSpacesBySemanticId.end()) {
                continue;
            }

            for (const auto& adjacency : semanticSpace.adjacency) {
                if (adjacency.importance != "required") {
                    continue;
                }

                if (adjacency.relationship == "around" || adjacency.relationship == "facing") {
                    continue;
                }

                auto toIt = resolvedSpacesBySemanticId.find(adjacency.to);
                if (toIt == resolvedSpacesBySemanticId.end()) {
                    continue;
                }

                if ((adjacency.relationship == "connected" || adjacency.relationship == "share_wall") &&
                    !graphBuilder.AreSpacesAdjacent(fromIt->second->spaceId, toIt->second->spaceId)) {
                    outError = "Resolved layout broke required adjacency between '" + semanticSpace.spaceId +
                        "' and '" + adjacency.to + "' on floor " + std::to_string(semanticFloor.level) +
                        ": " + DescribeResolvedSpace(*fromIt->second) +
                        " | target=" + DescribeResolvedSpace(*toIt->second) +
                        " | layout=" + DescribeResolvedFloor(*resolvedFloor);
                    return false;
                }
            }
        }
    }

    return true;
}

} // namespace Building
} // namespace Moon
