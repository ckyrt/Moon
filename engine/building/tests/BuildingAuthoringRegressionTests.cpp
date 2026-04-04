#include <gtest/gtest.h>

#include "building/BuildingAuthoringPrompt.h"
#include "building/BuildingAuthoringWorkflow.h"
#include "building/BuildingPipeline.h"
#include "core/Assets/AssetPaths.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace {

std::string ReadUtf8TextFile(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }
    return content;
}

struct PromptRouteCase {
    const char* prompt;
    Moon::Building::BuildingWorkflowTemplate workflowTemplate;
    Moon::Building::BuildingAuthoringStage stage;
    Moon::Building::BuildingAuthoringOutputKind outputKind;
};

} // namespace

TEST(BuildingAuthoringRegressionTests, WorkflowTemplatesExposeConsistentStageContracts) {
    using namespace Moon::Building;

    const std::vector<BuildingWorkflowTemplate> templates = {
        BuildingWorkflowTemplate::ResidentialLite,
        BuildingWorkflowTemplate::ResidentialMidrise,
        BuildingWorkflowTemplate::OfficeTower,
        BuildingWorkflowTemplate::RetailMall
    };

    for (const BuildingWorkflowTemplate workflowTemplate : templates) {
        const std::vector<BuildingAuthoringStage> stages = GetWorkflowStages(workflowTemplate);
        ASSERT_FALSE(stages.empty());
        EXPECT_EQ(stages.front(), BuildingAuthoringStage::Form);
        EXPECT_EQ(stages.back(), BuildingAuthoringStage::SceneComposition);

        for (const BuildingAuthoringStage stage : stages) {
            const BuildingAuthoringStageDefinition definition = GetStageDefinition(workflowTemplate, stage);
            EXPECT_FALSE(definition.id.empty());
            EXPECT_FALSE(definition.summary.empty());
            EXPECT_FALSE(definition.requiredFields.empty());

            if (stage == BuildingAuthoringStage::SceneComposition) {
                EXPECT_TRUE(definition.usesSceneOps);
                EXPECT_FALSE(definition.rewritesFullAsset);
            } else {
                EXPECT_FALSE(definition.usesSceneOps);
                EXPECT_TRUE(definition.rewritesFullAsset);
            }
        }
    }
}

TEST(BuildingAuthoringRegressionTests, PromptRoutingCoversRepresentativeWorkflowCases) {
    using namespace Moon::Building;

    const std::vector<PromptRouteCase> cases = {
        {"Design a compact one-story house with a porch and sloped roof.",
         BuildingWorkflowTemplate::ResidentialLite,
         BuildingAuthoringStage::Form,
         BuildingAuthoringOutputKind::BuildingAssetJson},
        {"Add an open stair and double-height living room to the villa.",
         BuildingWorkflowTemplate::ResidentialLite,
         BuildingAuthoringStage::Vertical,
         BuildingAuthoringOutputKind::BuildingAssetJson},
        {"Refine the facade with larger windows and warmer materials for the townhouse.",
         BuildingWorkflowTemplate::ResidentialLite,
         BuildingAuthoringStage::Facade,
         BuildingAuthoringOutputKind::BuildingAssetJson},
        {"Update the three-floor apartment layout to add one more bathroom and a bigger kitchen.",
         BuildingWorkflowTemplate::ResidentialMidrise,
         BuildingAuthoringStage::Program,
         BuildingAuthoringOutputKind::BuildingAssetJson},
        {"Add two elevator banks and a sky lobby to the office tower.",
         BuildingWorkflowTemplate::OfficeTower,
         BuildingAuthoringStage::Vertical,
         BuildingAuthoringOutputKind::BuildingAssetJson},
        {"Adjust the office tower curtain wall and crown design.",
         BuildingWorkflowTemplate::OfficeTower,
         BuildingAuthoringStage::Facade,
         BuildingAuthoringOutputKind::BuildingAssetJson},
        {"Create a central atrium and circulation loop for the shopping mall.",
         BuildingWorkflowTemplate::RetailMall,
         BuildingAuthoringStage::Plate,
         BuildingAuthoringOutputKind::BuildingAssetJson},
        {"Rework the mall tenant mix and food court layout.",
         BuildingWorkflowTemplate::RetailMall,
         BuildingAuthoringStage::Program,
         BuildingAuthoringOutputKind::BuildingAssetJson},
        {"Move the dining table closer to the window.",
         BuildingWorkflowTemplate::ResidentialLite,
         BuildingAuthoringStage::SceneComposition,
         BuildingAuthoringOutputKind::SceneEditOps}
    };

    for (const PromptRouteCase& testCase : cases) {
        SCOPED_TRACE(testCase.prompt);
        BuildingAuthoringPromptRequest request;
        request.userPrompt = testCase.prompt;

        const BuildingAuthoringPromptPlan plan = PlanBuildingAuthoringPrompt(request);
        EXPECT_EQ(plan.workflowTemplate, testCase.workflowTemplate);
        EXPECT_EQ(plan.stage, testCase.stage);
        EXPECT_EQ(plan.outputKind, testCase.outputKind);
        EXPECT_FALSE(plan.workflowId.empty());
        EXPECT_FALSE(plan.stageId.empty());
        EXPECT_FALSE(plan.systemInstructions.empty());
    }
}

