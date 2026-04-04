#pragma once

#include "BuildingTypology.h"
#include "SemanticBuildingTypes.h"

#include <string>
#include <vector>

namespace Moon {
namespace Building {

enum class BuildingWorkflowTemplate {
    ResidentialLite,
    ResidentialMidrise,
    OfficeTower,
    RetailMall
};

enum class BuildingAuthoringStage {
    Form,
    Vertical,
    Plate,
    Program,
    Facade,
    SceneComposition
};

struct BuildingAuthoringStageDefinition {
    BuildingAuthoringStage stage = BuildingAuthoringStage::Form;
    std::string id;
    std::string summary;
    std::vector<std::string> focusTopics;
    std::vector<std::string> requiredFields;
    bool rewritesFullAsset = true;
    bool usesSceneOps = false;
};

std::string ToString(BuildingWorkflowTemplate workflowTemplate);
std::string ToString(BuildingAuthoringStage stage);

BuildingWorkflowTemplate SelectWorkflowTemplate(const std::string& buildingType,
                                               const BuildingStyle& style,
                                               int floorCount);
BuildingWorkflowTemplate SelectWorkflowTemplate(const SemanticBuilding& building);

std::vector<BuildingAuthoringStage> GetWorkflowStages(BuildingWorkflowTemplate workflowTemplate);
BuildingAuthoringStageDefinition GetStageDefinition(BuildingWorkflowTemplate workflowTemplate,
                                                    BuildingAuthoringStage stage);

} // namespace Building
} // namespace Moon
