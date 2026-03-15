#include "BuildingMassingPlanner.h"

#include "MassRuleParser.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Moon {
namespace Massing {

namespace {

using json = nlohmann::json;

constexpr float kMinWidth = 10.0f;
constexpr float kMinDepth = 10.0f;
constexpr int kMinFloors = 1;
constexpr float kMinFloorHeight = 3.0f;

template <typename T>
T ClampValue(T value, T minValue, T maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

json MakeTransform(float x, float y, float z) {
    return {
        {"position", json::array({x, y, z})},
        {"rotation", json::array({0.0f, 0.0f, 0.0f})},
        {"scale", json::array({1.0f, 1.0f, 1.0f})}
    };
}

Curve2D MakeChamferedRectangle(float width, float depth, float cornerCut, const std::string& id) {
    const float halfWidth = width * 0.5f;
    const float halfDepth = depth * 0.5f;
    const float cut = std::max(0.0f, std::min(cornerCut, std::min(halfWidth, halfDepth) * 0.45f));

    Curve2D curve;
    curve.id = id;
    curve.closed = true;

    if (cut <= 0.01f) {
        curve.points = {
            {-halfWidth, -halfDepth},
            {halfWidth, -halfDepth},
            {halfWidth, halfDepth},
            {-halfWidth, halfDepth}
        };
        return curve;
    }

    curve.points = {
        {-halfWidth + cut, -halfDepth},
        {halfWidth - cut, -halfDepth},
        {halfWidth, -halfDepth + cut},
        {halfWidth, halfDepth - cut},
        {halfWidth - cut, halfDepth},
        {-halfWidth + cut, halfDepth},
        {-halfWidth, halfDepth - cut},
        {-halfWidth, -halfDepth + cut}
    };
    return curve;
}

Curve2D ScaleCurve(const Curve2D& source, float scaleX, float scaleY, const std::string& id) {
    Curve2D result = source;
    result.id = id;
    for (auto& point : result.points) {
        point[0] *= scaleX;
        point[1] *= scaleY;
    }
    return result;
}

RuleNode MakeExtrudeNode(const std::string& id,
                         const std::string& name,
                         const std::string& material,
                         const Curve2D& profile,
                         float height,
                         float yOffset) {
    RuleNode node;
    node.id = id;
    node.name = name;
    node.type = RuleNodeType::Extrude;
    node.material = material;
    node.params["height"] = height;
    node.profiles.push_back(profile);
    node.transform.position = {0.0f, yOffset, 0.0f};
    return node;
}

RuleNode MakeTowerLoft(const BuildingMassingIntent& intent,
                       float podiumHeight,
                       float towerHeight,
                       float towerWidth,
                       float towerDepth) {
    RuleNode loft;
    loft.id = "tower_loft";
    loft.name = "tower_loft";
    loft.type = RuleNodeType::Loft;
    loft.material = "glass";

    const Curve2D baseProfile = MakeChamferedRectangle(towerWidth, towerDepth, intent.cornerCut * 0.75f, "tower_base");
    const Curve2D midProfile = ScaleCurve(baseProfile,
                                          std::max(0.75f, 1.0f - intent.taper * 0.45f),
                                          std::max(0.75f, 1.0f - intent.taper * 0.35f),
                                          "tower_mid");
    const Curve2D topProfile = ScaleCurve(baseProfile,
                                          std::max(0.58f, 1.0f - intent.taper),
                                          std::max(0.58f, 1.0f - intent.taper * 0.85f),
                                          "tower_top");

    loft.params["levels"] = json::array({
        0.0f,
        towerHeight * 0.38f,
        towerHeight * 0.76f,
        towerHeight
    });
    loft.params["segments_per_span"] = 6;
    loft.profiles = {
        baseProfile,
        midProfile,
        topProfile,
        ScaleCurve(topProfile, 0.95f, 0.95f, "tower_crown_base")
    };
    loft.transform.position = {0.0f, podiumHeight, 0.0f};

    if (std::abs(intent.twistDegrees) <= 0.01f) {
        return loft;
    }

    RuleNode deform;
    deform.id = "tower_shell";
    deform.name = "tower_shell";
    deform.type = RuleNodeType::Deform;
    deform.material = "glass";
    deform.params["mode"] = "twist";
    deform.params["angle"] = intent.twistDegrees;
    deform.params["subdivide"] = 1;
    deform.children.push_back(loft);
    return deform;
}

RuleNode MakeCrownNode(float width, float depth, float yOffset, float topRotationDegrees) {
    RuleNode crown = MakeExtrudeNode(
        "roof_cap",
        "roof_cap",
        "metal",
        MakeChamferedRectangle(width * 0.92f, depth * 0.92f, 0.9f, "roof_cap_profile"),
        0.8f,
        yOffset);
    crown.transform.rotation = {0.0f, topRotationDegrees, 0.0f};
    return crown;
}

RuleNode MakeFinArray(float towerWidth, float towerDepth, float podiumHeight, float towerHeight) {
    RuleNode arrayNode;
    arrayNode.id = "fins";
    arrayNode.name = "fins";
    arrayNode.type = RuleNodeType::Array;
    arrayNode.params["count_x"] = 8;
    arrayNode.params["count_y"] = 1;
    arrayNode.params["count_z"] = 1;
    arrayNode.params["spacing_x"] = std::max(2.0f, towerWidth / 7.5f);
    arrayNode.params["rotate_step_y"] = 4.0f;
    arrayNode.transform.position = {
        -towerWidth * 0.45f,
        podiumHeight + towerHeight * 0.22f,
        towerDepth * 0.52f
    };

    RuleNode fin;
    fin.id = "fin";
    fin.name = "fin";
    fin.type = RuleNodeType::Primitive;
    fin.primitive = PrimitiveType::Cube;
    fin.material = "metal";
    fin.params["size_x"] = 0.35f;
    fin.params["size_y"] = std::max(8.0f, towerHeight * 0.28f);
    fin.params["size_z"] = 0.75f;

    arrayNode.children.push_back(fin);
    return arrayNode;
}

bool IsSupportedArchetype(const std::string& archetype) {
    return archetype == "tower" ||
           archetype == "podium_tower" ||
           archetype == "courtyard" ||
           archetype == "villa";
}

bool ValidateIntent(BuildingMassingIntent& intent, std::string& outError) {
    if (!IsSupportedArchetype(intent.archetype)) {
        outError = "Unsupported archetype: " + intent.archetype;
        return false;
    }

    intent.width = std::max(intent.width, kMinWidth);
    intent.depth = std::max(intent.depth, kMinDepth);
    intent.floors = std::max(intent.floors, kMinFloors);
    intent.floorHeight = std::max(intent.floorHeight, kMinFloorHeight);
    intent.podiumFloors = std::max(0, std::min(intent.podiumFloors, std::max(0, intent.floors - 1)));
    intent.towerCoverage = ClampValue(intent.towerCoverage, 0.35f, 0.9f);
    intent.setback = std::max(0.0f, std::min(intent.setback, std::min(intent.width, intent.depth) * 0.2f));
    intent.taper = ClampValue(intent.taper, 0.0f, 0.35f);
    intent.twistDegrees = ClampValue(intent.twistDegrees, -24.0f, 24.0f);
    intent.cornerCut = std::max(0.0f, std::min(intent.cornerCut, std::min(intent.width, intent.depth) * 0.12f));
    return true;
}

void FillRuleMetadata(const BuildingMassingIntent& intent, RuleSet& outRuleSet) {
    outRuleSet.schemaVersion = 1;
    outRuleSet.version = 1;
    outRuleSet.unit = "meter";
    outRuleSet.name = intent.buildingType + "_" + intent.archetype;

    std::ostringstream description;
    description << "Planner-generated " << intent.style << " " << intent.buildingType
                << " using archetype '" << intent.archetype << "'";
    outRuleSet.description = description.str();
    outRuleSet.ai = {
        {"source", "building_massing_planner"},
        {"archetype", intent.archetype},
        {"building_type", intent.buildingType},
        {"style", intent.style}
    };
    outRuleSet.editor = {
        {"exposed_params", json::array({"width", "depth", "floors", "podium_floors", "twist_degrees", "taper"})}
    };
}

bool PlanPodiumTower(const BuildingMassingIntent& intent, RuleSet& outRuleSet) {
    const float totalHeight = intent.floorHeight * static_cast<float>(intent.floors);
    const float podiumHeight = intent.floorHeight * static_cast<float>(intent.podiumFloors);
    const float towerHeight = std::max(intent.floorHeight * 6.0f, totalHeight - podiumHeight);

    const float podiumWidth = intent.width;
    const float podiumDepth = intent.depth;
    const float towerWidth = std::max(kMinWidth, podiumWidth * intent.towerCoverage - intent.setback * 0.5f);
    const float towerDepth = std::max(kMinDepth, podiumDepth * intent.towerCoverage - intent.setback * 0.5f);

    outRuleSet.root = RuleNode();
    outRuleSet.root.id = "root";
    outRuleSet.root.name = "root";
    outRuleSet.root.type = RuleNodeType::Group;

    const Curve2D podiumProfile = MakeChamferedRectangle(podiumWidth, podiumDepth, intent.cornerCut, "podium_profile");
    outRuleSet.root.children.push_back(
        MakeExtrudeNode("podium", "podium", "stone", podiumProfile, std::max(intent.floorHeight * 2.0f, podiumHeight), 0.0f));
    outRuleSet.root.children.push_back(MakeTowerLoft(intent, podiumHeight, towerHeight, towerWidth, towerDepth));

    if (intent.crown) {
        outRuleSet.root.children.push_back(MakeCrownNode(towerWidth, towerDepth,
                                                         podiumHeight + towerHeight,
                                                         intent.twistDegrees));
    }
    if (intent.fins) {
        outRuleSet.root.children.push_back(MakeFinArray(towerWidth, towerDepth, podiumHeight, towerHeight));
    }

    return true;
}

bool PlanTower(const BuildingMassingIntent& intent, RuleSet& outRuleSet) {
    BuildingMassingIntent derived = intent;
    derived.podiumFloors = 0;
    derived.towerCoverage = 0.88f;
    derived.fins = derived.fins || derived.floors >= 24;
    return PlanPodiumTower(derived, outRuleSet);
}

bool PlanCourtyard(const BuildingMassingIntent& intent, RuleSet& outRuleSet) {
    outRuleSet.root = RuleNode();
    outRuleSet.root.id = "courtyard_block";
    outRuleSet.root.name = "courtyard_block";
    outRuleSet.root.type = RuleNodeType::Csg;
    outRuleSet.root.csgOperation = CsgOperation::Subtract;
    outRuleSet.root.material = "stone";

    const float height = intent.floorHeight * static_cast<float>(std::max(3, intent.floors));
    const Curve2D outer = MakeChamferedRectangle(intent.width, intent.depth, intent.cornerCut, "outer");
    const Curve2D inner = MakeChamferedRectangle(intent.width * 0.42f, intent.depth * 0.38f, intent.cornerCut * 0.5f, "inner");

    RuleNode outerNode = MakeExtrudeNode("outer_shell", "outer_shell", "stone", outer, height, 0.0f);
    RuleNode innerNode = MakeExtrudeNode("courtyard_void", "courtyard_void", "stone", inner, height + 1.0f, 0.0f);
    outRuleSet.root.children.push_back(outerNode);
    outRuleSet.root.children.push_back(innerNode);
    return true;
}

bool PlanVilla(const BuildingMassingIntent& intent, RuleSet& outRuleSet) {
    outRuleSet.root = RuleNode();
    outRuleSet.root.id = "villa_group";
    outRuleSet.root.name = "villa_group";
    outRuleSet.root.type = RuleNodeType::Group;

    const float mainHeight = intent.floorHeight * static_cast<float>(std::min(intent.floors, 3));
    const float wingWidth = intent.width * 0.42f;
    const float wingDepth = intent.depth * 0.48f;

    RuleNode mainBlock = MakeExtrudeNode("main_block",
                                         "main_block",
                                         "plaster",
                                         MakeChamferedRectangle(intent.width, intent.depth, intent.cornerCut, "main_profile"),
                                         mainHeight,
                                         0.0f);
    RuleNode sideWing = MakeExtrudeNode("side_wing",
                                        "side_wing",
                                        "plaster",
                                        MakeChamferedRectangle(wingWidth, wingDepth, std::max(0.5f, intent.cornerCut * 0.5f), "wing_profile"),
                                        mainHeight * 0.72f,
                                        0.0f);
    sideWing.transform.position = {intent.width * 0.24f, 0.0f, intent.depth * 0.18f};

    RuleNode porch = RuleNode();
    porch.id = "porch";
    porch.name = "porch";
    porch.type = RuleNodeType::Primitive;
    porch.primitive = PrimitiveType::Cube;
    porch.material = "stone";
    porch.params["size_x"] = std::max(4.0f, intent.width * 0.22f);
    porch.params["size_y"] = intent.floorHeight * 0.45f;
    porch.params["size_z"] = std::max(3.0f, intent.depth * 0.16f);
    porch.transform.position = {0.0f, porch.params["size_y"].get<float>() * 0.5f, -intent.depth * 0.5f - porch.params["size_z"].get<float>() * 0.2f};

    outRuleSet.root.children.push_back(mainBlock);
    outRuleSet.root.children.push_back(sideWing);
    outRuleSet.root.children.push_back(porch);
    return true;
}

} // namespace

bool BuildingMassingPlanner::ParseIntent(const nlohmann::json& jsonValue,
                                         BuildingMassingIntent& outIntent,
                                         std::string& outError) {
    if (!jsonValue.is_object()) {
        outError = "Building massing intent must be a JSON object";
        return false;
    }

    BuildingMassingIntent intent;
    intent.archetype = jsonValue.value("archetype", intent.archetype);
    intent.buildingType = jsonValue.value("building_type", intent.buildingType);
    intent.style = jsonValue.value("style", intent.style);
    intent.width = jsonValue.value("width", intent.width);
    intent.depth = jsonValue.value("depth", intent.depth);
    intent.floors = jsonValue.value("floors", intent.floors);
    intent.floorHeight = jsonValue.value("floor_height", intent.floorHeight);
    intent.podiumFloors = jsonValue.value("podium_floors", intent.podiumFloors);
    intent.towerCoverage = jsonValue.value("tower_coverage", intent.towerCoverage);
    intent.setback = jsonValue.value("setback", intent.setback);
    intent.taper = jsonValue.value("taper", intent.taper);
    intent.twistDegrees = jsonValue.value("twist_degrees", intent.twistDegrees);
    intent.crown = jsonValue.value("crown", intent.crown);
    intent.fins = jsonValue.value("fins", intent.fins);
    intent.cornerCut = jsonValue.value("corner_cut", intent.cornerCut);

    if (!ValidateIntent(intent, outError)) {
        return false;
    }

    outIntent = intent;
    return true;
}

bool BuildingMassingPlanner::ParseIntentFromString(const std::string& jsonString,
                                                   BuildingMassingIntent& outIntent,
                                                   std::string& outError) {
    try {
        return ParseIntent(json::parse(jsonString), outIntent, outError);
    } catch (const json::exception& e) {
        outError = std::string("Failed to parse building massing intent JSON: ") + e.what();
        return false;
    }
}

bool BuildingMassingPlanner::Plan(const BuildingMassingIntent& inputIntent,
                                  RuleSet& outRuleSet,
                                  std::string& outError) {
    BuildingMassingIntent intent = inputIntent;
    if (!ValidateIntent(intent, outError)) {
        return false;
    }

    outRuleSet = RuleSet();
    FillRuleMetadata(intent, outRuleSet);

    if (intent.archetype == "podium_tower") {
        return PlanPodiumTower(intent, outRuleSet);
    }
    if (intent.archetype == "tower") {
        return PlanTower(intent, outRuleSet);
    }
    if (intent.archetype == "courtyard") {
        return PlanCourtyard(intent, outRuleSet);
    }
    if (intent.archetype == "villa") {
        return PlanVilla(intent, outRuleSet);
    }

    outError = "Unsupported archetype: " + intent.archetype;
    return false;
}

bool BuildingMassingPlanner::PlanToJsonString(const BuildingMassingIntent& intent,
                                              std::string& outRuleJson,
                                              std::string& outError,
                                              int indent) {
    RuleSet ruleSet;
    if (!Plan(intent, ruleSet, outError)) {
        return false;
    }

    outRuleJson = MassRuleParser::ToJsonString(ruleSet, indent);
    return true;
}

} // namespace Massing
} // namespace Moon
