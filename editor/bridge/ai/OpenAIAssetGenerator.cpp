#include "OpenAIAssetGenerator.h"

#include "../../../engine/building/BuildingPipeline.h"
#include "../../../engine/core/Assets/AssetPaths.h"
#include "../../../engine/core/CSG/CSGBuilder.h"
#include "../../../engine/core/Object/BlueprintLoader.h"
#include "../scene/SceneDesign.h"
#include "../../../external/nlohmann/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <initializer_list>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace Moon {
namespace Editor {
namespace AI {

using json = nlohmann::json;

namespace {

constexpr wchar_t kOpenAIHost[] = L"api.openai.com";
constexpr wchar_t kResponsesPath[] = L"/v1/responses";

enum class AssetKind {
    Building,
    Object
};

struct OpenAIResponsePayload {
    json resultJson;
    std::string model;
    std::string responseId;
};

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return std::wstring();
    }

    std::wstring result(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &result[0], required);
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return std::string();
    }

    std::string result(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &result[0], required, nullptr, nullptr);
    return result;
}

std::string GetEnvironmentVariableUtf8(const char* name) {
    char* buffer = nullptr;
    size_t envLength = 0;
    if (_dupenv_s(&buffer, &envLength, name) == 0 && buffer != nullptr && envLength > 1) {
        std::string result(buffer);
        free(buffer);
        return result;
    }
    if (buffer != nullptr) {
        free(buffer);
    }

    wchar_t wideBuffer[8192];
    const DWORD wideLength = ::GetEnvironmentVariableW(Utf8ToWide(name).c_str(), wideBuffer, static_cast<DWORD>(std::size(wideBuffer)));
    if (wideLength == 0 || wideLength >= std::size(wideBuffer)) {
        return std::string();
    }
    return WideToUtf8(std::wstring(wideBuffer, wideBuffer + wideLength));
}

std::vector<std::string> GetModelCandidates() {
    const std::string configuredModel = GetEnvironmentVariableUtf8("OPENAI_MODEL");
    if (!configuredModel.empty()) {
        return { configuredModel };
    }

    return {
        "gpt-4.1-mini",
        "gpt-4.1",
        "gpt-4o-mini"
    };
}

std::string ReadUtf8TextFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (contents.size() >= 3 &&
        static_cast<unsigned char>(contents[0]) == 0xEF &&
        static_cast<unsigned char>(contents[1]) == 0xBB &&
        static_cast<unsigned char>(contents[2]) == 0xBF) {
        contents.erase(0, 3);
    }
    return contents;
}

std::string ProjectPath(const std::string& relativePath) {
    return Moon::Assets::NormalizeSlashes(std::string(Moon::Assets::kProjectRootPath) + "/" + relativePath);
}

bool ReadAllResponseBytes(HINTERNET requestHandle, std::string& outResponseBody, std::string& outError) {
    outResponseBody.clear();

    for (;;) {
        DWORD availableBytes = 0;
        if (!WinHttpQueryDataAvailable(requestHandle, &availableBytes)) {
            outError = "WinHttpQueryDataAvailable failed";
            return false;
        }

        if (availableBytes == 0) {
            return true;
        }

        std::string chunk(static_cast<size_t>(availableBytes), '\0');
        DWORD bytesRead = 0;
        if (!WinHttpReadData(requestHandle, &chunk[0], availableBytes, &bytesRead)) {
            outError = "WinHttpReadData failed";
            return false;
        }

        chunk.resize(bytesRead);
        outResponseBody += chunk;
    }
}

