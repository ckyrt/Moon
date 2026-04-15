#pragma once

#include "../../external/nlohmann/json.hpp"

#include <string>
#include <vector>

namespace Moon {
namespace Tooling {

enum class ChatRole {
    User,
    Assistant,
    System
};

struct ChatMessage {
    ChatRole role = ChatRole::System;
    std::string text;
};

struct PatchOperation {
    std::string op;
    std::string path;
    nlohmann::json value;
};

struct AgentPatch {
    std::string summary;
    std::vector<PatchOperation> operations;
};

class ObjectCopilotSession {
public:
    ObjectCopilotSession();

    const nlohmann::json& GetWorldState() const { return m_worldState; }
    std::string GetWorldStatePrettyJson() const;
    std::string GetObjectBlueprintPrettyJson() const;

    bool SetWorldStateFromJson(const std::string& worldStateJson, std::string& outError);
    bool SetObjectBlueprintFromJson(const std::string& objectJson, std::string& outError);

    bool ApplyPatch(const AgentPatch& patch, std::string& outError);
    bool ApplyPatchJson(const std::string& patchJson, std::string& outError);

    void ResetToDefaultObject();

    void AddMessage(ChatRole role, const std::string& text);
    const std::vector<ChatMessage>& GetMessages() const { return m_messages; }

private:
    static nlohmann::json CreateDefaultWorldState();
    static bool ParsePatchJson(const std::string& patchJson, AgentPatch& outPatch, std::string& outError);

    nlohmann::json m_worldState;
    std::vector<ChatMessage> m_messages;
};

} // namespace Tooling
} // namespace Moon
