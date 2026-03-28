#pragma once

#include "LayoutResolver.h"
#include <string>
#include <vector>

namespace Moon {
namespace Building {

struct BuildingFormInput {
    std::string buildingType;
    BuildingStyle style;
    MassConstraints mass;
};

struct FloorLayoutInput {
    int level = 0;
    std::string name;
    std::vector<SemanticSpace> spaces;
    std::vector<SemanticVerticalSystem> verticalSystems;
};

struct BuildingLayoutInput {
    std::vector<FloorLayoutInput> floors;
    std::vector<SemanticVerticalSystem> verticalSystems;

    const FloorLayoutInput* FindFloor(int level) const;
};

struct ResolvedSpacePlan {
    std::string spaceId;
    SpaceUsage usage = SpaceUsage::Unknown;
    Rect rect;
    float ceilingHeight = 0.0f;
    bool isOutdoor = false;
    bool hasStairs = false;
    int stairConnectToLevel = -1;
    StairType stairType = StairType::Straight;
    float stairWidth = 0.0f;
    GridPos2D stairPosition = {0.0f, 0.0f};
    float stairRotationDegrees = 0.0f;
};

struct ResolvedVerticalTransportPlan {
    std::string transportId;
    VerticalTransportType type = VerticalTransportType::Stair;
    Rect shaftRect;
    int floorFrom = 0;
    int floorTo = 0;
    int sourceFloorLevel = 0;
    bool continuousShaft = true;
    bool enclosed = true;
    bool external = false;
    StairType stairType = StairType::Straight;
    float width = 0.0f;
    GridPos2D position = {0.0f, 0.0f};
    float rotationDegrees = 0.0f;
};

struct ResolvedFloorLayout {
    int level = 0;
    std::vector<ResolvedSpacePlan> spaces;
    std::vector<ResolvedVerticalTransportPlan> verticalTransports;
    std::vector<ProgramBlock> debugBlocks;
};

struct ResolvedBuildingLayout {
    std::string buildingType;
    std::vector<ResolvedFloorLayout> floors;
};

BuildingFormInput ExtractBuildingFormInput(const SemanticBuilding& building);
BuildingLayoutInput ExtractBuildingLayoutInput(const SemanticBuilding& building);
void BuildFloorSpacesFromResolvedLayout(const ResolvedFloorLayout& resolvedFloor,
                                        int& ioNextSpaceId,
                                        std::vector<Space>& outSpaces);
void BuildVerticalTransportsFromResolvedLayout(const ResolvedBuildingLayout& resolvedLayout,
                                               std::vector<VerticalTransport>& outVerticalTransports);
bool SerializeResolvedBuildingLayout(const BuildingFormInput* formInput,
                                     const ResolvedBuildingLayout& resolvedLayout,
                                     std::string& outJson,
                                     std::string& outError);

} // namespace Building
} // namespace Moon
