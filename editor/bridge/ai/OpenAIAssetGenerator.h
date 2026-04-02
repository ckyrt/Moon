#pragma once

#include <string>
#include <vector>

namespace Moon {
namespace Editor {
namespace AI {

struct AssetGenerationResult {
    std::string assetJson;
    std::string strategy;
    std::string hiddenContextSummary;
    std::string debugContext;
    std::string rawModelOutput;
    std::vector<std::string> notes;
    std::string model;
    std::string responseId;
};

struct SceneOperationGenerationResult {
    std::string opsJson;
    std::string strategy;
    std::string hiddenContextSummary;
    std::string debugContext;
    std::string rawModelOutput;
    std::vector<std::string> notes;
    std::string model;
    std::string responseId;
};

class OpenAIAssetGenerator {
public:
    static bool IsConfigured();

    static bool GenerateBuildingFromPrompt(const std::string& userPrompt,
                                           const std::string& currentBuildingJson,
                                           AssetGenerationResult& outResult,
                                           std::string& outError);

    static bool GenerateObjectFromPrompt(const std::string& userPrompt,
                                         const std::string& currentObjectJson,
                                         AssetGenerationResult& outResult,
                                         std::string& outError);

    static bool GenerateSceneOperationsFromPrompt(const std::string& userPrompt,
                                                  const std::string& currentSceneJson,
                                                  SceneOperationGenerationResult& outResult,
                                                  std::string& outError);
};

} // namespace AI
} // namespace Editor
} // namespace Moon
