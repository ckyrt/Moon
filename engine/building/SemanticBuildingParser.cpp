#include "SemanticBuildingTypes.h"

#include "../../external/nlohmann/json.hpp"

#include <fstream>
#include <sstream>
#include <utility>

using json = nlohmann::json;

namespace Moon {
namespace Building {

bool SemanticBuildingParser::ParseFromFile(const std::string& filePath,
                                           SemanticBuilding& building,
                                           std::string& error) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        error = "Failed to open file: " + filePath;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return ParseFromString(buffer.str(), building, error);
}

bool SemanticBuildingParser::ParseFromString(const std::string& jsonString,
                                             SemanticBuilding& building,
                                             std::string& error) {
    try {
        json j = json::parse(jsonString);
        building = SemanticBuilding();

        building.schema = j.value("schema", "");
        building.grid = j.value("grid", 0.5f);
        building.buildingType = j.value("building_type", "");

        if (j.contains("style")) {
            auto& style = j["style"];
            building.style.category = style.value("category", "");
            building.style.facade = style.value("facade", "");
            building.style.roof = style.value("roof", "");
            building.style.windowStyle = style.value("window_style", "");
            building.style.material = style.value("material", "");
            building.style.facadeOffset = style.value("facade_offset", 0.0f);
        }

        if (j.contains("mass")) {
            auto& mass = j["mass"];
            building.mass.footprintArea = mass.value("footprint_area", 0.0f);
            building.mass.floors = mass.value("floors", 1);
            building.mass.totalHeight = mass.value("total_height", 0.0f);
            building.mass.massingRuleAsset = mass.value("massing_rule", "");
        }

        if (j.contains("program") && j["program"].contains("floors")) {
            auto& floorsArray = j["program"]["floors"];
            for (auto& floorJson : floorsArray) {
                SemanticFloor floor;
                floor.level = floorJson.value("level", 0);
                floor.name = floorJson.value("name", "");

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

                        if (spaceJson.contains("adjacency")) {
                            for (auto& adjJson : spaceJson["adjacency"]) {
                                Adjacency adj;
                                adj.to = adjJson.value("to", "");
                                adj.relationship = adjJson.value("relationship", "connected");
                                adj.importance = adjJson.value("importance", "optional");
                                space.adjacency.push_back(adj);
                            }
                        }

                        if (spaceJson.contains("constraints")) {
                            auto& constr = spaceJson["constraints"];
                            space.constraints.aspectRatioMax = constr.value("aspect_ratio_max", 0.0f);
                            space.constraints.naturalLight = constr.value("natural_light", "none");
                            space.constraints.exteriorAccess = constr.value("exterior_access", false);
                            space.constraints.ceilingHeight = constr.value("ceiling_height", 3.0f);
                            space.constraints.minWidth = constr.value("min_width", 2.0f);
                            space.constraints.connectsToFloor = constr.value("connects_to_floor", -1);
                            space.constraints.connectsFromFloor = constr.value("connects_from_floor", -1);
                        }

                        floor.spaces.push_back(std::move(space));
                    }
                }

                building.floors.push_back(std::move(floor));
            }
        }

        if (j.contains("vertical_systems") && j["vertical_systems"].is_array()) {
            for (auto& systemJson : j["vertical_systems"]) {
                SemanticVerticalSystem system;
                system.id = systemJson.value("id", "");
                system.type = systemJson.value("type", "");
                system.mode = systemJson.value("mode", "enclosed");
                system.placement = systemJson.value("placement", "internal");
                system.floorFrom = systemJson.value("floor_from", 0);
                system.floorTo = systemJson.value("floor_to", system.floorFrom);
                system.stairForm = systemJson.value("stair_form", "straight");

                const bool hasShaftRect = systemJson.contains("shaft_rect") && systemJson["shaft_rect"].is_object();
                const bool hasStairRect = systemJson.contains("stair_rect") && systemJson["stair_rect"].is_object();
                const json* rectJson = nullptr;
                if (hasShaftRect) {
                    rectJson = &systemJson["shaft_rect"];
                } else if (hasStairRect) {
                    rectJson = &systemJson["stair_rect"];
                }
                if (rectJson) {
                    system.shaftRect.origin = {
                        (*rectJson).value("x", 0.0f),
                        (*rectJson).value("y", 0.0f)
                    };
                    system.shaftRect.size = {
                        (*rectJson).value("w", 0.0f),
                        (*rectJson).value("d", 0.0f)
                    };
                    system.shaftRect.rectId = system.id;
                }

                if (systemJson.contains("access_doors") && systemJson["access_doors"].is_array()) {
                    for (auto& accessJson : systemJson["access_doors"]) {
                        SemanticVerticalAccess access;
                        access.floor = accessJson.value("floor", 0);
                        access.toSpace = accessJson.value("to_space", "");
                        system.accessDoors.push_back(access);
                    }
                }
                if (systemJson.contains("connects") && systemJson["connects"].is_array()) {
                    for (auto& accessJson : systemJson["connects"]) {
                        SemanticVerticalAccess access;
                        access.floor = accessJson.value("floor", 0);
                        access.toSpace = accessJson.value("to_space", "");
                        system.accessDoors.push_back(access);
                    }
                }

                building.verticalSystems.push_back(std::move(system));
            }
        }

        return true;
    }
    catch (const json::exception& e) {
        error = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

} // namespace Building
} // namespace Moon
