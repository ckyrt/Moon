#include "SchemaValidator.h"
#include "LayoutResolver.h"
#include "SpaceGraphBuilder.h"
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

bool IsResidentialType(const std::string& buildingType);
bool IsOfficeType(const std::string& buildingType);
bool IsRetailType(const std::string& buildingType);
bool IsSupportedBuildingType(const std::string& buildingType);

constexpr float kDefaultGridSize = Moon::Building::GRID_SIZE;

void AddRepairNote(Moon::Building::BestEffortGenerationReport& report, const std::string& note) {
    report.usedBestEffort = true;
    report.repairNotes.push_back(note);
}

void AddAdjustedSpace(Moon::Building::BestEffortGenerationReport& report,
                      int floorLevel,
                      const std::string& spaceId,
                      const std::string& spaceType,
                      const std::string& reason) {
    Moon::Building::BestEffortAdjustedSpace issue;
    issue.floorLevel = floorLevel;
    issue.spaceId = spaceId;
    issue.spaceType = spaceType;
    issue.reason = reason;
    report.usedBestEffort = true;
    report.adjustedSpaces.push_back(issue);
}

std::string DefaultSpaceTypeForBuilding(const std::string& buildingType) {
    if (IsRetailType(buildingType)) {
        return "shop";
    }
    if (IsOfficeType(buildingType)) {
        return "office";
    }
    if (IsResidentialType(buildingType)) {
        return "bedroom";
    }
    return "living";
}

std::string InferZoneForType(const std::string& spaceType) {
    if (spaceType == "corridor" || spaceType == "lobby" || spaceType == "entrance" ||
        spaceType == "stairs") {
        return "circulation";
    }
    if (spaceType == "core" || spaceType == "storage" || spaceType == "mechanical" ||
        spaceType == "elevator") {
        return "service";
    }
    if (spaceType == "bedroom" || spaceType == "bathroom") {
        return "private";
    }
    return "public";
}

float DefaultAreaForType(const std::string& spaceType) {
    if (spaceType == "core") return 20.0f;
    if (spaceType == "stairs") return 16.0f;
    if (spaceType == "corridor") return 18.0f;
    if (spaceType == "lobby" || spaceType == "entrance") return 12.0f;
    if (spaceType == "bathroom") return 6.0f;
    if (spaceType == "kitchen") return 12.0f;
    if (spaceType == "storage") return 8.0f;
    if (spaceType == "shop") return 24.0f;
    if (spaceType == "office") return 20.0f;
    return 16.0f;
}

void EnsureStyleField(nlohmann::json& style,
                      const char* field,
                      const char* fallback,
                      Moon::Building::BestEffortGenerationReport& report) {
    if (!style.contains(field) || !style[field].is_string() || style[field].get<std::string>().empty()) {
        style[field] = fallback;
        AddRepairNote(report, std::string("Filled missing style.") + field + " with default '" + fallback + "'");
    }
}

