#pragma once

#include "BuildingAuthoringWorkflow.h"
#include "SemanticBuildingTypes.h"

#include <string>

namespace Moon {
namespace Building {

enum class BuildingAuthoringOutputKind {
    BuildingAssetJson,
    SceneEditOps
};

struct BuildingAuthoringPromptRequest {
    std::string userPrompt;
    const SemanticBuilding* currentBuilding = nullptr;
};

struct BuildingAuthoringPromptPlan {
    BuildingWorkflowTemplate workflowTemplate = BuildingWorkflowTemplate::ResidentialLite;
    BuildingAuthoringStage stage = BuildingAuthoringStage::Form;
    BuildingAuthoringOutputKind outputKind = BuildingAuthoringOutputKind::BuildingAssetJson;
    std::string stageId;
    std::string workflowId;
    std::string rationale;
    std::string systemInstructions;
};

std::string ToString(BuildingAuthoringOutputKind outputKind);

BuildingAuthoringPromptPlan PlanBuildingAuthoringPrompt(const BuildingAuthoringPromptRequest& request);

} // namespace Building
} // namespace Moon
