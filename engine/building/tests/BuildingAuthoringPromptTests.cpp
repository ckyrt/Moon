#include <gtest/gtest.h>

#include "building/BuildingAuthoringPrompt.h"

using namespace Moon::Building;

TEST(BuildingAuthoringPromptTests, RoutesVillaConceptRequestToResidentialLiteForm) {
    BuildingAuthoringPromptRequest request;
    request.userPrompt = "Design a simple villa with a garden and pitched roof.";

    const BuildingAuthoringPromptPlan plan = PlanBuildingAuthoringPrompt(request);

    EXPECT_EQ(plan.workflowTemplate, BuildingWorkflowTemplate::ResidentialLite);
    EXPECT_EQ(plan.stage, BuildingAuthoringStage::Form);
    EXPECT_EQ(plan.outputKind, BuildingAuthoringOutputKind::BuildingAssetJson);
    EXPECT_NE(plan.systemInstructions.find("Return a full valid moon_building JSON asset."), std::string::npos);
}

TEST(BuildingAuthoringPromptTests, RoutesOfficeCoreRequestToVerticalStage) {
    BuildingAuthoringPromptRequest request;
    request.userPrompt = "Add two elevator banks and an extra fire stair to the office tower core.";

    const BuildingAuthoringPromptPlan plan = PlanBuildingAuthoringPrompt(request);

    EXPECT_EQ(plan.workflowTemplate, BuildingWorkflowTemplate::OfficeTower);
    EXPECT_EQ(plan.stage, BuildingAuthoringStage::Vertical);
    EXPECT_EQ(plan.outputKind, BuildingAuthoringOutputKind::BuildingAssetJson);
}

TEST(BuildingAuthoringPromptTests, RoutesMallAtriumRequestToPlateStage) {
    BuildingAuthoringPromptRequest request;
    request.userPrompt = "Create a central atrium and ring circulation for the shopping mall.";

    const BuildingAuthoringPromptPlan plan = PlanBuildingAuthoringPrompt(request);

    EXPECT_EQ(plan.workflowTemplate, BuildingWorkflowTemplate::RetailMall);
    EXPECT_EQ(plan.stage, BuildingAuthoringStage::Plate);
    EXPECT_EQ(plan.outputKind, BuildingAuthoringOutputKind::BuildingAssetJson);
}

TEST(BuildingAuthoringPromptTests, RoutesApartmentRoomRequestToProgramStage) {
    BuildingAuthoringPromptRequest request;
    request.userPrompt = "Add one more bathroom and enlarge the kitchen in the apartment layout.";

    const BuildingAuthoringPromptPlan plan = PlanBuildingAuthoringPrompt(request);

    EXPECT_EQ(plan.workflowTemplate, BuildingWorkflowTemplate::ResidentialMidrise);
    EXPECT_EQ(plan.stage, BuildingAuthoringStage::Program);
    EXPECT_EQ(plan.outputKind, BuildingAuthoringOutputKind::BuildingAssetJson);
}

TEST(BuildingAuthoringPromptTests, RoutesFurnitureMoveToSceneOps) {
    BuildingAuthoringPromptRequest request;
    request.userPrompt = "Move the sofa near the window and place a coffee table in front of it.";

    const BuildingAuthoringPromptPlan plan = PlanBuildingAuthoringPrompt(request);

    EXPECT_EQ(plan.stage, BuildingAuthoringStage::SceneComposition);
    EXPECT_EQ(plan.outputKind, BuildingAuthoringOutputKind::SceneEditOps);
    EXPECT_NE(plan.systemInstructions.find("Return scene_edit_ops JSON."), std::string::npos);
}
