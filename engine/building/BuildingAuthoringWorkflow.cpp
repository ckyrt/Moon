#include "BuildingAuthoringWorkflow.h"

namespace Moon {
namespace Building {

namespace {

std::vector<std::string> MakeVector(std::initializer_list<const char*> values) {
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const char* value : values) {
        result.emplace_back(value);
    }
    return result;
}

BuildingAuthoringStageDefinition MakeStage(BuildingAuthoringStage stage,
                                           const char* id,
                                           const char* summary,
                                           std::initializer_list<const char*> focusTopics,
                                           std::initializer_list<const char*> requiredFields,
                                           bool rewritesFullAsset = true,
                                           bool usesSceneOps = false) {
    BuildingAuthoringStageDefinition definition;
    definition.stage = stage;
    definition.id = id;
    definition.summary = summary;
    definition.focusTopics = MakeVector(focusTopics);
    definition.requiredFields = MakeVector(requiredFields);
    definition.rewritesFullAsset = rewritesFullAsset;
    definition.usesSceneOps = usesSceneOps;
    return definition;
}

} // namespace

std::string ToString(BuildingWorkflowTemplate workflowTemplate) {
    switch (workflowTemplate) {
        case BuildingWorkflowTemplate::ResidentialLite:
            return "residential_lite";
        case BuildingWorkflowTemplate::ResidentialMidrise:
            return "residential_midrise";
        case BuildingWorkflowTemplate::OfficeTower:
            return "office_tower";
        case BuildingWorkflowTemplate::RetailMall:
            return "retail_mall";
    }
    return "residential_lite";
}

std::string ToString(BuildingAuthoringStage stage) {
    switch (stage) {
        case BuildingAuthoringStage::Form:
            return "form";
        case BuildingAuthoringStage::Vertical:
            return "vertical";
        case BuildingAuthoringStage::Plate:
            return "plate";
        case BuildingAuthoringStage::Program:
            return "program";
        case BuildingAuthoringStage::Facade:
            return "facade";
        case BuildingAuthoringStage::SceneComposition:
            return "scene_composition";
    }
    return "form";
}

BuildingWorkflowTemplate SelectWorkflowTemplate(const std::string& buildingType,
                                               const BuildingStyle& style,
                                               int floorCount) {
    const BuildingTypology typology = InferBuildingTypology(buildingType, style);
    if (IsRetailTypology(typology)) {
        return BuildingWorkflowTemplate::RetailMall;
    }
    if (IsOfficeTypology(typology)) {
        return BuildingWorkflowTemplate::OfficeTower;
    }
    if (IsVillaTypology(typology) || floorCount <= 2) {
        return BuildingWorkflowTemplate::ResidentialLite;
    }
    return BuildingWorkflowTemplate::ResidentialMidrise;
}

BuildingWorkflowTemplate SelectWorkflowTemplate(const SemanticBuilding& building) {
    return SelectWorkflowTemplate(building.buildingType, building.style, building.mass.floors);
}

std::vector<BuildingAuthoringStage> GetWorkflowStages(BuildingWorkflowTemplate workflowTemplate) {
    switch (workflowTemplate) {
        case BuildingWorkflowTemplate::ResidentialLite:
            return {
                BuildingAuthoringStage::Form,
                BuildingAuthoringStage::Vertical,
                BuildingAuthoringStage::Program,
                BuildingAuthoringStage::Facade,
                BuildingAuthoringStage::SceneComposition
            };
        case BuildingWorkflowTemplate::ResidentialMidrise:
            return {
                BuildingAuthoringStage::Form,
                BuildingAuthoringStage::Vertical,
                BuildingAuthoringStage::Program,
                BuildingAuthoringStage::Facade,
                BuildingAuthoringStage::SceneComposition
            };
        case BuildingWorkflowTemplate::OfficeTower:
            return {
                BuildingAuthoringStage::Form,
                BuildingAuthoringStage::Vertical,
                BuildingAuthoringStage::Plate,
                BuildingAuthoringStage::Program,
                BuildingAuthoringStage::Facade,
                BuildingAuthoringStage::SceneComposition
            };
        case BuildingWorkflowTemplate::RetailMall:
            return {
                BuildingAuthoringStage::Form,
                BuildingAuthoringStage::Vertical,
                BuildingAuthoringStage::Plate,
                BuildingAuthoringStage::Program,
                BuildingAuthoringStage::Facade,
                BuildingAuthoringStage::SceneComposition
            };
    }
    return {};
}

BuildingAuthoringStageDefinition GetStageDefinition(BuildingWorkflowTemplate workflowTemplate,
                                                    BuildingAuthoringStage stage) {
    switch (workflowTemplate) {
        case BuildingWorkflowTemplate::ResidentialLite:
            switch (stage) {
                case BuildingAuthoringStage::Form:
                    return MakeStage(stage,
                                     "form",
                                     "Lock the house or villa massing before detailed room planning.",
                                     {"footprint", "floors", "roof", "terraces", "garage", "garden relation"},
                                     {"building_type", "mass.footprint_area", "mass.floors", "mass.total_height"});
                case BuildingAuthoringStage::Vertical:
                    return MakeStage(stage,
                                     "vertical",
                                     "Confirm floor heights and stair strategy before final room arrangement.",
                                     {"floor heights", "stairs", "double-height spaces", "entry sequence"},
                                     {"program.floors[].level", "vertical_systems"});
                case BuildingAuthoringStage::Program:
                    return MakeStage(stage,
                                     "program",
                                     "Define room-by-room layout and living adjacencies.",
                                     {"living spaces", "bedrooms", "kitchen", "bathrooms", "circulation", "room adjacency"},
                                     {"program.floors[].spaces"});
                case BuildingAuthoringStage::Facade:
                    return MakeStage(stage,
                                     "facade",
                                     "Finalize doors, windows, balconies, and style language after plan approval.",
                                     {"openings", "balconies", "materials", "architectural style"},
                                     {"style"});
                case BuildingAuthoringStage::SceneComposition:
                    return MakeStage(stage,
                                     "scene_composition",
                                     "Place furniture and objects with scene ops instead of rewriting the building asset.",
                                     {"furniture", "props", "landscape placement", "instance transforms"},
                                     {"scene_edit_ops"},
                                     false,
                                     true);
                case BuildingAuthoringStage::Plate:
                    break;
            }
            break;
        case BuildingWorkflowTemplate::ResidentialMidrise:
            switch (stage) {
                case BuildingAuthoringStage::Form:
                    return MakeStage(stage,
                                     "form",
                                     "Lock footprint, floor count, and building mass before unit planning.",
                                     {"footprint", "setbacks", "floors", "roof", "entry massing"},
                                     {"building_type", "mass.footprint_area", "mass.floors", "mass.total_height"});
                case BuildingAuthoringStage::Vertical:
                    return MakeStage(stage,
                                     "vertical",
                                     "Confirm stairs, elevators, and floor height strategy before unit layout.",
                                     {"stairs", "elevators", "core placement", "corridor strategy", "floor heights"},
                                     {"vertical_systems", "program.floors[].level"});
                case BuildingAuthoringStage::Program:
                    return MakeStage(stage,
                                     "program",
                                     "Arrange units, corridors, services, and shared spaces floor by floor.",
                                     {"unit mix", "corridors", "cores", "amenities", "service spaces"},
                                     {"program.floors[].spaces"});
                case BuildingAuthoringStage::Facade:
                    return MakeStage(stage,
                                     "facade",
                                     "Finalize residential facade language only after unit logic is stable.",
                                     {"windows", "balconies", "sun shading", "materials"},
                                     {"style"});
                case BuildingAuthoringStage::SceneComposition:
                    return MakeStage(stage,
                                     "scene_composition",
                                     "Compose outdoor props and scene instances with scene ops.",
                                     {"site objects", "furniture", "landscape placement"},
                                     {"scene_edit_ops"},
                                     false,
                                     true);
                case BuildingAuthoringStage::Plate:
                    break;
            }
            break;
        case BuildingWorkflowTemplate::OfficeTower:
            switch (stage) {
                case BuildingAuthoringStage::Form:
                    return MakeStage(stage,
                                     "form",
                                     "Lock tower and podium massing before solving core and typical floor logic.",
                                     {"tower massing", "podium", "setbacks", "overall height", "lobby presence"},
                                     {"building_type", "mass.footprint_area", "mass.floors", "mass.total_height"});
                case BuildingAuthoringStage::Vertical:
                    return MakeStage(stage,
                                     "vertical",
                                     "Define core, elevator zoning, stairs, and vertical circulation first.",
                                     {"core", "elevator banks", "stairs", "service risers", "sky lobby"},
                                     {"vertical_systems", "program.floors[].level"});
                case BuildingAuthoringStage::Plate:
                    return MakeStage(stage,
                                     "plate",
                                     "Approve typical floor plate, voids, and special floor exceptions before detailed office planning.",
                                     {"typical floor", "voids", "setbacks by level", "mechanical floors", "podium transitions"},
                                     {"mass", "program.floors[].level"});
                case BuildingAuthoringStage::Program:
                    return MakeStage(stage,
                                     "program",
                                     "Assign office, lobby, amenity, retail, and service uses floor by floor.",
                                     {"lobby", "office floors", "amenity floors", "meeting zones", "services"},
                                     {"program.floors[].spaces"});
                case BuildingAuthoringStage::Facade:
                    return MakeStage(stage,
                                     "facade",
                                     "Apply facade systems only after tower planning and floor plate logic are stable.",
                                     {"curtain wall", "crown", "sun shading", "material rhythm"},
                                     {"style"});
                case BuildingAuthoringStage::SceneComposition:
                    return MakeStage(stage,
                                     "scene_composition",
                                     "Use scene ops for site furniture, plaza objects, and building placement edits.",
                                     {"plaza objects", "site props", "instance transforms"},
                                     {"scene_edit_ops"},
                                     false,
                                     true);
            }
            break;
        case BuildingWorkflowTemplate::RetailMall:
            switch (stage) {
                case BuildingAuthoringStage::Form:
                    return MakeStage(stage,
                                     "form",
                                     "Lock mall footprint, anchors, and massing before circulation and atrium design.",
                                     {"footprint", "anchors", "entrances", "podium massing", "roof lights"},
                                     {"building_type", "mass.footprint_area", "mass.floors", "mass.total_height"});
                case BuildingAuthoringStage::Vertical:
                    return MakeStage(stage,
                                     "vertical",
                                     "Define escalators, elevators, stairs, and vertical visitor movement early.",
                                     {"escalators", "elevators", "stairs", "service lifts", "entry connections"},
                                     {"vertical_systems", "program.floors[].level"});
                case BuildingAuthoringStage::Plate:
                    return MakeStage(stage,
                                     "plate",
                                     "Approve atriums, voids, and circulation plates before tenant planning.",
                                     {"atriums", "voids", "shop bands", "circulation loops", "special volumes"},
                                     {"mass", "program.floors[].level"});
                case BuildingAuthoringStage::Program:
                    return MakeStage(stage,
                                     "program",
                                     "Place anchor stores, shop strips, food, entertainment, and service spaces by level.",
                                     {"anchors", "shops", "food court", "cinema", "service back-of-house"},
                                     {"program.floors[].spaces"});
                case BuildingAuthoringStage::Facade:
                    return MakeStage(stage,
                                     "facade",
                                     "Finalize entrances, canopies, skylights, and exterior retail expression last.",
                                     {"canopies", "skylights", "shopfront rhythm", "materials"},
                                     {"style"});
                case BuildingAuthoringStage::SceneComposition:
                    return MakeStage(stage,
                                     "scene_composition",
                                     "Use scene ops for kiosks, seating, landscaping, and instance placement changes.",
                                     {"kiosks", "seating", "landscape", "instance transforms"},
                                     {"scene_edit_ops"},
                                     false,
                                     true);
            }
            break;
    }

    return MakeStage(stage,
                     "unknown",
                     "No workflow definition available for this stage.",
                     {},
                     {});
}

} // namespace Building
} // namespace Moon
