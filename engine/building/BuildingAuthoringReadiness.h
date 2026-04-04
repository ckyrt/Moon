#pragma once

#include "BuildingAuthoringWorkflow.h"

#include <string>
#include <vector>

namespace Moon {
namespace Building {

struct BuildingStageReadiness {
    BuildingAuthoringStage stage = BuildingAuthoringStage::Form;
    bool satisfied = false;
    std::vector<std::string> missingSignals;
};

struct BuildingAuthoringReadinessReport {
    BuildingWorkflowTemplate workflowTemplate = BuildingWorkflowTemplate::ResidentialLite;
    std::vector<BuildingStageReadiness> stages;
    BuildingAuthoringStage furthestCompletedStage = BuildingAuthoringStage::Form;
    BuildingAuthoringStage nextSuggestedStage = BuildingAuthoringStage::Form;

    bool IsStageSatisfied(BuildingAuthoringStage stage) const;
    const BuildingStageReadiness* FindStage(BuildingAuthoringStage stage) const;
};

BuildingAuthoringReadinessReport EvaluateAuthoringReadiness(const SemanticBuilding& building);

} // namespace Building
} // namespace Moon
