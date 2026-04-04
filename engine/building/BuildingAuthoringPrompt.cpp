#include "BuildingAuthoringPrompt.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace Moon {
namespace Building {

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool ContainsAny(const std::string& haystack, std::initializer_list<const char*> needles) {
    for (const char* needle : needles) {
        if (haystack.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

BuildingWorkflowTemplate DetermineWorkflowTemplate(const BuildingAuthoringPromptRequest& request) {
    if (request.currentBuilding != nullptr) {
        return SelectWorkflowTemplate(*request.currentBuilding);
    }

    const std::string prompt = ToLower(request.userPrompt);
    BuildingStyle style;

    if (ContainsAny(prompt, {"mall", "shopping center", "shopping centre", "retail center", "retail centre"})) {
        return SelectWorkflowTemplate("mall", style, 3);
    }
    if (ContainsAny(prompt, {"office tower", "high-rise office", "high rise office", "tower office", "office"})) {
        return SelectWorkflowTemplate("office_tower", style, 30);
    }
    if (ContainsAny(prompt, {"apartment", "residential block", "midrise", "mid-rise"})) {
        return SelectWorkflowTemplate("apartment", style, 6);
    }
    if (ContainsAny(prompt, {"villa", "house", "townhouse"})) {
        return SelectWorkflowTemplate("villa", style, 2);
    }

    return BuildingWorkflowTemplate::ResidentialLite;
}

BuildingAuthoringStage DetermineStage(const std::string& promptLower,
                                      BuildingWorkflowTemplate workflowTemplate) {
    if (workflowTemplate == BuildingWorkflowTemplate::RetailMall &&
        ContainsAny(promptLower, {"atrium", "ring circulation", "circulation loop", "mall void", "concourse loop"})) {
        return BuildingAuthoringStage::Plate;
    }
    if ((workflowTemplate == BuildingWorkflowTemplate::ResidentialLite ||
         workflowTemplate == BuildingWorkflowTemplate::ResidentialMidrise) &&
        ContainsAny(promptLower, {"open stair", "stairs", "stair", "double-height", "double height", "split level"})) {
        return BuildingAuthoringStage::Vertical;
    }
    if (ContainsAny(promptLower, {"sofa", "chair", "table", "bed", "desk", "furniture", "move ", "place ", "delete ", "remove ", "replace "})){
        return BuildingAuthoringStage::SceneComposition;
    }
    if (ContainsAny(promptLower, {"facade", "façade", "window", "glass", "material", "balcony", "canopy", "curtain wall", "roof style"})) {
        return BuildingAuthoringStage::Facade;
    }
    if (ContainsAny(promptLower, {"atrium", "void", "floor plate", "plate", "setback", "typical floor", "podium transition"})) {
        return BuildingAuthoringStage::Plate;
    }
    if (ContainsAny(promptLower, {"elevator", "lift", "stair", "stairs", "core", "vertical", "escalator", "sky lobby", "floor height", "double-height", "double height"})) {
        return BuildingAuthoringStage::Vertical;
    }
    if (ContainsAny(promptLower, {"layout", "bedroom", "bathroom", "kitchen", "living room", "office floor", "tenant", "program", "room"})) {
        return BuildingAuthoringStage::Program;
    }

    const std::vector<BuildingAuthoringStage> workflowStages = GetWorkflowStages(workflowTemplate);
    return workflowStages.empty() ? BuildingAuthoringStage::Form : workflowStages.front();
}

std::string BuildRationale(BuildingWorkflowTemplate workflowTemplate,
                           const BuildingAuthoringStageDefinition& stageDefinition,
                           BuildingAuthoringOutputKind outputKind) {
    std::ostringstream rationale;
    rationale << "Workflow template '" << ToString(workflowTemplate)
              << "' selected. Current stage '" << stageDefinition.id
              << "' should return " << ToString(outputKind) << ".";
    return rationale.str();
}

std::string BuildSystemInstructions(BuildingWorkflowTemplate workflowTemplate,
                                    const BuildingAuthoringStageDefinition& stageDefinition,
                                    BuildingAuthoringOutputKind outputKind) {
    std::ostringstream instructions;
    instructions
        << "You are authoring a Moon building design asset.\n"
        << "Follow the staged building workflow and keep authored data valid.\n"
        << "Workflow template: " << ToString(workflowTemplate) << ".\n"
        << "Current stage: " << stageDefinition.id << ".\n"
        << "Stage goal: " << stageDefinition.summary << "\n";

    instructions << "Focus topics:";
    for (const std::string& topic : stageDefinition.focusTopics) {
        instructions << "\n- " << topic;
    }

    instructions << "\nRequired fields:";
    for (const std::string& field : stageDefinition.requiredFields) {
        instructions << "\n- " << field;
    }

    if (outputKind == BuildingAuthoringOutputKind::BuildingAssetJson) {
        instructions
            << "\nReturn a full valid moon_building JSON asset."
            << "\nDo not return partial patches."
            << "\nKeep unchanged valid parts stable."
            << "\nIf the request is underspecified, preserve current valid design decisions instead of inventing scene ops.";
    } else {
        instructions
            << "\nReturn scene_edit_ops JSON."
            << "\nDo not rewrite the whole building asset."
            << "\nOnly express instance placement and scene composition changes.";
    }

    instructions
        << "\nDo not rely on engine-side repair."
        << "\nIf authored data would be invalid, correct the data in the output.";

    return instructions.str();
}

} // namespace

std::string ToString(BuildingAuthoringOutputKind outputKind) {
    switch (outputKind) {
        case BuildingAuthoringOutputKind::BuildingAssetJson:
            return "building_asset_json";
        case BuildingAuthoringOutputKind::SceneEditOps:
            return "scene_edit_ops";
    }
    return "building_asset_json";
}

BuildingAuthoringPromptPlan PlanBuildingAuthoringPrompt(const BuildingAuthoringPromptRequest& request) {
    BuildingAuthoringPromptPlan plan;
    plan.workflowTemplate = DetermineWorkflowTemplate(request);
    plan.workflowId = ToString(plan.workflowTemplate);

    const std::string promptLower = ToLower(request.userPrompt);
    plan.stage = DetermineStage(promptLower, plan.workflowTemplate);
    plan.outputKind = plan.stage == BuildingAuthoringStage::SceneComposition
        ? BuildingAuthoringOutputKind::SceneEditOps
        : BuildingAuthoringOutputKind::BuildingAssetJson;

    const BuildingAuthoringStageDefinition stageDefinition = GetStageDefinition(plan.workflowTemplate, plan.stage);
    plan.stageId = stageDefinition.id;
    plan.rationale = BuildRationale(plan.workflowTemplate, stageDefinition, plan.outputKind);
    plan.systemInstructions = BuildSystemInstructions(plan.workflowTemplate, stageDefinition, plan.outputKind);
    return plan;
}

} // namespace Building
} // namespace Moon
