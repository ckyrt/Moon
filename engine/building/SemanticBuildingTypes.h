#pragma once

#include "BuildingTypes.h"

#include <string>
#include <vector>

namespace Moon {
namespace Building {

enum class RelationshipType {
    Connected,
    Nearby,
    Separated,
    Visual
};

enum class ImportanceLevel {
    Required,
    Preferred,
    Optional
};

struct Adjacency {
    std::string to;
    std::string relationship;
    std::string importance;
};

struct SpaceConstraints {
    float aspectRatioMax = 0.0f;
    std::string naturalLight = "none";
    bool exteriorAccess = false;
    float ceilingHeight = 3.0f;
    float minWidth = 2.0f;
    int connectsToFloor = -1;
    int connectsFromFloor = -1;
};

struct SemanticSpace {
    std::string spaceId;
    std::string unitId;
    std::string type;
    std::string zone;
    float areaMin = 0.0f;
    float areaMax = 0.0f;
    float areaPreferred = 0.0f;
    std::string priority = "medium";
    std::vector<Adjacency> adjacency;
    SpaceConstraints constraints;
};

struct SemanticFloor {
    int level = 0;
    std::string name;
    std::vector<SemanticSpace> spaces;
};

struct SemanticVerticalAccess {
    int floor = 0;
    std::string toSpace;
};

struct SemanticVerticalSystem {
    std::string id;
    std::string type;
    std::string mode;
    std::string placement;
    int floorFrom = 0;
    int floorTo = 0;
    std::string stairForm;
    Rect shaftRect;
    std::vector<SemanticVerticalAccess> accessDoors;
};

struct MassConstraints {
    float footprintArea = 0.0f;
    int floors = 1;
    float totalHeight = 0.0f;
    std::string massingRuleAsset;
};

struct SemanticBuilding {
    std::string schema;
    float grid = 0.5f;
    std::string buildingType;
    BuildingStyle style;
    MassConstraints mass;
    std::vector<SemanticFloor> floors;
    std::vector<SemanticVerticalSystem> verticalSystems;
};

class SemanticBuildingParser {
public:
    static bool ParseFromFile(const std::string& filePath,
                              SemanticBuilding& building,
                              std::string& error);

    static bool ParseFromString(const std::string& jsonString,
                                SemanticBuilding& building,
                                std::string& error);
};

} // namespace Building
} // namespace Moon
