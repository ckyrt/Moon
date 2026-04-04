#include <algorithm>
#include <gtest/gtest.h>

#include "building/BuildingAuthoringWorkflow.h"

using namespace Moon::Building;

TEST(BuildingAuthoringWorkflowTests, SelectsResidentialLiteForVilla) {
    BuildingStyle style;
    EXPECT_EQ(SelectWorkflowTemplate("villa", style, 2), BuildingWorkflowTemplate::ResidentialLite);
}

TEST(BuildingAuthoringWorkflowTests, SelectsOfficeTowerForOfficeTower) {
    BuildingStyle style;
    EXPECT_EQ(SelectWorkflowTemplate("office_tower", style, 30), BuildingWorkflowTemplate::OfficeTower);
}

TEST(BuildingAuthoringWorkflowTests, SelectsRetailMallForShoppingMallAlias) {
    BuildingStyle style;
    EXPECT_EQ(SelectWorkflowTemplate("shopping_mall", style, 3), BuildingWorkflowTemplate::RetailMall);
}

TEST(BuildingAuthoringWorkflowTests, ResidentialLiteWorkflowSkipsPlateStage) {
    const std::vector<BuildingAuthoringStage> stages = GetWorkflowStages(BuildingWorkflowTemplate::ResidentialLite);
    EXPECT_EQ(stages.size(), 5u);
    EXPECT_EQ(stages.front(), BuildingAuthoringStage::Form);
    EXPECT_EQ(stages.back(), BuildingAuthoringStage::SceneComposition);
    EXPECT_EQ(std::find(stages.begin(), stages.end(), BuildingAuthoringStage::Plate), stages.end());
}

TEST(BuildingAuthoringWorkflowTests, OfficeTowerWorkflowIncludesPlateStage) {
    const std::vector<BuildingAuthoringStage> stages = GetWorkflowStages(BuildingWorkflowTemplate::OfficeTower);
    EXPECT_NE(std::find(stages.begin(), stages.end(), BuildingAuthoringStage::Plate), stages.end());
}

TEST(BuildingAuthoringWorkflowTests, SceneCompositionUsesSceneOpsInsteadOfAssetRewrite) {
    const BuildingAuthoringStageDefinition definition = GetStageDefinition(
        BuildingWorkflowTemplate::RetailMall,
        BuildingAuthoringStage::SceneComposition);
    EXPECT_TRUE(definition.usesSceneOps);
    EXPECT_FALSE(definition.rewritesFullAsset);
    EXPECT_EQ(definition.id, "scene_composition");
}
