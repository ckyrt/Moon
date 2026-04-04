#include "SemanticBuildingValidator.h"

#include "BuildingTypology.h"
#include "BuildingTypes.h"

#include <sstream>
#include <unordered_map>
#include <vector>

namespace Moon {
namespace Building {

bool SemanticBuildingValidator::Validate(const nlohmann::json& json, std::string& outError) const {
    if (!ValidateSchema(json, outError)) return false;
    if (!ValidateGrid(json, outError)) return false;
    if (!ValidateBuildingType(json, outError)) return false;
    if (!ValidateStyle(json, outError)) return false;
    if (!ValidateMass(json, outError)) return false;
    if (!ValidateProgram(json, outError)) return false;
    if (!ValidateTypology(json, outError)) return false;
    return true;
}

bool SemanticBuildingValidator::ValidateSchema(const nlohmann::json& json, std::string& outError) const {
    if (!json.contains("schema")) {
        outError = "Missing required field: schema";
        return false;
    }

    const std::string schema = json["schema"].get<std::string>();
    if (schema != "moon_building") {
        outError = "Invalid schema. Expected 'moon_building', got '" + schema + "'";
        return false;
    }

    return true;
}

bool SemanticBuildingValidator::ValidateGrid(const nlohmann::json& json, std::string& outError) const {
    if (!json.contains("grid")) {
        outError = "Missing required field: grid";
        return false;
    }

    const float grid = json["grid"].get<float>();
    if (grid != GRID_SIZE) {
        std::ostringstream oss;
        oss << "Invalid grid size. Expected " << GRID_SIZE << ", got " << grid;
        outError = oss.str();
        return false;
    }

    return true;
}

bool SemanticBuildingValidator::ValidateBuildingType(const nlohmann::json& json, std::string& outError) const {
    if (!json.contains("building_type")) {
        outError = "Missing required field: building_type";
        return false;
    }

    const std::string buildingType = json["building_type"].get<std::string>();
    if (!IsSupportedBuildingType(buildingType)) {
        outError = "Unsupported building_type '" + buildingType +
            "'. Expected one of: villa, apartment, cbd_residential, office, office_tower, mall, shopping_center, retail_center";
        return false;
    }

    return true;
}

bool SemanticBuildingValidator::ValidateStyle(const nlohmann::json& json, std::string& outError) const {
    if (!json.contains("style")) {
        outError = "Missing required field: style";
        return false;
    }

    const auto& style = json["style"];
    if (!style.is_object()) {
        outError = "Field 'style' must be an object";
        return false;
    }

    const std::vector<std::string> requiredFields = {
        "category", "facade", "roof", "window_style", "material"
    };
    for (const auto& field : requiredFields) {
        if (!style.contains(field)) {
            outError = "Missing required style field: " + field;
            return false;
        }
    }

    return true;
}

bool SemanticBuildingValidator::ValidateMass(const nlohmann::json& json, std::string& outError) const {
    if (!json.contains("mass")) {
        outError = "Missing required field: mass";
        return false;
    }

    const auto& mass = json["mass"];
    if (!mass.is_object()) {
        outError = "Field 'mass' must be an object";
        return false;
    }

    if (!mass.contains("footprint_area")) {
        outError = "Missing required field: mass.footprint_area";
        return false;
    }

    if (!mass.contains("floors")) {
        outError = "Missing required field: mass.floors";
        return false;
    }

    if (mass["footprint_area"].get<float>() <= 0.0f) {
        outError = "mass.footprint_area must be > 0";
        return false;
    }

    if (mass["floors"].get<int>() <= 0) {
        outError = "mass.floors must be > 0";
        return false;
    }

    return true;
}

bool SemanticBuildingValidator::ValidateProgram(const nlohmann::json& json, std::string& outError) const {
    if (!json.contains("program")) {
        outError = "Missing required field: program";
        return false;
    }

    const auto& program = json["program"];
    if (!program.is_object()) {
        outError = "Field 'program' must be an object";
        return false;
    }

    if (!program.contains("floors")) {
        outError = "Missing required field: program.floors";
        return false;
    }

    return ValidateFloors(program["floors"], json["mass"]["floors"].get<int>(), outError);
}

bool SemanticBuildingValidator::ValidateFloors(const nlohmann::json& floorsJson,
                                               int massFloors,
                                               std::string& outError) const {
    if (!floorsJson.is_array()) {
        outError = "Field 'program.floors' must be an array";
        return false;
    }

    if (floorsJson.empty()) {
        outError = "At least one floor is required";
        return false;
    }

    std::unordered_set<std::string> knownSpaceIds;
    std::unordered_set<int> knownLevels;
    for (const auto& floorJson : floorsJson) {
        if (!ValidateFloor(floorJson, outError)) {
            return false;
        }

        const int level = floorJson["level"].get<int>();
        if (level < 0 || level >= massFloors) {
            outError = "Floor level " + std::to_string(level) +
                " is outside mass.floors range [0, " + std::to_string(massFloors - 1) + "]";
            return false;
        }

        if (!knownLevels.insert(level).second) {
            outError = "Duplicate floor level: " + std::to_string(level);
            return false;
        }

        for (const auto& spaceJson : floorJson["spaces"]) {
            if (!ValidateSpace(spaceJson, knownSpaceIds, outError)) {
                return false;
            }
        }
    }

    return true;
}

bool SemanticBuildingValidator::ValidateFloor(const nlohmann::json& floorJson, std::string& outError) const {
    if (!floorJson.is_object()) {
        outError = "Each floor must be an object";
        return false;
    }

    if (!floorJson.contains("level")) {
        outError = "Floor is missing required field: level";
        return false;
    }

    if (!floorJson.contains("spaces") || !floorJson["spaces"].is_array()) {
        outError = "Floor is missing required array field: spaces";
        return false;
    }

    return true;
}

bool SemanticBuildingValidator::ValidateSpace(const nlohmann::json& spaceJson,
                                              std::unordered_set<std::string>& knownSpaceIds,
                                              std::string& outError) const {
    if (!spaceJson.is_object()) {
        outError = "Each space must be an object";
        return false;
    }

    if (!spaceJson.contains("space_id")) {
        outError = "Space is missing required field: space_id";
        return false;
    }

    if (!spaceJson.contains("type")) {
        outError = "Space is missing required field: type";
        return false;
    }

    const std::string spaceId = spaceJson["space_id"].get<std::string>();
    if (spaceId.empty()) {
        outError = "space_id must not be empty";
        return false;
    }

    if (!knownSpaceIds.insert(spaceId).second) {
        outError = "Duplicate space_id: " + spaceId;
        return false;
    }

    if (spaceJson.contains("zone")) {
        const std::string zone = spaceJson["zone"].get<std::string>();
        if (zone != "public" && zone != "private" && zone != "service" && zone != "circulation") {
            outError = "Invalid zone '" + zone + "' for space '" + spaceId + "'";
            return false;
        }
    }

    if (spaceJson.contains("adjacency")) {
        if (!spaceJson["adjacency"].is_array()) {
            outError = "Space adjacency must be an array for space '" + spaceId + "'";
            return false;
        }
        for (const auto& adjacencyJson : spaceJson["adjacency"]) {
            if (!ValidateAdjacency(adjacencyJson, outError)) {
                return false;
            }
        }
    }

    if (spaceJson.contains("constraints")) {
        if (!spaceJson["constraints"].is_object()) {
            outError = "Space constraints must be an object for space '" + spaceId + "'";
            return false;
        }
        if (!ValidateConstraints(spaceJson["constraints"], outError)) {
            return false;
        }
    }

    return true;
}

bool SemanticBuildingValidator::ValidateAdjacency(const nlohmann::json& adjacencyJson, std::string& outError) const {
    if (!adjacencyJson.is_object()) {
        outError = "Each adjacency entry must be an object";
        return false;
    }

    if (!adjacencyJson.contains("to")) {
        outError = "Adjacency entry is missing required field: to";
        return false;
    }

    if (adjacencyJson.contains("relationship")) {
        const std::string relationship = adjacencyJson["relationship"].get<std::string>();
        if (relationship != "connected" && relationship != "nearby" &&
            relationship != "share_wall" && relationship != "around" &&
            relationship != "facing") {
            outError = "Unsupported adjacency relationship: " + relationship;
            return false;
        }
    }

    if (adjacencyJson.contains("importance")) {
        const std::string importance = adjacencyJson["importance"].get<std::string>();
        if (importance != "required" && importance != "preferred") {
            outError = "Unsupported adjacency importance: " + importance;
            return false;
        }
    }

    return true;
}

bool SemanticBuildingValidator::ValidateConstraints(const nlohmann::json& constraintsJson, std::string& outError) const {
    if (constraintsJson.contains("natural_light")) {
        const std::string naturalLight = constraintsJson["natural_light"].get<std::string>();
        if (naturalLight != "required" && naturalLight != "preferred" && naturalLight != "none") {
            outError = "Unsupported constraints.natural_light value: " + naturalLight;
            return false;
        }
    }

    if (constraintsJson.contains("min_width") && constraintsJson["min_width"].get<float>() <= 0.0f) {
        outError = "constraints.min_width must be > 0";
        return false;
    }

    if (constraintsJson.contains("aspect_ratio_max") && constraintsJson["aspect_ratio_max"].get<float>() < 1.0f) {
        outError = "constraints.aspect_ratio_max must be >= 1";
        return false;
    }

    return true;
}

bool SemanticBuildingValidator::ValidateTypology(const nlohmann::json& json, std::string& outError) const {
    const std::string buildingType = json["building_type"].get<std::string>();
    const BuildingTypology typology = ClassifyBuildingType(buildingType);
    const int massFloors = json["mass"]["floors"].get<int>();
    const auto& floorsJson = json["program"]["floors"];
    const auto& verticalSystemsJson = json.contains("vertical_systems") && json["vertical_systems"].is_array()
        ? json["vertical_systems"]
        : nlohmann::json::array();

    struct FloorStats {
        bool hasCore = false;
        bool hasCirculation = false;
        bool hasLobbyOrEntrance = false;
        bool hasVoid = false;
        bool hasOfficeProgram = false;
        bool hasResidentialUnit = false;
        bool hasRetailUnit = false;
        bool hasMeetingRoom = false;
        bool hasNonUnitPublicSpace = false;
        bool hasAroundVoidCorridor = false;
        bool shopConnectedToCirculation = true;
        int shopCount = 0;
    };

    std::unordered_map<int, FloorStats> perFloor;
    std::unordered_set<std::string> residentialUnits;
    std::unordered_set<std::string> retailUnits;
    int totalCoreCount = 0;
    int totalVoidCount = 0;
    int totalOfficeProgramCount = 0;

    for (const auto& floorJson : floorsJson) {
        const int level = floorJson["level"].get<int>();
        auto& stats = perFloor[level];

        std::unordered_set<std::string> voidIds;
        std::unordered_set<std::string> circulationIds;
        for (const auto& spaceJson : floorJson["spaces"]) {
            const std::string type = spaceJson["type"].get<std::string>();
            if (type == "void") {
                voidIds.insert(spaceJson["space_id"].get<std::string>());
            }
            if (type == "corridor" || type == "lobby" || type == "entrance") {
                circulationIds.insert(spaceJson["space_id"].get<std::string>());
            }
        }

        for (const auto& spaceJson : floorJson["spaces"]) {
            const std::string type = spaceJson["type"].get<std::string>();
            const std::string zone = spaceJson.value("zone", "");
            const std::string unitId = spaceJson.value("unit_id", "");

            if (type == "core" || type == "elevator" || type == "stairs") {
                stats.hasCore = true;
                ++totalCoreCount;
            }
            if (zone == "circulation" || type == "corridor" || type == "lobby" || type == "entrance") {
                stats.hasCirculation = true;
            }
            if (type == "lobby" || type == "entrance") {
                stats.hasLobbyOrEntrance = true;
            }
            if (type == "void") {
                stats.hasVoid = true;
                ++totalVoidCount;
            }
            if (type == "office" || type == "meeting_room") {
                stats.hasOfficeProgram = true;
                ++totalOfficeProgramCount;
            }
            if (type == "meeting_room") {
                stats.hasMeetingRoom = true;
            }
            if ((type == "living" || type == "bedroom") && !unitId.empty()) {
                stats.hasResidentialUnit = true;
                residentialUnits.insert(unitId);
            }
            if (type == "shop" && !unitId.empty()) {
                stats.hasRetailUnit = true;
                ++stats.shopCount;
                retailUnits.insert(unitId);
            }
            if (zone == "public" && unitId.empty() && type != "void") {
                stats.hasNonUnitPublicSpace = true;
            }

            bool shopHasCirculationAdjacency = false;
            if (spaceJson.contains("adjacency")) {
                for (const auto& adjacencyJson : spaceJson["adjacency"]) {
                    const std::string to = adjacencyJson["to"].get<std::string>();
                    const std::string relationship = adjacencyJson.value("relationship", "connected");
                    if ((type == "corridor" || type == "lobby") && relationship == "around" &&
                        voidIds.find(to) != voidIds.end()) {
                        stats.hasAroundVoidCorridor = true;
                    }
                    if (type == "shop" && circulationIds.find(to) != circulationIds.end() &&
                        (relationship == "connected" || relationship == "nearby")) {
                        shopHasCirculationAdjacency = true;
                    }
                }
            }

            if (type == "shop" && stats.shopConnectedToCirculation) {
                stats.shopConnectedToCirculation = shopHasCirculationAdjacency;
            }
        }
    }

    for (const auto& systemJson : verticalSystemsJson) {
        const std::string type = systemJson.value("type", "");
        const int floorFrom = systemJson.value("floor_from", 0);
        const int floorTo = std::max(floorFrom, systemJson.value("floor_to", floorFrom));
        for (int level = floorFrom; level <= floorTo; ++level) {
            auto& stats = perFloor[level];
            if (type == "stairwell" || type == "stair" || type == "elevator") {
                stats.hasCore = true;
            }
        }

        if (type == "stairwell" || type == "stair" || type == "elevator") {
            ++totalCoreCount;
        }
    }

    if (IsVillaTypology(typology)) {
        if (massFloors > 1 && totalCoreCount == 0) {
            outError = "Villa typology requires a vertical core/stair when mass.floors > 1";
            return false;
        }
        return true;
    }

    if (IsResidentialTypology(typology)) {
        if (massFloors < 2) {
            outError = buildingType + " typology requires at least 2 floors";
            return false;
        }
        if (totalCoreCount == 0) {
            outError = buildingType + " typology requires at least one core";
            return false;
        }
        if (residentialUnits.size() < 2) {
            outError = buildingType + " typology requires at least two residential unit_id groups";
            return false;
        }

        if (buildingType == "cbd_residential" && massFloors >= 4) {
            bool hasAmenityFloor = false;
            for (const auto& [level, stats] : perFloor) {
                if (level == 0) {
                    continue;
                }
                if (stats.hasCore && stats.hasCirculation && stats.hasNonUnitPublicSpace && !stats.hasResidentialUnit) {
                    hasAmenityFloor = true;
                    break;
                }
            }

            if (!hasAmenityFloor) {
                outError = "cbd_residential towers with 4+ floors require at least one non-ground amenity floor beside the main core";
                return false;
            }
        }

        for (const auto& [level, stats] : perFloor) {
            if (level == 0) {
                if (!stats.hasLobbyOrEntrance && !stats.hasCirculation) {
                    outError = buildingType + " ground floor requires a lobby, entrance, or circulation space";
                    return false;
                }
                continue;
            }

            if (stats.hasResidentialUnit && !stats.hasCirculation) {
                outError = buildingType + " residential floors require a corridor or other circulation space";
                return false;
            }
            if (stats.hasResidentialUnit && !stats.hasCore) {
                outError = buildingType + " residential floors require a core";
                return false;
            }
        }

        return true;
    }

    if (IsOfficeTypology(typology)) {
        if (buildingType == "office_tower" && massFloors < 3) {
            outError = "office_tower typology requires at least 3 floors";
            return false;
        }
        if (totalCoreCount == 0) {
            outError = buildingType + " typology requires a core on office floors";
            return false;
        }
        if (totalOfficeProgramCount == 0) {
            outError = buildingType + " typology requires office or meeting_room program spaces";
            return false;
        }
        auto groundFloor = perFloor.find(0);
        if (groundFloor == perFloor.end() || !groundFloor->second.hasLobbyOrEntrance) {
            outError = buildingType + " typology requires a ground-floor lobby or entrance";
            return false;
        }

        if (buildingType == "office_tower") {
            bool hasPodiumFloor = false;
            bool hasTowerOfficeFloor = false;
            for (const auto& [level, stats] : perFloor) {
                if (level <= 1 && (stats.hasLobbyOrEntrance || stats.hasMeetingRoom || stats.hasRetailUnit)) {
                    hasPodiumFloor = true;
                }
                if (level >= 2 && stats.hasOfficeProgram) {
                    hasTowerOfficeFloor = true;
                }
                if (level >= 2 && stats.hasRetailUnit) {
                    outError = "office_tower podium retail must stay on lower podium floors, not tower floors";
                    return false;
                }
            }

            if (!hasPodiumFloor) {
                outError = "office_tower typology requires a podium floor with lobby, meeting, or retail program on lower levels";
                return false;
            }
            if (!hasTowerOfficeFloor) {
                outError = "office_tower typology requires upper tower floors with office program";
                return false;
            }
        }

        for (const auto& [level, stats] : perFloor) {
            if (!stats.hasCore) {
                outError = buildingType + " floor " + std::to_string(level) + " requires a core";
                return false;
            }
        }
        return true;
    }

    if (IsRetailTypology(typology)) {
        if (totalVoidCount == 0) {
            outError = buildingType + " typology requires at least one void or atrium";
            return false;
        }
        if (retailUnits.size() < 2) {
            outError = buildingType + " typology requires at least two shop unit_id groups";
            return false;
        }
        auto groundFloor = perFloor.find(0);
        if (groundFloor == perFloor.end() || !groundFloor->second.hasCirculation) {
            outError = buildingType + " ground floor requires a circulation concourse or lobby";
            return false;
        }

        bool hasRingCorridorPattern = false;
        for (const auto& [level, stats] : perFloor) {
            if (stats.shopCount > 0) {
                if (!stats.hasVoid) {
                    outError = buildingType + " retail floors require a void or atrium on the same floor";
                    return false;
                }
                if (!stats.hasAroundVoidCorridor) {
                    outError = buildingType + " retail floors require a corridor that wraps or references the void with relationship 'around'";
                    return false;
                }
                if (!stats.shopConnectedToCirculation) {
                    outError = buildingType + " each shop must connect to a circulation space";
                    return false;
                }
                hasRingCorridorPattern = true;
            }
        }

        if (!hasRingCorridorPattern) {
            outError = buildingType + " typology requires at least one ring-corridor retail floor";
            return false;
        }
        return true;
    }

    return true;
}

} // namespace Building
} // namespace Moon
