#pragma once

#include "../../external/nlohmann/json.hpp"

#include <string>

namespace Moon {
namespace Tooling {

struct AgentPatchResponse {
    std::string summary;
    std::string patchJson;
    std::string rawResponseJson;
};

class ObjectCopilotAgentClient {
public:
    static bool RequestPatch(const std::string& serviceUrl,
                             const nlohmann::json& worldState,
                             const nlohmann::json& conversation,
                             const std::string& userPrompt,
                             AgentPatchResponse& outResponse,
                             std::string& outError);
};

} // namespace Tooling
} // namespace Moon
