#include "BuildingGenerationInputs.h"
#include "../../external/nlohmann/json.hpp"

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
        input.floors.push_back(std::move(layoutFloor));
    }

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
            space.stairsConfig.width = rect.size[0];
            space.stairsConfig.position = rect.origin;
        }

        outSpaces.push_back(std::move(space));
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
                }
                floorJson["spaces"].push_back(std::move(spaceJson));
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
