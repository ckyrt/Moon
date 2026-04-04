#include "BuildingAuthoringReadiness.h"

#include <algorithm>

namespace Moon {
namespace Building {

namespace {

bool HasStyleSignal(const SemanticBuilding& building) {
    return !building.style.facade.empty() &&
           !building.style.roof.empty() &&
           !building.style.windowStyle.empty() &&
           !building.style.material.empty();
}

bool IsProgrammableSpace(const SemanticSpace& space) {
    return space.type != "core" &&
           space.type != "corridor" &&
           space.type != "void";
}

bool HasExplicitFloorPrograms(const SemanticBuilding& building,
                              BuildingWorkflowTemplate workflowTemplate) {
    int programmableSpaceCount = 0;
    int programmableFloorCount = 0;
    bool hasProgrammableUpperFloor = false;

    for (const auto& floor : building.floors) {
        bool floorHasProgram = false;
        for (const auto& space : floor.spaces) {
            if (IsProgrammableSpace(space)) {
                ++programmableSpaceCount;
                floorHasProgram = true;
                if (floor.level > 0) {
                    hasProgrammableUpperFloor = true;
                }
            }
        }
        if (floorHasProgram) {
            ++programmableFloorCount;
        }
    }

    if (workflowTemplate == BuildingWorkflowTemplate::OfficeTower ||
        workflowTemplate == BuildingWorkflowTemplate::RetailMall) {
        if (programmableSpaceCount < 2) {
            return false;
        }
        if (building.mass.floors > 1 && !hasProgrammableUpperFloor) {
            return false;
        }
        return programmableFloorCount >= 1;
    }

    return programmableSpaceCount > 0;
}

bool HasVerticalSignal(const SemanticBuilding& building) {
    if (building.mass.floors <= 1) {
        return true;
    }
    if (!building.verticalSystems.empty()) {
        return true;
    }

    for (const auto& floor : building.floors) {
        for (const auto& space : floor.spaces) {
            if (space.type == "core" || space.type == "stairs") {
                return true;
            }
            if (space.constraints.connectsToFloor >= 0 || space.constraints.connectsFromFloor >= 0) {
                return true;
            }
        }
    }
    return false;
}

bool HasPlateSignal(const SemanticBuilding& building, BuildingWorkflowTemplate workflowTemplate) {
    if (workflowTemplate == BuildingWorkflowTemplate::ResidentialLite ||
        workflowTemplate == BuildingWorkflowTemplate::ResidentialMidrise) {
        return true;
    }

    if (workflowTemplate == BuildingWorkflowTemplate::RetailMall) {
        bool hasVoid = false;
        bool hasCirculationNearVoid = false;
        for (const auto& floor : building.floors) {
            for (const auto& space : floor.spaces) {
                if (space.type == "void") {
                    hasVoid = true;
                }
                if (space.type == "corridor") {
                    for (const auto& adjacency : space.adjacency) {
                        if (adjacency.relationship == "around" || adjacency.relationship == "connected") {
                            hasCirculationNearVoid = true;
                            break;
                        }
                    }
                }
            }
        }
        return hasVoid && hasCirculationNearVoid;
    }

    int floorsWithCore = 0;
    for (const auto& floor : building.floors) {
        const bool hasCoreOnFloor = std::any_of(floor.spaces.begin(), floor.spaces.end(), [](const SemanticSpace& space) {
            return space.type == "core";
        });
        if (hasCoreOnFloor) {
            ++floorsWithCore;
        }
    }
    return floorsWithCore >= std::min<int>(2, static_cast<int>(building.floors.size()));
}

BuildingStageReadiness EvaluateFormStage(const SemanticBuilding& building) {
    BuildingStageReadiness readiness;
    readiness.stage = BuildingAuthoringStage::Form;

    if (building.schema != "moon_building") {
        readiness.missingSignals.push_back("schema");
    }
    if (building.grid <= 0.0f) {
        readiness.missingSignals.push_back("grid");
    }
    if (building.buildingType.empty()) {
        readiness.missingSignals.push_back("building_type");
    }
    if (building.mass.footprintArea <= 0.0f) {
        readiness.missingSignals.push_back("mass.footprint_area");
    }
    if (building.mass.floors <= 0) {
        readiness.missingSignals.push_back("mass.floors");
    }
    if (building.mass.totalHeight <= 0.0f) {
        readiness.missingSignals.push_back("mass.total_height");
    }

    readiness.satisfied = readiness.missingSignals.empty();
    return readiness;
}

BuildingStageReadiness EvaluateVerticalStage(const SemanticBuilding& building) {
    BuildingStageReadiness readiness;
    readiness.stage = BuildingAuthoringStage::Vertical;
    if (!HasVerticalSignal(building)) {
        readiness.missingSignals.push_back("vertical circulation signal");
    }
    readiness.satisfied = readiness.missingSignals.empty();
    return readiness;
}

BuildingStageReadiness EvaluatePlateStage(const SemanticBuilding& building,
                                          BuildingWorkflowTemplate workflowTemplate) {
    BuildingStageReadiness readiness;
    readiness.stage = BuildingAuthoringStage::Plate;
    if (!HasPlateSignal(building, workflowTemplate)) {
        readiness.missingSignals.push_back("plate planning signal");
    }
    readiness.satisfied = readiness.missingSignals.empty();
    return readiness;
}

BuildingStageReadiness EvaluateProgramStage(const SemanticBuilding& building,
                                            BuildingWorkflowTemplate workflowTemplate) {
    BuildingStageReadiness readiness;
    readiness.stage = BuildingAuthoringStage::Program;
    if (building.floors.empty()) {
        readiness.missingSignals.push_back("program.floors");
    }
    if (!HasExplicitFloorPrograms(building, workflowTemplate)) {
        readiness.missingSignals.push_back("programmable spaces");
    }
    readiness.satisfied = readiness.missingSignals.empty();
    return readiness;
}

BuildingStageReadiness EvaluateFacadeStage(const SemanticBuilding& building) {
    BuildingStageReadiness readiness;
    readiness.stage = BuildingAuthoringStage::Facade;
    if (!HasStyleSignal(building)) {
        readiness.missingSignals.push_back("style facade/roof/window/material signals");
    }
    readiness.satisfied = readiness.missingSignals.empty();
    return readiness;
}

BuildingStageReadiness EvaluateStage(const SemanticBuilding& building,
                                     BuildingWorkflowTemplate workflowTemplate,
                                     BuildingAuthoringStage stage) {
    switch (stage) {
        case BuildingAuthoringStage::Form:
            return EvaluateFormStage(building);
        case BuildingAuthoringStage::Vertical:
            return EvaluateVerticalStage(building);
        case BuildingAuthoringStage::Plate:
            return EvaluatePlateStage(building, workflowTemplate);
        case BuildingAuthoringStage::Program:
            return EvaluateProgramStage(building, workflowTemplate);
        case BuildingAuthoringStage::Facade:
            return EvaluateFacadeStage(building);
        case BuildingAuthoringStage::SceneComposition: {
            BuildingStageReadiness readiness;
            readiness.stage = stage;
            readiness.satisfied = false;
            readiness.missingSignals.push_back("scene_edit_ops");
            return readiness;
        }
    }
    BuildingStageReadiness readiness;
    readiness.stage = stage;
    return readiness;
}

} // namespace

bool BuildingAuthoringReadinessReport::IsStageSatisfied(BuildingAuthoringStage stage) const {
    const BuildingStageReadiness* readiness = FindStage(stage);
    return readiness != nullptr && readiness->satisfied;
}

const BuildingStageReadiness* BuildingAuthoringReadinessReport::FindStage(BuildingAuthoringStage stage) const {
    for (const auto& readiness : stages) {
        if (readiness.stage == stage) {
            return &readiness;
        }
    }
    return nullptr;
}

BuildingAuthoringReadinessReport EvaluateAuthoringReadiness(const SemanticBuilding& building) {
    BuildingAuthoringReadinessReport report;
    report.workflowTemplate = SelectWorkflowTemplate(building);

    const std::vector<BuildingAuthoringStage> workflowStages = GetWorkflowStages(report.workflowTemplate);
    report.nextSuggestedStage = workflowStages.empty() ? BuildingAuthoringStage::Form : workflowStages.front();
    report.furthestCompletedStage = BuildingAuthoringStage::Form;

    bool allPreviousSatisfied = true;
    for (const BuildingAuthoringStage stage : workflowStages) {
        const BuildingStageReadiness readiness = EvaluateStage(building, report.workflowTemplate, stage);
        report.stages.push_back(readiness);

        if (stage == BuildingAuthoringStage::SceneComposition) {
            break;
        }

        if (allPreviousSatisfied && readiness.satisfied) {
            report.furthestCompletedStage = stage;
            report.nextSuggestedStage = stage;
        } else if (allPreviousSatisfied && !readiness.satisfied) {
            report.nextSuggestedStage = stage;
            allPreviousSatisfied = false;
        }
    }

    if (allPreviousSatisfied) {
        report.nextSuggestedStage = BuildingAuthoringStage::SceneComposition;
    }

    return report;
}

} // namespace Building
} // namespace Moon