void RepairSemanticJsonForBestEffort(nlohmann::json& json,
                                     Moon::Building::BestEffortGenerationReport& report) {
    if (!json.is_object()) {
        json = nlohmann::json::object();
        AddRepairNote(report, "Replaced non-object root JSON with an empty object for recovery");
    }

    if (!json.contains("schema") || !json["schema"].is_string() ||
        json["schema"].get<std::string>() != "moon_building") {
        json["schema"] = "moon_building";
        AddRepairNote(report, "Normalized schema to 'moon_building'");
    }

    if (!json.contains("grid") || !json["grid"].is_number() ||
        std::abs(json["grid"].get<float>() - kDefaultGridSize) > 0.001f) {
        json["grid"] = kDefaultGridSize;
        AddRepairNote(report, "Normalized grid to 0.5m");
    }

    std::string buildingType = json.value("building_type", "");
    if (!IsSupportedBuildingType(buildingType)) {
        buildingType = "villa";
        json["building_type"] = buildingType;
        AddRepairNote(report, "Replaced unsupported or missing building_type with 'villa'");
    }

    if (!json.contains("style") || !json["style"].is_object()) {
        json["style"] = nlohmann::json::object();
        AddRepairNote(report, "Created missing style object");
    }
    auto& style = json["style"];
    EnsureStyleField(style, "category", "minimal", report);
    EnsureStyleField(style, "facade", "plaster", report);
    EnsureStyleField(style, "roof", "flat", report);
    EnsureStyleField(style, "window_style", "standard", report);
    EnsureStyleField(style, "material", "plaster", report);

    if (!json.contains("program") || !json["program"].is_object()) {
        json["program"] = nlohmann::json::object();
        AddRepairNote(report, "Created missing program object");
    }
    auto& program = json["program"];
    if (!program.contains("floors") || !program["floors"].is_array()) {
        program["floors"] = nlohmann::json::array();
        AddRepairNote(report, "Created missing program.floors array");
    }

    if (!json.contains("mass") || !json["mass"].is_object()) {
        json["mass"] = nlohmann::json::object();
        AddRepairNote(report, "Created missing mass object");
    }
    auto& mass = json["mass"];

    int inferredFloorCount = static_cast<int>(program["floors"].size());
    if (inferredFloorCount <= 0) {
        inferredFloorCount = 1;
    }
    if (!mass.contains("floors") || !mass["floors"].is_number_integer() || mass["floors"].get<int>() < 1) {
        mass["floors"] = inferredFloorCount;
        AddRepairNote(report, "Repaired mass.floors from program floor count");
    }

    if (program["floors"].empty()) {
        for (int level = 0; level < mass["floors"].get<int>(); ++level) {
            program["floors"].push_back({
                {"level", level},
                {"name", std::string("recovered_floor_") + std::to_string(level)},
                {"spaces", nlohmann::json::array()}
            });
        }
        AddRepairNote(report, "Created fallback floors because program.floors was empty");
    }

    std::unordered_set<std::string> knownSpaceIds;
    int generatedSpaceIndex = 0;
    int totalSpaces = 0;
    int floorIndex = 0;
    const int maxFloorLevel = std::max(0, mass["floors"].get<int>() - 1);
    for (auto& floor : program["floors"]) {
        if (!floor.is_object()) {
            floor = nlohmann::json::object();
            AddRepairNote(report, "Replaced non-object floor entry during recovery");
        }

        const int originalLevel = floor.value("level", floorIndex);
        if (!floor.contains("level") || !floor["level"].is_number_integer() || originalLevel != floorIndex) {
            floor["level"] = floorIndex;
            AddRepairNote(report, "Renumbered floor levels to a valid sequential range");
        }
        if (!floor.contains("name") || !floor["name"].is_string()) {
            floor["name"] = std::string("recovered_floor_") + std::to_string(floorIndex);
        }
        if (!floor.contains("spaces") || !floor["spaces"].is_array()) {
            floor["spaces"] = nlohmann::json::array();
            AddRepairNote(report, "Created missing spaces array on a floor");
        }

        for (auto& space : floor["spaces"]) {
            if (!space.is_object()) {
                space = nlohmann::json::object();
                AddRepairNote(report, "Replaced non-object space entry during recovery");
            }

            std::string spaceId = space.value("space_id", "");
            if (spaceId.empty() || knownSpaceIds.find(spaceId) != knownSpaceIds.end()) {
                spaceId = std::string("recovered_space_") + std::to_string(generatedSpaceIndex++);
                space["space_id"] = spaceId;
                AddAdjustedSpace(report, floorIndex, spaceId, "unknown", "assigned a unique recovered space_id");
            }
            knownSpaceIds.insert(spaceId);

            std::string spaceType = space.value("type", "");
            if (spaceType.empty()) {
                spaceType = DefaultSpaceTypeForBuilding(buildingType);
                space["type"] = spaceType;
                AddAdjustedSpace(report, floorIndex, spaceId, spaceType, "assigned default space type");
            }

            const std::string zone = space.value("zone", "");
            if (zone != "public" && zone != "private" && zone != "service" && zone != "circulation") {
                const std::string repairedZone = InferZoneForType(spaceType);
                space["zone"] = repairedZone;
                AddAdjustedSpace(report, floorIndex, spaceId, spaceType, "repaired invalid zone to '" + repairedZone + "'");
            }

            float areaPreferred = space.value("area_preferred", 0.0f);
            if (areaPreferred <= 0.0f) {
                areaPreferred = DefaultAreaForType(spaceType);
                space["area_preferred"] = areaPreferred;
                AddAdjustedSpace(report, floorIndex, spaceId, spaceType, "repaired missing area_preferred");
            }

            if (space.contains("area_min") && space["area_min"].is_number()) {
                if (space["area_min"].get<float>() > areaPreferred) {
                    space["area_min"] = areaPreferred;
                    AddAdjustedSpace(report, floorIndex, spaceId, spaceType, "clamped area_min to area_preferred");
                }
            }
            if (space.contains("area_max") && space["area_max"].is_number()) {
                if (space["area_max"].get<float>() > 0.0f && space["area_max"].get<float>() < areaPreferred) {
                    space["area_max"] = areaPreferred;
                    AddAdjustedSpace(report, floorIndex, spaceId, spaceType, "raised area_max to area_preferred");
                }
            }

            if (space.contains("constraints") && space["constraints"].is_object()) {
                auto& constraints = space["constraints"];
                if (constraints.contains("min_width") &&
                    (!constraints["min_width"].is_number() || constraints["min_width"].get<float>() <= 0.0f)) {
                    constraints["min_width"] = 2.0f;
                    AddAdjustedSpace(report, floorIndex, spaceId, spaceType, "repaired invalid constraints.min_width");
                }
                if (constraints.contains("aspect_ratio_max") &&
                    (!constraints["aspect_ratio_max"].is_number() || constraints["aspect_ratio_max"].get<float>() < 1.0f)) {
                    constraints["aspect_ratio_max"] = 3.0f;
                    AddAdjustedSpace(report, floorIndex, spaceId, spaceType, "repaired invalid constraints.aspect_ratio_max");
                }
                if (constraints.contains("natural_light")) {
                    const std::string naturalLight = constraints.value("natural_light", "none");
                    if (naturalLight != "required" && naturalLight != "preferred" && naturalLight != "none") {
                        constraints["natural_light"] = "none";
                        AddAdjustedSpace(report, floorIndex, spaceId, spaceType, "repaired invalid constraints.natural_light");
                    }
                }
                if (constraints.contains("ceiling_height") &&
                    (!constraints["ceiling_height"].is_number() || constraints["ceiling_height"].get<float>() <= 0.0f)) {
                    constraints["ceiling_height"] = 3.0f;
                    AddAdjustedSpace(report, floorIndex, spaceId, spaceType, "repaired invalid constraints.ceiling_height");
                }

                if (constraints.contains("connects_to_floor") && constraints["connects_to_floor"].is_number_integer()) {
                    const int target = constraints["connects_to_floor"].get<int>();
                    if (target < 0 || target > maxFloorLevel) {
                        constraints["connects_to_floor"] = std::min(std::max(floorIndex, 0), maxFloorLevel);
                        AddAdjustedSpace(report, floorIndex, spaceId, spaceType, "clamped connects_to_floor to valid range");
                    }
                }
                if (constraints.contains("connects_from_floor") && constraints["connects_from_floor"].is_number_integer()) {
                    const int source = constraints["connects_from_floor"].get<int>();
                    if (source < 0 || source > maxFloorLevel) {
                        constraints["connects_from_floor"] = std::min(std::max(floorIndex - 1, 0), maxFloorLevel);
                        AddAdjustedSpace(report, floorIndex, spaceId, spaceType, "clamped connects_from_floor to valid range");
                    }
                }
            }

            ++totalSpaces;
        }

        ++floorIndex;
    }

    for (auto& floor : program["floors"]) {
        const int level = floor["level"].get<int>();
        for (auto& space : floor["spaces"]) {
            const std::string spaceId = space.value("space_id", "");
            const std::string spaceType = space.value("type", DefaultSpaceTypeForBuilding(buildingType));

            nlohmann::json repairedAdjacency = nlohmann::json::array();
            if (space.contains("adjacency") && space["adjacency"].is_array()) {
                for (const auto& adjacency : space["adjacency"]) {
                    if (!adjacency.is_object() || !adjacency.contains("to") || !adjacency["to"].is_string()) {
                        AddAdjustedSpace(report, level, spaceId, spaceType, "removed malformed adjacency entry");
                        continue;
                    }
                    const std::string to = adjacency["to"].get<std::string>();
                    if (to == spaceId || knownSpaceIds.find(to) == knownSpaceIds.end()) {
                        AddAdjustedSpace(report, level, spaceId, spaceType, "removed adjacency to unknown space '" + to + "'");
                        continue;
                    }

                    nlohmann::json repaired = adjacency;
                    const std::string relationship = adjacency.value("relationship", "connected");
                    if (relationship != "connected" && relationship != "nearby" &&
                        relationship != "share_wall" && relationship != "around" &&
                        relationship != "facing") {
                        repaired["relationship"] = "connected";
                        AddAdjustedSpace(report, level, spaceId, spaceType, "normalized unsupported adjacency relationship");
                    }
                    const std::string importance = adjacency.value("importance", "preferred");
                    if (importance != "required" && importance != "preferred") {
                        repaired["importance"] = "preferred";
                        AddAdjustedSpace(report, level, spaceId, spaceType, "normalized unsupported adjacency importance");
                    }
                    repairedAdjacency.push_back(repaired);
                }
            }
            if (!repairedAdjacency.empty()) {
                space["adjacency"] = repairedAdjacency;
            } else if (space.contains("adjacency")) {
                space["adjacency"] = nlohmann::json::array();
            }
        }
    }

    if (totalSpaces == 0) {
        const std::string fallbackType = DefaultSpaceTypeForBuilding(buildingType);
        program["floors"][0]["spaces"] = nlohmann::json::array({
            {
                {"space_id", "recovered_space_0"},
                {"type", fallbackType},
                {"zone", InferZoneForType(fallbackType)},
                {"area_preferred", DefaultAreaForType(fallbackType)},
                {"constraints", {
                    {"min_width", 2.0f},
                    {"ceiling_height", 3.0f},
                    {"natural_light", "none"},
                    {"aspect_ratio_max", 3.0f}
                }}
            }
        });
        AddRepairNote(report, "Inserted a fallback recoverable space because no valid spaces remained");
        totalSpaces = 1;
    }

    if (!mass.contains("footprint_area") || !mass["footprint_area"].is_number() || mass["footprint_area"].get<float>() <= 0.0f) {
        float estimatedArea = 0.0f;
        for (const auto& floor : program["floors"]) {
            for (const auto& space : floor["spaces"]) {
                const float repairedArea = space.value("area_preferred", 0.0f);
                estimatedArea += std::max(repairedArea, 4.0f);
            }
        }
        const float floorCount = std::max(1.0f, static_cast<float>(mass["floors"].get<int>()));
        mass["footprint_area"] = std::max(36.0f, estimatedArea / floorCount * 1.15f);
        AddRepairNote(report, "Estimated missing mass.footprint_area from repaired program areas");
    }

    if (!mass.contains("total_height") || !mass["total_height"].is_number() || mass["total_height"].get<float>() <= 0.0f) {
        mass["total_height"] = std::max(3.0f, 3.0f * static_cast<float>(mass["floors"].get<int>()));
        AddRepairNote(report, "Estimated missing mass.total_height from floor count");
    }
}

