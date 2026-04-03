#include "BuildingGenerationInputs.h"
#include "../../external/nlohmann/json.hpp"
#include <unordered_map>

using json = nlohmann::json;

namespace Moon {
namespace Building {

const FloorLayoutInput* BuildingLayoutInput::FindFloor(int level) const {
    for (const auto& floor : floors) {
        if (floor.level == level) {
            return &floor;
        }
    }
    return nullptr;
}

BuildingFormInput ExtractBuildingFormInput(const SemanticBuilding& building) {
    BuildingFormInput input;
    input.buildingType = building.buildingType;
    input.style = building.style;
    input.mass = building.mass;
    return input;
}

BuildingLayoutInput ExtractBuildingLayoutInput(const SemanticBuilding& building) {
    BuildingLayoutInput input;
    input.floors.reserve(building.floors.size());

    for (const auto& floor : building.floors) {
        FloorLayoutInput layoutFloor;
        layoutFloor.level = floor.level;
        layoutFloor.name = floor.name;
        layoutFloor.spaces = floor.spaces;
        for (const auto& system : building.verticalSystems) {
            if (floor.level >= system.floorFrom && floor.level <= system.floorTo) {
                layoutFloor.verticalSystems.push_back(system);
            }
        }
        input.floors.push_back(std::move(layoutFloor));
    }

    input.verticalSystems = building.verticalSystems;

    return input;
}

void BuildFloorSpacesFromResolvedLayout(const ResolvedFloorLayout& resolvedFloor,
                                        int& ioNextSpaceId,
                                        std::vector<Space>& outSpaces) {
    outSpaces.clear();
    outSpaces.reserve(resolvedFloor.spaces.size());

    for (const auto& resolvedSpace : resolvedFloor.spaces) {
        Space space;
        space.spaceId = ioNextSpaceId++;
        space.properties.usage = resolvedSpace.usage;
        space.properties.isOutdoor = resolvedSpace.isOutdoor;
        space.properties.hasStairs = resolvedSpace.hasStairs;
        space.properties.ceilingHeight = resolvedSpace.ceilingHeight;

        Rect rect = resolvedSpace.rect;
        rect.rectId = resolvedSpace.spaceId;
        space.rects.push_back(rect);

        if (resolvedSpace.hasStairs) {
            space.stairsConfig.connectToLevel = resolvedSpace.stairConnectToLevel >= 0
                ? resolvedSpace.stairConnectToLevel
                : resolvedFloor.level + 1;
            space.stairsConfig.type = resolvedSpace.stairType;
            space.stairsConfig.width = resolvedSpace.stairWidth > 0.0f
                ? resolvedSpace.stairWidth
                : rect.size[0];
            space.stairsConfig.position = resolvedSpace.stairPosition;
            space.stairsConfig.rotationDegrees = resolvedSpace.stairRotationDegrees;
            space.stairsConfig.footprintRect = rect;
        }

        outSpaces.push_back(std::move(space));
    }
}

void BuildVerticalTransportsFromResolvedLayout(const ResolvedBuildingLayout& resolvedLayout,
                                               std::vector<VerticalTransport>& outVerticalTransports) {
    outVerticalTransports.clear();
    std::unordered_map<std::string, size_t> transportIndexByKey;

    for (const auto& floor : resolvedLayout.floors) {
        for (const auto& resolvedTransport : floor.verticalTransports) {
            std::string key = resolvedTransport.transportId;
            if (key.empty()) {
                key = (resolvedTransport.type == VerticalTransportType::Elevator ? "elevator" : "stair") +
                    std::string("_") + std::to_string(resolvedTransport.floorFrom) +
                    "_" + std::to_string(resolvedTransport.floorTo);
            }
            if (!resolvedTransport.continuousShaft) {
                key += "_segment_" + std::to_string(resolvedTransport.sourceFloorLevel);
            }

            auto existing = transportIndexByKey.find(key);
            if (existing != transportIndexByKey.end()) {
                auto& transport = outVerticalTransports[existing->second];
                transport.floorFrom = std::min(transport.floorFrom, resolvedTransport.floorFrom);
                transport.floorTo = std::max(transport.floorTo, resolvedTransport.floorTo);
                transport.continuousShaft = transport.continuousShaft || resolvedTransport.continuousShaft;
                transport.enclosed = transport.enclosed || resolvedTransport.enclosed;
                transport.external = transport.external && resolvedTransport.external;
                continue;
            }

            VerticalTransport transport;
            transport.transportId = resolvedTransport.transportId;
            transport.type = resolvedTransport.type;
            transport.shaftRect = resolvedTransport.shaftRect;
            transport.openingRect = resolvedTransport.openingRect;
            transport.floorFrom = resolvedTransport.floorFrom;
            transport.floorTo = resolvedTransport.floorTo;
            transport.sourceFloorLevel = resolvedTransport.sourceFloorLevel;
            transport.continuousShaft = resolvedTransport.continuousShaft;
            transport.enclosed = resolvedTransport.enclosed;
            transport.external = resolvedTransport.external;
            transport.stairType = resolvedTransport.stairType;
            transport.width = resolvedTransport.width;
            transport.position = resolvedTransport.position;
            transport.rotationDegrees = resolvedTransport.rotationDegrees;
            outVerticalTransports.push_back(std::move(transport));
            transportIndexByKey.emplace(std::move(key), outVerticalTransports.size() - 1);
        }
    }
}

bool SerializeResolvedBuildingLayout(const BuildingFormInput* formInput,
                                     const ResolvedBuildingLayout& resolvedLayout,
                                     std::string& outJson,
                                     std::string& outError) {
    try {
        json root;
        root["schema"] = "moon_resolved_layout";
        root["building_type"] = formInput ? formInput->buildingType : resolvedLayout.buildingType;
        root["floors"] = json::array();

        for (const auto& floor : resolvedLayout.floors) {
            json floorJson;
            floorJson["level"] = floor.level;
            floorJson["spaces"] = json::array();
            floorJson["vertical_transports"] = json::array();

            for (const auto& space : floor.spaces) {
                json spaceJson;
                spaceJson["space_id"] = space.spaceId;
                spaceJson["usage"] = SpaceUsageToString(space.usage);
                spaceJson["rect"] = {
                    {"origin", {space.rect.origin[0], space.rect.origin[1]}},
                    {"size", {space.rect.size[0], space.rect.size[1]}}
                };
                spaceJson["ceiling_height"] = space.ceilingHeight;
                if (space.isOutdoor) {
                    spaceJson["is_outdoor"] = true;
                }
                if (space.hasStairs) {
                    spaceJson["has_stairs"] = true;
                    spaceJson["stair_connect_to_level"] = space.stairConnectToLevel;
                    spaceJson["stair_rotation_degrees"] = space.stairRotationDegrees;
                }
                floorJson["spaces"].push_back(std::move(spaceJson));
            }

            for (const auto& transport : floor.verticalTransports) {
                json transportJson;
                transportJson["transport_id"] = transport.transportId;
                transportJson["type"] = transport.type == VerticalTransportType::Elevator ? "elevator" : "stair";
                transportJson["shaft_rect"] = {
                    {"origin", {transport.shaftRect.origin[0], transport.shaftRect.origin[1]}},
                    {"size", {transport.shaftRect.size[0], transport.shaftRect.size[1]}}
                };
                transportJson["opening_rect"] = {
                    {"origin", {transport.openingRect.origin[0], transport.openingRect.origin[1]}},
                    {"size", {transport.openingRect.size[0], transport.openingRect.size[1]}}
                };
                transportJson["floor_from"] = transport.floorFrom;
                transportJson["floor_to"] = transport.floorTo;
                transportJson["source_floor_level"] = transport.sourceFloorLevel;
                transportJson["continuous_shaft"] = transport.continuousShaft;
                transportJson["enclosed"] = transport.enclosed;
                transportJson["external"] = transport.external;
                transportJson["width"] = transport.width;
                transportJson["position"] = {transport.position[0], transport.position[1]};
                transportJson["rotation_degrees"] = transport.rotationDegrees;
                if (transport.type == VerticalTransportType::Stair) {
                    switch (transport.stairType) {
                        case StairType::Straight: transportJson["stair_type"] = "straight"; break;
                        case StairType::L: transportJson["stair_type"] = "l"; break;
                        case StairType::U: transportJson["stair_type"] = "u"; break;
                        case StairType::Spiral: transportJson["stair_type"] = "spiral"; break;
                    }
                }
                floorJson["vertical_transports"].push_back(std::move(transportJson));
            }

            root["floors"].push_back(std::move(floorJson));
        }

        outJson = root.dump(2);
        return true;
    } catch (const std::exception& e) {
        outError = e.what();
        return false;
    }
}

} // namespace Building
} // namespace Moon
