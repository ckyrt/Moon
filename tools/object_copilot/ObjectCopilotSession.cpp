#include "ObjectCopilotSession.h"

namespace Moon {
namespace Tooling {

namespace {

bool ValidateWorldStateShape(const nlohmann::json& worldState, std::string& outError) {
    if (!worldState.is_object()) {
        outError = "World state root must be a JSON object";
        return false;
    }

    if (!worldState.contains("object_blueprint") || !worldState["object_blueprint"].is_object()) {
        outError = "World state must contain an object_blueprint object";
        return false;
    }

    return true;
}

} // namespace

ObjectCopilotSession::ObjectCopilotSession()
    : m_worldState(CreateDefaultWorldState()) {
    AddMessage(ChatRole::System,
               "Object Copilot session started. Use this prototype to iterate on one object at a time.");
}

std::string ObjectCopilotSession::GetWorldStatePrettyJson() const {
    return m_worldState.dump(2);
}

std::string ObjectCopilotSession::GetObjectBlueprintPrettyJson() const {
    if (!m_worldState.contains("object_blueprint")) {
        return "{}";
    }
    return m_worldState["object_blueprint"].dump(2);
}

bool ObjectCopilotSession::SetWorldStateFromJson(const std::string& worldStateJson, std::string& outError) {
    nlohmann::json parsed = nlohmann::json::parse(worldStateJson, nullptr, false);
    if (parsed.is_discarded()) {
        outError = "World state JSON is invalid";
        return false;
    }

    if (!ValidateWorldStateShape(parsed, outError)) {
        return false;
    }

    m_worldState = std::move(parsed);
    return true;
}

bool ObjectCopilotSession::SetObjectBlueprintFromJson(const std::string& objectJson, std::string& outError) {
    nlohmann::json parsed = nlohmann::json::parse(objectJson, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        outError = "Object blueprint JSON is invalid";
        return false;
    }

    m_worldState["object_blueprint"] = std::move(parsed);
    return true;
}

bool ObjectCopilotSession::ApplyPatch(const AgentPatch& patch, std::string& outError) {
    nlohmann::json nextState = m_worldState;

    try {
        for (const PatchOperation& operation : patch.operations) {
            const nlohmann::json::json_pointer pointer(operation.path);
            if (operation.op == "replace") {
                if (!nextState.contains(pointer)) {
                    outError = "Patch replace path does not exist: " + operation.path;
                    return false;
                }
                nextState[pointer] = operation.value;
            } else if (operation.op == "add") {
                nextState[pointer] = operation.value;
            } else if (operation.op == "remove") {
                const nlohmann::json::json_pointer parentPointer = pointer.parent_pointer();
                nlohmann::json& parent = parentPointer.empty() ? nextState : nextState[parentPointer];
                const std::string token = pointer.back();
                if (parent.is_object()) {
                    parent.erase(token);
                } else {
                    outError = "Patch remove currently supports only object members: " + operation.path;
                    return false;
                }
            } else {
                outError = "Unsupported patch op: " + operation.op;
                return false;
            }
        }
    } catch (const std::exception& e) {
        outError = std::string("Patch apply failed: ") + e.what();
        return false;
    }

    if (!ValidateWorldStateShape(nextState, outError)) {
        return false;
    }

    m_worldState = std::move(nextState);
    return true;
}

bool ObjectCopilotSession::ApplyPatchJson(const std::string& patchJson, std::string& outError) {
    AgentPatch patch;
    if (!ParsePatchJson(patchJson, patch, outError)) {
        return false;
    }
    return ApplyPatch(patch, outError);
}

void ObjectCopilotSession::ResetToDefaultObject() {
    m_worldState = CreateDefaultWorldState();
}

void ObjectCopilotSession::AddMessage(ChatRole role, const std::string& text) {
    ChatMessage message;
    message.role = role;
    message.text = text;
    m_messages.push_back(std::move(message));
}

nlohmann::json ObjectCopilotSession::CreateDefaultWorldState() {
    return {
        {"schema", "moon_object_session"},
        {"version", 1},
        {"agent", {
            {"service_url", "http://127.0.0.1:8008"},
            {"loop_mode", "manual"}
        }},
        {"conversation", {
            {"revision", 0},
            {"summary", "Fresh object session"}
        }},
        {"object_blueprint", {
            {"schema_version", 1},
            {"id", "object_copilot_default_cube"},
            {"name", "Object Copilot Default Cube"},
            {"category", "prototype"},
            {"tags", nlohmann::json::array({"prototype", "default"})},
            {"parameters", {
                {"size", {
                    {"type", "float"},
                    {"default", 100.0},
                    {"min", 10.0},
                    {"max", 400.0}
                }}
            }},
            {"root", {
                {"type", "primitive"},
                {"primitive", "cube"},
                {"params", {
                    {"size", "$size"}
                }},
                {"material", "wood"}
            }}
        }}
    };
}

bool ObjectCopilotSession::ParsePatchJson(const std::string& patchJson,
                                          AgentPatch& outPatch,
                                          std::string& outError) {
    nlohmann::json parsed = nlohmann::json::parse(patchJson, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        outError = "Patch JSON is invalid";
        return false;
    }

    if (parsed.contains("summary") && parsed["summary"].is_string()) {
        outPatch.summary = parsed["summary"].get<std::string>();
    }

    if (!parsed.contains("operations") || !parsed["operations"].is_array()) {
        outError = "Patch JSON must contain an operations array";
        return false;
    }

    outPatch.operations.clear();
    for (const nlohmann::json& item : parsed["operations"]) {
        if (!item.is_object()) {
            outError = "Each patch operation must be an object";
            return false;
        }

        PatchOperation operation;
        operation.op = item.value("op", std::string());
        operation.path = item.value("path", std::string());
        if (operation.op.empty() || operation.path.empty()) {
            outError = "Patch operation requires op and path";
            return false;
        }
        if (item.contains("value")) {
            operation.value = item["value"];
        }

        outPatch.operations.push_back(std::move(operation));
    }

    return true;
}

} // namespace Tooling
} // namespace Moon
