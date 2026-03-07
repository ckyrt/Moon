#include "SchemaValidator.h"
#include <cmath>
#include <sstream>

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

bool SchemaValidator::ValidateAndParse(const nlohmann::json& json,
                                       BuildingDefinition& outDefinition,
                                       std::string& outError) {
    // Validate schema version
    if (!ValidateSchema(json, outError)) return false;
    
    // Validate grid
    if (!ValidateGrid(json, outError)) return false;
    
    // Validate style
    if (!ValidateStyle(json, outError)) return false;
    
    // Validate masses
    if (!ValidateMasses(json, outError)) return false;
    
    // Validate floors
    if (!ValidateFloors(json, outError)) return false;
    
    // Parse all fields
    outDefinition.schema = json["schema"].get<std::string>();
    outDefinition.grid = json["grid"].get<float>();
    
    if (!ParseStyle(json["style"], outDefinition.style, outError)) return false;
    
    // Parse masses
    outDefinition.masses.clear();
    for (const auto& massJson : json["masses"]) {
        Mass mass;
        if (!ParseMass(massJson, mass, outError)) return false;
        outDefinition.masses.push_back(mass);
    }
    
    // Parse floors
    outDefinition.floors.clear();
    for (const auto& floorJson : json["floors"]) {
        Floor floor;
        if (!ParseFloor(floorJson, floor, outError)) return false;
        outDefinition.floors.push_back(floor);
    }
    
    return true;
}

bool SchemaValidator::ValidateSchema(const nlohmann::json& json, std::string& outError) {
    if (!json.contains("schema")) {
        outError = "Missing required field: schema";
        return false;
    }
    
    std::string schema = json["schema"].get<std::string>();
    if (schema != "moon_building_v8") {
        outError = "Invalid schema version. Expected 'moon_building_v8', got '" + schema + "'";
        return false;
    }
    
    return true;
}