TEST(BuildingAuthoringRegressionTests, CurrentBuildingBiasesWorkflowSelectionForIncrementalEdits) {
    using namespace Moon::Building;

    SemanticBuilding building;
    building.buildingType = "office_tower";
    building.mass.floors = 28;

    BuildingAuthoringPromptRequest request;
    request.userPrompt = "Make the facade warmer with more vertical fins.";
    request.currentBuilding = &building;

    const BuildingAuthoringPromptPlan plan = PlanBuildingAuthoringPrompt(request);
    EXPECT_EQ(plan.workflowTemplate, BuildingWorkflowTemplate::OfficeTower);
    EXPECT_EQ(plan.stage, BuildingAuthoringStage::Facade);
}

TEST(BuildingAuthoringRegressionTests, SceneCompositionInstructionsDoNotAskForBuildingRewrite) {
    using namespace Moon::Building;

    BuildingAuthoringPromptRequest request;
    request.userPrompt = "Place a sofa, coffee table, and rug in the living room.";

    const BuildingAuthoringPromptPlan plan = PlanBuildingAuthoringPrompt(request);
    ASSERT_EQ(plan.outputKind, BuildingAuthoringOutputKind::SceneEditOps);
    EXPECT_NE(plan.systemInstructions.find("Return scene_edit_ops JSON."), std::string::npos);
    EXPECT_EQ(plan.systemInstructions.find("Return a full valid moon_building JSON asset."), std::string::npos);
}

TEST(BuildingAuthoringRegressionTests, AssetStagesAlwaysRequestFullBuildingJson) {
    using namespace Moon::Building;

    const std::vector<const char*> prompts = {
        "Design a villa with a courtyard.",
        "Add two stairs to the apartment.",
        "Create an atrium in the mall.",
        "Refine the office tower facade."
    };

    for (const char* prompt : prompts) {
        SCOPED_TRACE(prompt);
        BuildingAuthoringPromptRequest request;
        request.userPrompt = prompt;

        const BuildingAuthoringPromptPlan plan = PlanBuildingAuthoringPrompt(request);
        ASSERT_EQ(plan.outputKind, BuildingAuthoringOutputKind::BuildingAssetJson);
        EXPECT_NE(plan.systemInstructions.find("Return a full valid moon_building JSON asset."), std::string::npos);
        EXPECT_NE(plan.systemInstructions.find("Do not rely on engine-side repair."), std::string::npos);
    }
}

TEST(BuildingAuthoringRegressionTests, RepresentativeAssetsRemainValidForWorkflowCoverage) {
    using namespace Moon::Building;

    const std::vector<std::string> assetPaths = {
        Moon::Assets::BuildAssetPath("building/fixtures/villa.json"),
        Moon::Assets::BuildAssetPath("building/reference/midrise_apartment.json"),
        Moon::Assets::BuildAssetPath("building/reference/corporate_office_tower.json"),
        Moon::Assets::BuildAssetPath("building/fixtures/shopping_mall.json")
    };

    BuildingPipeline pipeline;
    for (const std::string& assetPath : assetPaths) {
        SCOPED_TRACE(assetPath);
        const std::string json = ReadUtf8TextFile(assetPath);
        ASSERT_FALSE(json.empty());

        GeneratedBuilding building;
        std::string error;
        EXPECT_TRUE(pipeline.ProcessBuilding(json, building, error))
            << "Representative workflow asset failed: " << error;
    }
}
