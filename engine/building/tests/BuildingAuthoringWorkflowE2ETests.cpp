#include <gtest/gtest.h>

#include "building/BuildingAuthoringPrompt.h"
#include "building/BuildingPipeline.h"
#include "core/Assets/AssetPaths.h"

#include <fstream>
#include <sstream>

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

void ExpectWorkflowAssetProcesses(const std::string& userPrompt,
                                  Moon::Building::BuildingWorkflowTemplate expectedWorkflow,
                                  Moon::Building::BuildingAuthoringStage expectedStage,
                                  const std::string& assetPath) {
    Moon::Building::BuildingAuthoringPromptRequest request;
    request.userPrompt = userPrompt;

    const Moon::Building::BuildingAuthoringPromptPlan plan =
        Moon::Building::PlanBuildingAuthoringPrompt(request);

    EXPECT_EQ(plan.workflowTemplate, expectedWorkflow);
    EXPECT_EQ(plan.stage, expectedStage);
    EXPECT_EQ(plan.outputKind, Moon::Building::BuildingAuthoringOutputKind::BuildingAssetJson);

    const std::string json = ReadUtf8TextFile(assetPath);
    ASSERT_FALSE(json.empty()) << assetPath;

    Moon::Building::BuildingPipeline pipeline;
    Moon::Building::GeneratedBuilding building;
    std::string error;
    EXPECT_TRUE(pipeline.ProcessBuilding(json, building, error))
        << "Representative asset failed pipeline: " << error;
}

} // namespace

TEST(BuildingAuthoringWorkflowE2ETests, VillaWorkflowRequestMapsToValidRepresentativeAsset) {
    ExpectWorkflowAssetProcesses(
        "Design a simple two-story villa with a garden and pitched roof.",
        Moon::Building::BuildingWorkflowTemplate::ResidentialLite,
        Moon::Building::BuildingAuthoringStage::Form,
        Moon::Assets::BuildAssetPath("building/fixtures/villa.json"));
}

TEST(BuildingAuthoringWorkflowE2ETests, OfficeTowerVerticalRequestMapsToValidRepresentativeAsset) {
    ExpectWorkflowAssetProcesses(
        "Add two elevator banks and extra fire stairs to the office tower core.",
        Moon::Building::BuildingWorkflowTemplate::OfficeTower,
        Moon::Building::BuildingAuthoringStage::Vertical,
        Moon::Assets::BuildAssetPath("building/reference/corporate_office_tower.json"));
}

TEST(BuildingAuthoringWorkflowE2ETests, MallPlateRequestMapsToValidRepresentativeAsset) {
    ExpectWorkflowAssetProcesses(
        "Create a central atrium and ring circulation in the shopping mall.",
        Moon::Building::BuildingWorkflowTemplate::RetailMall,
        Moon::Building::BuildingAuthoringStage::Plate,
        Moon::Assets::BuildAssetPath("building/fixtures/shopping_mall.json"));
}