bool SchemaValidator::ValidateGrid(const nlohmann::json& json, std::string& outError) {
    if (!json.contains("grid")) {
        outError = "Missing required field: grid";
        return false;
    }
    
    float grid = json["grid"].get<float>();
    if (grid != GRID_SIZE) {
        std::ostringstream oss;
        oss << "Invalid grid size. Expected " << GRID_SIZE << ", got " << grid;
        outError = oss.str();
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
    
    // Check required style fields
    std::vector<std::string> requiredFields = {"category", "facade", "roof", "window_style", "material"};
    for (const auto& field : requiredFields) {
        if (!style.contains(field)) {
            outError = "Missing required style field: " + field;
            return false;
        }
    }
    
    return true;
}

bool SchemaValidator::ValidateMasses(const nlohmann::json& json, std::string& outError) {
    if (!json.contains("masses")) {
        outError = "Missing required field: masses";
        return false;
    }
    
    const auto& masses = json["masses"];
    if (!masses.is_array()) {
        outError = "Field 'masses' must be an array";
        return false;
    }
    
    if (masses.empty()) {
        outError = "At least one mass is required";
        return false;
    }
    
    return true;
}

bool SchemaValidator::ValidateFloors(const nlohmann::json& json, std::string& outError) {
    if (!json.contains("floors")) {
        outError = "Missing required field: floors";
        return false;
    }
    
    const auto& floors = json["floors"];
    if (!floors.is_array()) {
        outError = "Field 'floors' must be an array";
        return false;
    }
    
    if (floors.empty()) {
        outError = "At least one floor is required";
        return false;
    }
    
    return true;
}

bool SchemaValidator::ParseStyle(const nlohmann::json& json, BuildingStyle& style, std::string& outError) {
    try {
        style.category = json["category"].get<std::string>();
        style.facade = json["facade"].get<std::string>();
        style.roof = json["roof"].get<std::string>();
        style.windowStyle = json["window_style"].get<std::string>();
        style.material = json["material"].get<std::string>();
        return true;
    }
    catch (const std::exception& e) {
        outError = std::string("Error parsing style: ") + e.what();
        return false;
    }
}

bool SchemaValidator::ParseMass(const nlohmann::json& json, Mass& mass, std::string& outError) {
    try {
        // Required fields
        if (!json.contains("mass_id") || !json.contains("origin") || 
            !json.contains("size") || !json.contains("floors")) {
            outError = "Mass is missing required fields (mass_id, origin, size, floors)";
            return false;
        }
        
        mass.massId = json["mass_id"].get<std::string>();
        
        // Parse origin
        const auto& originJson = json["origin"];
        if (!originJson.is_array() || originJson.size() != 2) {
            outError = "Mass origin must be an array of 2 numbers [x, y]";
            return false;
        }
        mass.origin[0] = originJson[0].get<float>();
        mass.origin[1] = originJson[1].get<float>();
        
        if (!ValidateGridPos2D(mass.origin, outError)) {
            return false;
        }
        
        // Parse size
        const auto& sizeJson = json["size"];
        if (!sizeJson.is_array() || sizeJson.size() != 2) {
            outError = "Mass size must be an array of 2 numbers [width, depth]";
            return false;
        }
        mass.size[0] = sizeJson[0].get<float>();
        mass.size[1] = sizeJson[1].get<float>();
        
        if (!ValidateGridSize2D(mass.size, outError)) {
            return false;
        }
        
        mass.floors = json["floors"].get<int>();
        if (mass.floors < 1) {
            outError = "Mass must have at least 1 floor";
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        outError = std::string("Error parsing mass: ") + e.what();
        return false;
    }
}

bool SchemaValidator::ParseFloor(const nlohmann::json& json, Floor& floor, std::string& outError) {
    try {
        if (!json.contains("level") || !json.contains("mass_id") || 
            !json.contains("floor_height") || !json.contains("spaces")) {
            outError = "Floor is missing required fields (level, mass_id, floor_height, spaces)";
            return false;
        }
        
        floor.level = json["level"].get<int>();
        floor.massId = json["mass_id"].get<std::string>();
        floor.floorHeight = json["floor_height"].get<float>();
        
        if (floor.floorHeight <= 0) {
            outError = "Floor height must be positive";
            return false;
        }
        
        // Parse spaces
        const auto& spacesJson = json["spaces"];
        if (!spacesJson.is_array()) {
            outError = "Floor spaces must be an array";
            return false;
        }
        
        floor.spaces.clear();
        for (const auto& spaceJson : spacesJson) {
            Space space;
            if (!ParseSpace(spaceJson, space, outError)) {
                return false;
            }
            floor.spaces.push_back(space);
        }
        
        return true;
    }
    catch (const std::exception& e) {
        outError = std::string("Error parsing floor: ") + e.what();
        return false;
    }
}

bool SchemaValidator::ParseSpace(const nlohmann::json& json, Space& space, std::string& outError) {
    try {
        if (!json.contains("space_id") || !json.contains("rects") || 
            !json.contains("properties")) {
            outError = "Space is missing required fields (space_id, rects, properties)";
            return false;
        }
        
        space.spaceId = json["space_id"].get<int>();
        
        // Parse rects
        const auto& rectsJson = json["rects"];
        if (!rectsJson.is_array() || rectsJson.empty()) {
            outError = "Space must have at least one rect";
            return false;
        }
        
        space.rects.clear();
        for (const auto& rectJson : rectsJson) {
            Rect rect;
            if (!ParseRect(rectJson, rect, outError)) {
                return false;
            }
            space.rects.push_back(rect);
        }
        
        // Parse properties
        const auto& props = json["properties"];
        std::string usageStr = props.value("usage_hint", "");
        space.properties.usage = StringToSpaceUsage(usageStr);
        space.properties.isOutdoor = props.value("is_outdoor", false);
        space.properties.hasStairs = props.value("stairs", false);
        space.properties.ceilingHeight = props.value("ceiling_height", 3.0f);
        
        // Parse anchors (optional)
        if (json.contains("anchors") && json["anchors"].is_array()) {
            space.anchors.clear();
            for (const auto& anchorJson : json["anchors"]) {
                Anchor anchor;
                if (!ParseAnchor(anchorJson, anchor, outError)) {
                    return false;
                }
                space.anchors.push_back(anchor);
            }
        }
        
        // Parse stairs (optional)
        if (json.contains("stairs") && !json["stairs"].is_null()) {
            if (!ParseStairs(json["stairs"], space.stairsConfig, outError)) {
                return false;
            }
            space.hasStairs = true;
        } else {
            space.hasStairs = false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        outError = std::string("Error parsing space: ") + e.what();
        return false;
    }
}

bool SchemaValidator::ParseRect(const nlohmann::json& json, Rect& rect, std::string& outError) {
    try {
        if (!json.contains("rect_id") || !json.contains("origin") || !json.contains("size")) {
            outError = "Rect is missing required fields (rect_id, origin, size)";
            return false;
        }
        
        rect.rectId = json["rect_id"].get<std::string>();
        
        // Parse origin
        const auto& originJson = json["origin"];
        if (!originJson.is_array() || originJson.size() != 2) {
            outError = "Rect origin must be an array of 2 numbers [x, y]";
            return false;
        }
        rect.origin[0] = originJson[0].get<float>();
        rect.origin[1] = originJson[1].get<float>();
        
        if (!ValidateGridPos2D(rect.origin, outError)) {
            return false;
        }
        
        // Parse size
        const auto& sizeJson = json["size"];
        if (!sizeJson.is_array() || sizeJson.size() != 2) {
            outError = "Rect size must be an array of 2 numbers [width, depth]";
            return false;
        }
        rect.size[0] = sizeJson[0].get<float>();
        rect.size[1] = sizeJson[1].get<float>();
        
        if (!ValidateGridSize2D(rect.size, outError)) {
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        outError = std::string("Error parsing rect: ") + e.what();
        return false;
    }
}

bool SchemaValidator::ParseAnchor(const nlohmann::json& json, Anchor& anchor, std::string& outError) {
    try {
        if (!json.contains("name") || !json.contains("pos") || !json.contains("type")) {
            outError = "Anchor is missing required fields (name, pos, type)";
            return false;
        }
        
        anchor.name = json["name"].get<std::string>();
        
        // Parse position
        const auto& posJson = json["pos"];
        if (!posJson.is_array() || posJson.size() != 2) {
            outError = "Anchor pos must be an array of 2 numbers [x, y]";
            return false;
        }
        anchor.position[0] = posJson[0].get<float>();
        anchor.position[1] = posJson[1].get<float>();
        
        if (!ValidateGridPos2D(anchor.position, outError)) {
            return false;
        }
        
        // Parse type
        std::string typeStr = json["type"].get<std::string>();
        if (typeStr == "furniture_zone") {
            anchor.type = AnchorType::FurnitureZone;
        } else if (typeStr == "sofa_zone") {
            anchor.type = AnchorType::SofaZone;
        } else if (typeStr == "bed_zone") {
            anchor.type = AnchorType::BedZone;
        } else if (typeStr == "door_hint") {
            anchor.type = AnchorType::DoorHint;
        } else if (typeStr == "stair_hint") {
            anchor.type = AnchorType::StairHint;
        } else if (typeStr == "window_hint") {
            anchor.type = AnchorType::WindowHint;
        } else {
            outError = "Unknown anchor type: " + typeStr;
            return false;
        }
        
        // Optional fields
        if (json.contains("rotation")) {
            anchor.rotation = json["rotation"].get<float>();
        }
        if (json.contains("metadata")) {
            anchor.metadata = json["metadata"].dump();
        }
        
        return true;
    }
    catch (const std::exception& e) {
        outError = std::string("Error parsing anchor: ") + e.what();
        return false;
    }
}

bool SchemaValidator::ParseStairs(const nlohmann::json& json, StairConfig& stairs, std::string& outError) {
    try {
        if (!json.contains("type") || !json.contains("connect_to_level")) {
            outError = "Stairs is missing required fields (type, connect_to_level)";
            return false;
        }
        
        // Parse type
        std::string typeStr = json["type"].get<std::string>();
        if (typeStr == "straight") {
            stairs.type = StairType::Straight;
        } else if (typeStr == "L") {
            stairs.type = StairType::L;
        } else if (typeStr == "U") {
            stairs.type = StairType::U;
        } else if (typeStr == "spiral") {
            stairs.type = StairType::Spiral;
        } else {
            outError = "Unknown stair type: " + typeStr;
            return false;
        }
        
        stairs.connectToLevel = json["connect_to_level"].get<int>();
        
        // Optional fields
        if (json.contains("position")) {
            const auto& posJson = json["position"];
            if (!posJson.is_array() || posJson.size() != 2) {
                outError = "Stair position must be an array of 2 numbers [x, y]";
                return false;
            }
            stairs.position[0] = posJson[0].get<float>();
            stairs.position[1] = posJson[1].get<float>();
            
            if (!ValidateGridPos2D(stairs.position, outError)) {
                return false;
            }
        }
        
        if (json.contains("width")) {
            stairs.width = json["width"].get<float>();
        } else {
            stairs.width = 1.2f; // Default width
        }
        
        return true;
    }
    catch (const std::exception& e) {
        outError = std::string("Error parsing stairs: ") + e.what();
        return false;
    }
}

bool SchemaValidator::ValidateGridAlignment(float value, std::string& outError) {
    float remainder = std::fmod(std::abs(value), GRID_SIZE);
    if (remainder > 0.001f && remainder < (GRID_SIZE - 0.001f)) {
        std::ostringstream oss;
        oss << "Value " << value << " is not aligned to grid size " << GRID_SIZE;
        outError = oss.str();
        return false;
    }
    return true;
}

bool SchemaValidator::ValidateGridPos2D(const GridPos2D& pos, std::string& outError) {
    if (!ValidateGridAlignment(pos[0], outError)) {
        outError = "Position X: " + outError;
        return false;
    }
    if (!ValidateGridAlignment(pos[1], outError)) {
        outError = "Position Y: " + outError;
        return false;
    }
    return true;
}

bool SchemaValidator::ValidateGridSize2D(const GridSize2D& size, std::string& outError) {
    if (size[0] <= 0 || size[1] <= 0) {
        outError = "Size must be positive";
        return false;
    }
    if (!ValidateGridAlignment(size[0], outError)) {
        outError = "Size width: " + outError;
        return false;
    }
    if (!ValidateGridAlignment(size[1], outError)) {
        outError = "Size depth: " + outError;
        return false;
    }
    return true;
}

} // namespace Building
} // namespace Moon