bool IsResidentialType(const std::string& buildingType) {
    return buildingType == "apartment" || buildingType == "cbd_residential";
}

bool IsOfficeType(const std::string& buildingType) {
    return buildingType == "office" || buildingType == "office_tower";
}

bool IsRetailType(const std::string& buildingType) {
    return buildingType == "mall" || buildingType == "shopping_center" ||
           buildingType == "retail_center";
}

bool IsSupportedBuildingType(const std::string& buildingType) {
    return buildingType == "villa" || IsResidentialType(buildingType) ||
           IsOfficeType(buildingType) || IsRetailType(buildingType);
}

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

SchemaValidator::SchemaValidator() {}

SchemaValidator::~SchemaValidator() {}

bool SchemaValidator::ValidateAndParse(const std::string& jsonStr,
                                       BuildingDefinition& outDefinition,
                                       std::string& outError) {
    try {
        nlohmann::json json = nlohmann::json::parse(jsonStr);
        return ValidateAndParse(json, outDefinition, outError);
    }
    catch (const nlohmann::json::parse_error& e) {
        outError = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

bool SchemaValidator::ValidateAndParseBestEffort(const std::string& jsonStr,
                                                 BuildingDefinition& outDefinition,
                                                 BestEffortGenerationReport& outReport,
                                                 std::string& outError) {
    try {
        nlohmann::json json = nlohmann::json::parse(jsonStr);
        return ValidateAndParseBestEffort(json, outDefinition, outReport, outError);
    }
    catch (const nlohmann::json::parse_error& e) {
        outError = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

bool SchemaValidator::ValidateAndParse(const nlohmann::json& json,
                                       BuildingDefinition& outDefinition,
                                       std::string& outError) {
    if (!ValidateSemanticJson(json, outError)) return false;

    SemanticBuilding semanticBuilding;
    if (!SemanticBuildingParser::ParseFromString(json.dump(), semanticBuilding, outError)) {
        return false;
    }

    LayoutResolver resolver;
    resolver.SetGridSize(semanticBuilding.grid);
    if (!resolver.Resolve(semanticBuilding, outDefinition, outError)) {
        return false;
    }

    return ValidateResolvedSemanticConsistency(semanticBuilding, outDefinition, outError);
}

bool SchemaValidator::ValidateAndParseBestEffort(const nlohmann::json& json,
                                                 BuildingDefinition& outDefinition,
                                                 BestEffortGenerationReport& outReport,
                                                 std::string& outError) {
    outReport.usedBestEffort = false;
    outReport.repairNotes.clear();

    nlohmann::json repairedJson = json;
    RepairSemanticJsonForBestEffort(repairedJson, outReport);

    SemanticBuilding semanticBuilding;
    if (!SemanticBuildingParser::ParseFromString(repairedJson.dump(), semanticBuilding, outError)) {
        return false;
    }

    LayoutResolver resolver;
    resolver.SetGridSize(semanticBuilding.grid);
    if (!resolver.ResolveBestEffort(semanticBuilding, outDefinition, outReport, outError)) {
        return false;
    }

    if (!outReport.usedBestEffort) {
        return ValidateResolvedSemanticConsistency(semanticBuilding, outDefinition, outError);
    }

    return true;
}

bool SchemaValidator::ValidateSemanticJson(const nlohmann::json& json, std::string& outError) {
    if (!ValidateSchema(json, outError)) return false;
    if (!ValidateGrid(json, outError)) return false;
    if (!ValidateBuildingType(json, outError)) return false;
    if (!ValidateStyle(json, outError)) return false;
    if (!ValidateMass(json, outError)) return false;
    if (!ValidateProgram(json, outError)) return false;
    if (!ValidateTypology(json, outError)) return false;
    return true;
}

bool SchemaValidator::ValidateSchema(const nlohmann::json& json, std::string& outError) {
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

bool SchemaValidator::ValidateGrid(const nlohmann::json& json, std::string& outError) {
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

bool SchemaValidator::ValidateBuildingType(const nlohmann::json& json, std::string& outError) {
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

bool SchemaValidator::ValidateStyle(const nlohmann::json& json, std::string& outError) {
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

bool SchemaValidator::ValidateMass(const nlohmann::json& json, std::string& outError) {
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
        outError = "Missing required mass field: footprint_area";
        return false;
    }

    if (!mass.contains("floors")) {
        outError = "Missing required mass field: floors";
        return false;
    }

    if (mass["footprint_area"].get<float>() <= 0.0f) {
        outError = "mass.footprint_area must be > 0";
        return false;
    }

    if (mass["floors"].get<int>() < 1) {
        outError = "mass.floors must be >= 1";
        return false;
    }

    return true;
}

bool SchemaValidator::ValidateProgram(const nlohmann::json& json, std::string& outError) {
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

bool SchemaValidator::ValidateFloors(const nlohmann::json& floorsJson,
                                     int massFloors,
                                     std::string& outError) {
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

bool SchemaValidator::ValidateFloor(const nlohmann::json& floorJson, std::string& outError) {
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

bool SchemaValidator::ValidateSpace(const nlohmann::json& spaceJson,
                                    std::unordered_set<std::string>& knownSpaceIds,
                                    std::string& outError) {
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

bool SchemaValidator::ValidateAdjacency(const nlohmann::json& adjacencyJson, std::string& outError) {
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

bool SchemaValidator::ValidateConstraints(const nlohmann::json& constraintsJson, std::string& outError) {
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

bool SchemaValidator::ValidateTypology(const nlohmann::json& json, std::string& outError) {
    const std::string buildingType = json["building_type"].get<std::string>();
    const int massFloors = json["mass"]["floors"].get<int>();
    const auto& floorsJson = json["program"]["floors"];

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

    if (buildingType == "villa") {
        if (massFloors > 1 && totalCoreCount == 0) {
            outError = "Villa typology requires a vertical core/stair when mass.floors > 1";
            return false;
        }
        return true;
    }

    if (IsResidentialType(buildingType)) {
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

    if (IsOfficeType(buildingType)) {
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

    if (IsRetailType(buildingType)) {
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

bool SchemaValidator::ValidateResolvedSemanticConsistency(const SemanticBuilding& semanticBuilding,
                                                         const BuildingDefinition& resolvedBuilding,
                                                         std::string& outError) {
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