bool ExtractResponseText(const json& responseJson, std::string& outText) {
    if (responseJson.contains("output_text") && responseJson["output_text"].is_string()) {
        outText = responseJson["output_text"].get<std::string>();
        return !outText.empty();
    }

    if (!responseJson.contains("output") || !responseJson["output"].is_array()) {
        return false;
    }

    for (const json& outputItem : responseJson["output"]) {
        if (!outputItem.contains("content") || !outputItem["content"].is_array()) {
            continue;
        }

        for (const json& contentItem : outputItem["content"]) {
            if (contentItem.contains("text") && contentItem["text"].is_string()) {
                outText = contentItem["text"].get<std::string>();
                if (!outText.empty()) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool ValidateBuildingJson(const std::string& buildingJson, std::string& outError) {
    Moon::Building::BuildingPipeline pipeline;
    Moon::Building::ValidationResult validation;
    if (!pipeline.ValidateOnly(buildingJson, validation)) {
        if (!validation.errors.empty()) {
            outError = validation.errors.front();
        } else if (outError.empty()) {
            outError = "Building JSON failed validation";
        }
        return false;
    }
    return true;
}

bool ValidateObjectJson(const std::string& objectJson, std::string& outError) {
    Moon::Object::BlueprintDatabase database;
    if (!database.LoadIndex(Moon::Assets::BuildObjectPath("index.json"), outError)) {
        return false;
    }

    auto blueprint = Moon::Object::BlueprintLoader::ParseFromString(objectJson, outError);
    if (!blueprint) {
        return false;
    }

    Moon::CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(&database);
    std::unordered_map<std::string, float> params;
    Moon::CSG::BuildResult buildResult = builder.Build(blueprint.get(), params, outError);
    if (!outError.empty()) {
        return false;
    }

    if (buildResult.meshes.empty() && buildResult.lights.empty()) {
        outError = "Generated object JSON did not build any meshes or lights";
        return false;
    }

    return true;
}

bool BuildHiddenContext(AssetKind kind, std::string& outContext, std::string& outError) {
    std::vector<std::string> hiddenFiles;
    if (kind == AssetKind::Building) {
        hiddenFiles = {
            ProjectPath("docs/building-system-ai.md"),
            ProjectPath("docs/building-system.md"),
            Moon::Assets::BuildBuildingPath("test_building.json"),
            Moon::Assets::BuildBuildingPath("fixtures/apartment_building.json"),
            Moon::Assets::BuildBuildingPath("reference/corporate_office_tower.json")
        };
    } else {
        hiddenFiles = {
            ProjectPath("docs/object-system-ai.md"),
            ProjectPath("docs/object-system.md"),
            Moon::Assets::BuildObjectPath("index.json"),
            Moon::Assets::BuildObjectPath("components/furniture/table_v1.json"),
            Moon::Assets::BuildObjectPath("components/furniture/chair_v1.json"),
            Moon::Assets::BuildObjectPath("test_room.json")
        };
    }

    std::ostringstream combined;
    for (const std::string& file : hiddenFiles) {
        const std::string contents = ReadUtf8TextFile(file);
        if (contents.empty()) {
            outError = "Failed to load hidden context file: " + file;
            return false;
        }
        combined << "\n=== " << file << " ===\n" << contents << "\n";
    }

    outContext = combined.str();
    return true;
}

bool ExecuteStructuredResponseRequest(const std::string& model,
                                     const std::string& instructionsText,
                                     const std::string& userText,
                                     const std::string& schemaName,
                                     const json& schema,
                                     OpenAIResponsePayload& outPayload,
                                     std::string& outError) {
    json requestBody = {
        {"model", model},
        {"store", false},
        {"input", json::array({
            {
                {"role", "user"},
                {"content", json::array({
                    {
                        {"type", "input_text"},
                        {"text", userText}
                    }
                })}
            }
        })},
        {"instructions", instructionsText},
        {"text", {
            {"format", {
                {"type", "json_schema"},
                {"name", schemaName},
                {"strict", true},
                {"schema", schema}
            }}
        }}
    };

    const std::string apiKey = GetEnvironmentVariableUtf8("OPENAI_API_KEY");
    const std::string project = GetEnvironmentVariableUtf8("OPENAI_PROJECT");
    const std::string organization = GetEnvironmentVariableUtf8("OPENAI_ORGANIZATION");

    HINTERNET sessionHandle = WinHttpOpen(L"MoonEditor/1.0",
                                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                          WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS,
                                          0);
    if (!sessionHandle) {
        outError = "WinHttpOpen failed";
        return false;
    }

    WinHttpSetTimeouts(sessionHandle, 10000, 10000, 10000, 120000);

    HINTERNET connectionHandle = WinHttpConnect(sessionHandle, kOpenAIHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connectionHandle) {
        outError = "WinHttpConnect failed";
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    HINTERNET requestHandle = WinHttpOpenRequest(connectionHandle,
                                                 L"POST",
                                                 kResponsesPath,
                                                 nullptr,
                                                 WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                 WINHTTP_FLAG_SECURE);
    if (!requestHandle) {
        outError = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += Utf8ToWide("Authorization: Bearer " + apiKey) + L"\r\n";
    if (!project.empty()) {
        headers += Utf8ToWide("OpenAI-Project: " + project) + L"\r\n";
    }
    if (!organization.empty()) {
        headers += Utf8ToWide("OpenAI-Organization: " + organization) + L"\r\n";
    }

    const std::string requestJson = requestBody.dump();
    const BOOL sent = WinHttpSendRequest(requestHandle,
                                         headers.c_str(),
                                         static_cast<DWORD>(headers.size()),
                                         reinterpret_cast<LPVOID>(const_cast<char*>(requestJson.data())),
                                         static_cast<DWORD>(requestJson.size()),
                                         static_cast<DWORD>(requestJson.size()),
                                         0);
    if (!sent) {
        outError = "WinHttpSendRequest failed";
        WinHttpCloseHandle(requestHandle);
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    if (!WinHttpReceiveResponse(requestHandle, nullptr)) {
        outError = "WinHttpReceiveResponse failed";
        WinHttpCloseHandle(requestHandle);
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(requestHandle,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,
                             &statusCode,
                             &statusCodeSize,
                             WINHTTP_NO_HEADER_INDEX)) {
        outError = "WinHttpQueryHeaders failed";
        WinHttpCloseHandle(requestHandle);
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    std::string responseBody;
    if (!ReadAllResponseBytes(requestHandle, responseBody, outError)) {
        WinHttpCloseHandle(requestHandle);
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    WinHttpCloseHandle(requestHandle);
    WinHttpCloseHandle(connectionHandle);
    WinHttpCloseHandle(sessionHandle);

    json responseJson = json::parse(responseBody, nullptr, false);
    if (responseJson.is_discarded()) {
        outError = "OpenAI response was not valid JSON";
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (responseJson.contains("error") && responseJson["error"].contains("message")) {
            outError = responseJson["error"]["message"].get<std::string>();
        } else {
            outError = "OpenAI request failed with status " + std::to_string(statusCode);
        }
        return false;
    }

    std::string resultText;
    if (!ExtractResponseText(responseJson, resultText)) {
        outError = "OpenAI response did not contain output text";
        return false;
    }

    outPayload.resultJson = json::parse(resultText, nullptr, false);
    if (outPayload.resultJson.is_discarded()) {
        outError = "Structured OpenAI result was not valid JSON";
        return false;
    }

    outPayload.model = responseJson.value("model", model);
    outPayload.responseId = responseJson.value("id", std::string());
    return true;
}

bool ExecuteStructuredResponseRequestWithFallback(const std::string& instructionsText,
                                                  const std::string& userText,
                                                  const std::string& schemaName,
                                                  const json& schema,
                                                  OpenAIResponsePayload& outPayload,
                                                  std::string& outSelectedModel,
                                                  std::string& outError) {
    std::vector<std::string> errors;

    for (const std::string& model : GetModelCandidates()) {
        std::string attemptError;
        if (ExecuteStructuredResponseRequest(model,
                                             instructionsText,
                                             userText,
                                             schemaName,
                                             schema,
                                             outPayload,
                                             attemptError)) {
            outSelectedModel = model;
            return true;
        }

        errors.push_back(model + ": " + attemptError);
    }

    std::ostringstream combined;
    combined << "All candidate models failed";
    if (!errors.empty()) {
        combined << " (";
        for (size_t i = 0; i < errors.size(); ++i) {
            if (i > 0) {
                combined << " | ";
            }
            combined << errors[i];
        }
        combined << ")";
    }
    outError = combined.str();
    return false;
}

bool GenerateAsset(AssetKind kind,
                   const std::string& userPrompt,
                   const std::string& currentAssetJson,
                   AssetGenerationResult& outResult,
                   std::string& outError) {
    outResult = AssetGenerationResult();

    const std::string apiKey = GetEnvironmentVariableUtf8("OPENAI_API_KEY");
    if (apiKey.empty()) {
        outError = "OPENAI_API_KEY is not configured";
        return false;
    }

    std::string hiddenContext;
    if (!BuildHiddenContext(kind, hiddenContext, outError)) {
        return false;
    }

    const std::string assetLabel = (kind == AssetKind::Building) ? "building" : "object";

    std::ostringstream instructions;
    instructions
        << "You are generating " << assetLabel << " asset JSON for the Moon editor.\n"
        << "Return a structured result whose asset_json field is itself valid JSON text.\n"
        << "Prefer editing the current asset when one is provided instead of throwing it away.\n"
        << "Do not wrap JSON in markdown fences.\n";
    if (kind == AssetKind::Building) {
        instructions
            << "Generate semantic moon_building JSON only.\n"
            << "Keep circulation, floors, spaces, and typology validator-friendly.\n";
    } else {
        instructions
            << "Generate object blueprint JSON only.\n"
            << "Reuse existing references and supported materials from the object index.\n";
    }
    instructions << "\nHidden context follows:\n" << hiddenContext;

    std::ostringstream userInput;
    userInput
        << "User prompt:\n" << userPrompt << "\n\n"
        << "Current " << assetLabel << " asset JSON:\n";
    if (currentAssetJson.empty()) {
        userInput << "(none)\n";
    } else {
        userInput << currentAssetJson << "\n";
    }

    OpenAIResponsePayload payload;
    std::string selectedModel;
    if (!ExecuteStructuredResponseRequestWithFallback(
            instructions.str(),
            userInput.str(),
            std::string(assetLabel) + "_generation_result",
            {
                {"type", "object"},
                {"properties", {
                    {"asset_json", {
                        {"type", "string"},
                        {"description", "The generated Moon asset JSON as a single string."}
                    }},
                    {"strategy", {
                        {"type", "string"}
                    }},
                    {"notes", {
                        {"type", "array"},
                        {"items", {{"type", "string"}}}
                    }}
                }},
                {"required", json::array({"asset_json", "strategy", "notes"})},
                {"additionalProperties", false}
            },
            payload,
            selectedModel,
            outError)) {
        return false;
    }

    const std::string assetJson = payload.resultJson.value("asset_json", std::string());
    if (assetJson.empty()) {
        outError = "Structured OpenAI result did not contain asset_json";
        return false;
    }

    if (kind == AssetKind::Building) {
        if (!ValidateBuildingJson(assetJson, outError)) {
            outError = "OpenAI returned invalid building JSON: " + outError;
            return false;
        }
    } else {
        if (!ValidateObjectJson(assetJson, outError)) {
            outError = "OpenAI returned invalid object JSON: " + outError;
            return false;
        }
    }

    outResult.assetJson = assetJson;
    outResult.strategy = payload.resultJson.value("strategy", "openai_generated");
    outResult.notes = payload.resultJson.value("notes", std::vector<std::string>());
    outResult.hiddenContextSummary = "Injected asset generation guide, source-of-truth docs, and curated JSON examples.";
    outResult.model = payload.model.empty() ? selectedModel : payload.model;
    outResult.responseId = payload.responseId;
    return true;
}

bool GenerateSceneOperations(const std::string& userPrompt,
                             const std::string& currentSceneJson,
                             SceneOperationGenerationResult& outResult,
                             std::string& outError) {
    outResult = SceneOperationGenerationResult();

    const std::string apiKey = GetEnvironmentVariableUtf8("OPENAI_API_KEY");
    if (apiKey.empty()) {
        outError = "OPENAI_API_KEY is not configured";
        return false;
    }

    const std::string sceneGuidePath = ProjectPath("docs/scene-system-ai.md");
    const std::string buildingGuidePath = ProjectPath("docs/building-system-ai.md");
    const std::string objectGuidePath = ProjectPath("docs/object-system-ai.md");
    const std::string sceneGuide = ReadUtf8TextFile(sceneGuidePath);
    const std::string buildingGuide = ReadUtf8TextFile(buildingGuidePath);
    const std::string objectGuide = ReadUtf8TextFile(objectGuidePath);
    if (sceneGuide.empty() || buildingGuide.empty() || objectGuide.empty()) {
        outError = "Failed to load hidden context file for scene operations";
        return false;
    }

    json normalizedScene;
    std::string sceneParseError;
    const std::string sceneJson = currentSceneJson.empty()
        ? std::string("{\"schema\":\"moon_scene_design\",\"version\":1,\"building_instances\":[],\"object_instances\":[]}")
        : currentSceneJson;
    if (!Moon::Editor::SceneDesign::ParseSceneDesign(sceneJson, normalizedScene, sceneParseError)) {
        outError = "Current scene JSON is invalid: " + sceneParseError;
        return false;
    }

    std::ostringstream instructions;
    instructions
        << "You generate structured scene_edit_ops for the Moon editor.\n"
        << "Do not rewrite the whole scene. Only return operations.\n"
        << "Use only supported ops: place_building, place_object, move_instance, remove_instance, replace_instance_asset.\n"
        << "For building redesign requests, use replace_instance_asset with building_json.\n"
        << "For new object placement, prefer inline object_json when the user is describing a brand new object.\n"
        << "When moving an instance, prefer absolute position if the user names a target placement; use delta for relative moves.\n"
        << "Never wrap JSON in markdown fences.\n\n"
        << "Scene editing guide:\n" << sceneGuide
        << "\n\nBuilding asset guide:\n" << buildingGuide
        << "\n\nObject asset guide:\n" << objectGuide;

    std::ostringstream userInput;
    userInput
        << "User prompt:\n" << userPrompt << "\n\n"
        << "Current scene JSON:\n" << normalizedScene.dump(2) << "\n";

    OpenAIResponsePayload payload;
    std::string selectedModel;
    if (!ExecuteStructuredResponseRequestWithFallback(
            instructions.str(),
            userInput.str(),
            "scene_operations_generation_result",
            {
                {"type", "object"},
                {"properties", {
                    {"ops_json", {
                        {"type", "string"},
                        {"description", "A scene_edit_ops JSON document encoded as a string."}
                    }},
                    {"strategy", {{"type", "string"}}},
                    {"notes", {
                        {"type", "array"},
                        {"items", {{"type", "string"}}}
                    }}
                }},
                {"required", json::array({"ops_json", "strategy", "notes"})},
                {"additionalProperties", false}
            },
            payload,
            selectedModel,
            outError)) {
        return false;
    }

    const std::string opsJson = payload.resultJson.value("ops_json", std::string());
    if (opsJson.empty()) {
        outError = "Structured OpenAI result did not contain ops_json";
        return false;
    }

    json parsedOps;
    if (!Moon::Editor::SceneDesign::ParseSceneEditOps(opsJson, parsedOps, outError)) {
        outError = "OpenAI returned invalid scene operations: " + outError;
        return false;
    }

    outResult.opsJson = parsedOps.dump(2);
    outResult.strategy = payload.resultJson.value("strategy", "openai_scene_ops");
    outResult.notes = payload.resultJson.value("notes", std::vector<std::string>());
    outResult.hiddenContextSummary = "Injected scene, building, and object guides plus the full normalized working scene JSON.";
    outResult.model = payload.model.empty() ? selectedModel : payload.model;
    outResult.responseId = payload.responseId;
    return true;
}

} // namespace

bool OpenAIAssetGenerator::IsConfigured() {
    return !GetEnvironmentVariableUtf8("OPENAI_API_KEY").empty();
}

bool OpenAIAssetGenerator::GenerateBuildingFromPrompt(const std::string& userPrompt,
                                                      const std::string& currentBuildingJson,
                                                      AssetGenerationResult& outResult,
                                                      std::string& outError) {
    return GenerateAsset(AssetKind::Building, userPrompt, currentBuildingJson, outResult, outError);
}

bool OpenAIAssetGenerator::GenerateObjectFromPrompt(const std::string& userPrompt,
                                                    const std::string& currentObjectJson,
                                                    AssetGenerationResult& outResult,
                                                    std::string& outError) {
    return GenerateAsset(AssetKind::Object, userPrompt, currentObjectJson, outResult, outError);
}

bool OpenAIAssetGenerator::GenerateSceneOperationsFromPrompt(const std::string& userPrompt,
                                                             const std::string& currentSceneJson,
                                                             SceneOperationGenerationResult& outResult,
                                                             std::string& outError) {
    return GenerateSceneOperations(userPrompt, currentSceneJson, outResult, outError);
}

} // namespace AI
} // namespace Editor
} // namespace Moon
