#pragma once

#include <string>
#include <vector>

namespace Moon {
namespace Massing {

struct OpenAIMassingResponse {
    std::string ruleJson;
    std::string strategy;
    std::vector<std::string> notes;
    std::string model;
    std::string responseId;
};

class OpenAIMassingService {
public:
    static bool IsConfigured();

    static bool GenerateRuleJson(const std::string& hiddenContext,
                                 const std::string& userPrompt,
                                 const std::string& currentRuleJson,
                                 OpenAIMassingResponse& outResponse,
                                 std::string& outError);
};

} // namespace Massing
} // namespace Moon
