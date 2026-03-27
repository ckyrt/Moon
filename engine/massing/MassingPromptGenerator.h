#pragma once

#include "BuildingMassingPlanner.h"

#include <string>
#include <vector>

namespace Moon {
namespace Massing {

struct MassingPromptGenerationResult {
    std::string ruleJson;
    std::string strategy;
    std::string hiddenContextSummary;
    std::vector<std::string> notes;
};

class MassingPromptGenerator {
public:
    static bool GenerateFromPrompt(const std::string& userPrompt,
                                   const std::string& currentRuleJson,
                                   MassingPromptGenerationResult& outResult,
                                   std::string& outError);

private:
    static bool BuildHiddenContext(std::string& outContext, std::string& outError);
    static bool LoadCuratedPreset(const std::string& presetFile,
                                  std::string& outRuleJson,
                                  std::string& outError);
    static bool InferIntentFromPrompt(const std::string& userPrompt,
                                      const std::string& currentRuleJson,
                                      BuildingMassingIntent& outIntent,
                                      std::string& outPresetFile,
                                      std::vector<std::string>& outNotes,
                                      std::string& outError);
};

} // namespace Massing
} // namespace Moon
