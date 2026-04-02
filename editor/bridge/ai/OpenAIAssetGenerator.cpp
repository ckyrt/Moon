#include "OpenAIAssetGenerator.h"

#include "../../../engine/building/BuildingPipeline.h"
#include "../../../engine/core/Assets/AssetPaths.h"
#include "../../../engine/core/CSG/CSGBuilder.h"
#include "../../../engine/core/Logging/Logger.h"
#include "../../../engine/core/Object/BlueprintLoader.h"
#include "../scene/SceneDesign.h"
#include "../../../external/nlohmann/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <cctype>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <initializer_list>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

bool GenerateAsset(AssetKind kind,
                   const std::string& userPrompt,
                   const std::string& currentAssetJson,
                   AssetGenerationResult& outResult,
                   std::string& outError);

struct OpenAIResponsePayload {
    json resultJson;
    std::string model;
    std::string responseId;
    std::string rawText;
};

struct RetrievedContextItem {
    std::string label;
    std::string body;
    int score = 0;
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
        "gpt-5-mini",
        "gpt-5",
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

bool IsPrimitiveKeyword(const std::string& value) {
    return value == "cube" || value == "sphere" || value == "cylinder" ||
           value == "capsule" || value == "cone" || value == "torus";
}

std::string ToLowerCopy(const std::string& value) {
    std::string result = value;
    for (char& ch : result) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return result;
}

std::string TruncateForPrompt(const std::string& value, size_t maxChars) {
    if (value.size() <= maxChars) {
        return value;
    }
    return value.substr(0, maxChars) + "\n... [truncated]";
}

std::string ExpandRetrievalQuery(const std::string& text) {
    std::string expanded = text;
    const std::string lower = ToLowerCopy(text);
    const std::vector<std::pair<std::string, std::string>> aliases = {
        {"\xE8\x87\xAA\xE8\xA1\x8C\xE8\xBD\xA6", " bicycle bike wheel frame handlebar pedal seat chain tire "},
        {"\xE5\x8D\x95\xE8\xBD\xA6", " bicycle bike wheel frame handlebar pedal seat chain tire "},
        {"bike", " bicycle wheel frame handlebar pedal seat chain tire "},
        {"bicycle", " bike wheel frame handlebar pedal seat chain tire "},
        {"\xE6\xA1\x8C\xE5\xAD\x90", " table desk furniture tabletop leg "},
        {"\xE4\xB9\xA6\xE6\xA1\x8C", " desk table furniture leg top "},
        {"\xE6\xA4\x85\xE5\xAD\x90", " chair furniture seat backrest leg "},
        {"\xE6\xB2\x99\xE5\x8F\x91", " sofa couch furniture seating cushion armrest "},
        {"\xE5\x86\xB0\xE7\xAE\xB1", " fridge refrigerator appliance kitchen handle door "},
        {"\xE7\x94\xB5\xE8\xA7\x86", " television tv screen stand furniture "},
        {"\xE7\x81\xAF", " lamp light lighting shade bulb point light "},
        {"\xE5\x8F\xB0\xE7\x81\xAF", " desk lamp light lighting shade bulb point light "},
        {"\xE5\xBA\x8A", " bed furniture mattress frame headboard "},
        {"\xE6\x9F\x9C", " cabinet shelf bookshelf storage door panel "},
        {"\xE4\xB9\xA6\xE6\x9E\xB6", " bookshelf shelf storage furniture panel "},
        {"\xE9\x97\xA8", " door frame panel opening architectural "},
        {"\xE7\xAA\x97", " window glass frame architectural opening "},
        {"\xE5\xA2\x99", " wall panel architectural "},
        {"\xE6\x88\xBF\xE9\x97\xB4", " room shell wall floor roof architectural "},
        {"\xE6\xA5\xBC\xE6\xA2\xAF", " stair staircase step railing architectural "},
        {"\xE8\xBD\xAE", " wheel tire rim mechanical vehicle "},
        {"\xE8\xBD\xAE\xE8\x83\x8E", " wheel tire rubber mechanical vehicle "},
        {"\xE6\x9C\xBA\xE6\xA2\xB0", " mechanical vehicle wheel metal frame "},
        {"\xE5\xAE\xB6\xE5\x85\xB7", " furniture chair table sofa bed cabinet shelf "},
        {"\xE5\xAE\xB9\xE5\x99\xA8", " container bowl cup bottle vessel "},
        {"\xE7\xA2\x97", " bowl container vessel "},
        {"\xE6\x9D\xAF", " cup mug container vessel "},
        {"\xE7\x93\xB6", " bottle container vessel "},
        {"\xE9\x87\x91\xE5\xB1\x9E", " metal steel aluminum chrome "},
        {"\xE6\x9C\xA8", " wood wooden painted wood polished wood "},
        {"\xE7\x8E\xBB\xE7\x92\x83", " glass frosted glass tinted glass "},
        {"\xE5\xA1\x91\xE6\x96\x99", " plastic polymer "},
        {"\xE6\xA9\xA1\xE8\x83\xB6", " rubber tire "}
    };
    for (const auto& alias : aliases) {
        if (lower.find(alias.first) != std::string::npos) {
            expanded += alias.second;
        }
    }
    return expanded;
}

std::unordered_set<std::string> TokenizeWords(const std::string& text) {
    std::unordered_set<std::string> tokens;
    std::string current;
    const std::string lower = ToLowerCopy(text);
    const std::unordered_set<std::string> stopWords = {
        "the","and","for","with","from","that","this","into","only","json","asset","object",
        "moon","editor","components","component","using","used","have","has","are","was","were",
        "your","will","just","than","then","when","what","where","which","there","their","about"
    };

    auto flush = [&]() {
        if (current.size() >= 2 && !stopWords.count(current)) {
            tokens.insert(current);
        }
        current.clear();
    };

    for (unsigned char ch : lower) {
        if (std::isalnum(ch) || ch == '_') {
            current.push_back(static_cast<char>(ch));
        } else {
            flush();
        }
    }
    flush();
    return tokens;
}

int CountTokenOverlap(const std::unordered_set<std::string>& queryTokens,
                      const std::unordered_set<std::string>& candidateTokens) {
    int score = 0;
    for (const std::string& token : queryTokens) {
        if (candidateTokens.count(token)) {
            score += static_cast<int>(token.size() >= 6 ? 4 : 2);
        }
    }
    return score;
}

std::string JoinStrings(const std::vector<std::string>& values, const std::string& separator) {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out << separator;
        }
        out << values[i];
    }
    return out.str();
}

std::string ExtractJsonStringArray(const json& value) {
    if (!value.is_array()) {
        return {};
    }
    std::vector<std::string> parts;
    for (const json& item : value) {
        if (item.is_string()) {
            parts.push_back(item.get<std::string>());
        }
    }
    return JoinStrings(parts, ", ");
}

std::string BuildBlueprintMetadataSummary(const json& parsed, const std::string& relativePath) {
    std::vector<std::string> lines;
    lines.push_back("path: " + relativePath);
    if (parsed.contains("id") && parsed["id"].is_string()) {
        lines.push_back("id: " + parsed["id"].get<std::string>());
    }
    if (parsed.contains("name") && parsed["name"].is_string()) {
        lines.push_back("name: " + parsed["name"].get<std::string>());
    }
    if (parsed.contains("description") && parsed["description"].is_string()) {
        lines.push_back("description: " + parsed["description"].get<std::string>());
    }
    if (parsed.contains("category") && parsed["category"].is_string()) {
        lines.push_back("category: " + parsed["category"].get<std::string>());
    }
    if (parsed.contains("tags")) {
        const std::string tags = ExtractJsonStringArray(parsed["tags"]);
        if (!tags.empty()) {
            lines.push_back("tags: " + tags);
        }
    }
    if (parsed.contains("parameters") && parsed["parameters"].is_object()) {
        std::vector<std::string> keys;
        for (auto it = parsed["parameters"].begin(); it != parsed["parameters"].end(); ++it) {
            keys.push_back(it.key());
        }
        std::sort(keys.begin(), keys.end());
        if (!keys.empty()) {
            lines.push_back("parameters: " + JoinStrings(keys, ", "));
        }
    }
    if (parsed.contains("root") && parsed["root"].is_object()) {
        const json& root = parsed["root"];
        if (root.contains("type") && root["type"].is_string()) {
            lines.push_back("root_type: " + root["type"].get<std::string>());
        }
        std::set<std::string> refs;
        std::set<std::string> materials;
        std::set<std::string> primitives;
        std::vector<const json*> stack = { &root };
        while (!stack.empty()) {
            const json* node = stack.back();
            stack.pop_back();
            if (!node->is_object()) {
                continue;
            }
            if (node->contains("ref") && (*node)["ref"].is_string()) {
                refs.insert((*node)["ref"].get<std::string>());
            }
            if (node->contains("material") && (*node)["material"].is_string()) {
                materials.insert((*node)["material"].get<std::string>());
            }
            if (node->contains("primitive") && (*node)["primitive"].is_string()) {
                primitives.insert((*node)["primitive"].get<std::string>());
            }
            if (node->contains("children") && (*node)["children"].is_array()) {
                for (const json& child : (*node)["children"]) {
                    stack.push_back(&child);
                }
            }
            if (node->contains("left")) {
                stack.push_back(&(*node)["left"]);
            }
            if (node->contains("right")) {
                stack.push_back(&(*node)["right"]);
            }
        }
        if (!refs.empty()) {
            lines.push_back("refs: " + JoinStrings(std::vector<std::string>(refs.begin(), refs.end()), ", "));
        }
        if (!materials.empty()) {
            lines.push_back("materials: " + JoinStrings(std::vector<std::string>(materials.begin(), materials.end()), ", "));
        }
        if (!primitives.empty()) {
            lines.push_back("primitives: " + JoinStrings(std::vector<std::string>(primitives.begin(), primitives.end()), ", "));
        }
    }
    return JoinStrings(lines, "\n");
}

bool LooksLikeBlueprintNodeObject(const json& parsed) {
    return parsed.is_object() &&
           parsed.contains("type") &&
           parsed["type"].is_string() &&
           !parsed.contains("root");
}

void RepairObjectNode(json& node) {
    if (!node.is_object()) {
        return;
    }

    if (node.contains("type") && node["type"].is_string()) {
        const std::string typeValue = ToLowerCopy(node["type"].get<std::string>());
        node["type"] = typeValue;
        if (typeValue == "box") {
            node["type"] = "primitive";
            node["primitive"] = "cube";
        } else if (IsPrimitiveKeyword(typeValue)) {
            node["type"] = "primitive";
            if (!node.contains("primitive") || !node["primitive"].is_string()) {
                node["primitive"] = typeValue;
            }
        } else if (typeValue == "primitive" && node.contains("primitive") && node["primitive"].is_string()) {
            const std::string primitiveValue = ToLowerCopy(node["primitive"].get<std::string>());
            node["primitive"] = primitiveValue;
            if (primitiveValue == "box") {
                node["primitive"] = "cube";
            }
        }
    }

    if (node.contains("children") && node["children"].is_array()) {
        for (json& child : node["children"]) {
            RepairObjectNode(child);
        }
    }

    if (node.contains("left")) {
        RepairObjectNode(node["left"]);
    }
    if (node.contains("right")) {
        RepairObjectNode(node["right"]);
    }
}

bool NormalizeGeneratedObjectJson(std::string& objectJson, std::string& outError) {
    json parsed = json::parse(objectJson, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return true;
    }

    if (parsed.contains("root")) {
        RepairObjectNode(parsed["root"]);
    } else if (LooksLikeBlueprintNodeObject(parsed)) {
        parsed = {
            {"schema_version", 1},
            {"id", "generated_object"},
            {"name", "generated_object"},
            {"root", parsed}
        };
        RepairObjectNode(parsed["root"]);
    }

    objectJson = parsed.dump();
    return true;
}

std::string BuildSceneBuildingGenerationPrompt(const std::string& userPrompt,
                                               const json& operation,
                                               const json& instance) {
    std::ostringstream prompt;
    prompt << "Generate a complete Moon building asset JSON for this scene request.\n"
           << "This building will be placed into the scene by scene operations, so the building JSON itself must be fully valid.\n"
           << "User request: " << userPrompt << "\n";

    if (instance.contains("name") && instance["name"].is_string()) {
        prompt << "Requested building instance name: " << instance["name"].get<std::string>() << "\n";
    }
    if (instance.contains("instance_id") && instance["instance_id"].is_string()) {
        prompt << "Requested building instance id: " << instance["instance_id"].get<std::string>() << "\n";
    }
    if (operation.contains("op") && operation["op"].is_string()) {
        prompt << "Scene operation type: " << operation["op"].get<std::string>() << "\n";
    }
    prompt << "Return a complete moon_building JSON with all required style and mass fields filled.\n";
    return prompt.str();
}

bool RepairSceneBuildingOperation(const std::string& userPrompt,
                                  json& operation,
                                  std::vector<std::string>& ioNotes,
                                  std::string& outError) {
    if (!operation.is_object() || !operation.contains("op") || !operation["op"].is_string()) {
        return true;
    }

    const std::string opType = operation["op"].get<std::string>();
    if (opType != "place_building" && opType != "replace_instance_asset") {
        return true;
    }

    if (!operation.contains("instance") || !operation["instance"].is_object()) {
        return true;
    }

    json& instance = operation["instance"];
    const std::string currentBuildingJson =
        (instance.contains("building_json") && instance["building_json"].is_string())
            ? instance["building_json"].get<std::string>()
            : std::string();

    std::string validationError;
    if (!currentBuildingJson.empty() && ValidateBuildingJson(currentBuildingJson, validationError)) {
        return true;
    }

    AssetGenerationResult buildingResult;
    const std::string buildingPrompt = BuildSceneBuildingGenerationPrompt(userPrompt, operation, instance);
    if (!GenerateAsset(AssetKind::Building, buildingPrompt, currentBuildingJson, buildingResult, outError)) {
        outError = "Failed to generate complete building asset for scene operation: " + outError;
        return false;
    }

    instance["building_json"] = buildingResult.assetJson;
    ioNotes.push_back("Upgraded scene building operation to use a fully generated building asset JSON.");
    return true;
}

bool RepairSceneOperations(const std::string& userPrompt,
                           json& ioParsedOps,
                           std::vector<std::string>& ioNotes,
                           std::string& outError) {
    if (!ioParsedOps.is_object() ||
        !ioParsedOps.contains("operations") ||
        !ioParsedOps["operations"].is_array()) {
        return true;
    }

    for (json& operation : ioParsedOps["operations"]) {
        if (!RepairSceneBuildingOperation(userPrompt, operation, ioNotes, outError)) {
            return false;
        }
    }

    return true;
}

bool TryExtractGeneratedAssetJson(AssetKind kind,
                                  const json& resultJson,
                                  std::string& outAssetJson) {
    outAssetJson.clear();

    if (resultJson.contains("asset_root") && resultJson["asset_root"].is_object()) {
        outAssetJson = resultJson["asset_root"].dump();
        return true;
    }

    if (resultJson.contains("asset_json") && resultJson["asset_json"].is_string()) {
        outAssetJson = resultJson["asset_json"].get<std::string>();
        return !outAssetJson.empty();
    }

    if (!resultJson.is_object()) {
        return false;
    }

    if (kind == AssetKind::Building) {
        if (resultJson.contains("schema") || resultJson.contains("building_type") || resultJson.contains("floors")) {
            outAssetJson = resultJson.dump();
            return true;
        }
    } else {
        if (resultJson.contains("root") || LooksLikeBlueprintNodeObject(resultJson)) {
            outAssetJson = resultJson.dump();
            return true;
        }
    }

    return false;
}

bool BuildObjectRetrievedContext(const std::string& userPrompt,
                                 const std::string& currentAssetJson,
                                 std::string& outContext,
                                 std::string& outError) {
    const std::filesystem::path objectRoot(Moon::Assets::BuildObjectPath(""));
    const std::string indexPath = Moon::Assets::BuildObjectPath("index.json");
    const std::string objectGuidePath = ProjectPath("docs/object-system-ai.md");
    const std::string objectSystemPath = ProjectPath("docs/object-system.md");

    const std::string objectGuide = ReadUtf8TextFile(objectGuidePath);
    const std::string objectSystem = ReadUtf8TextFile(objectSystemPath);
    const std::string indexText = ReadUtf8TextFile(indexPath);
    if (objectGuide.empty() || objectSystem.empty() || indexText.empty()) {
        outError = "Failed to load object retrieval context files";
        return false;
    }

    std::ostringstream context;
    context << "\n=== core_rulebook ===\n"
            << TruncateForPrompt(objectGuide, 4000) << "\n"
            << "\n=== object_system_source_of_truth ===\n"
            << TruncateForPrompt(objectSystem, 2500) << "\n";

    const std::string expandedQuery = ExpandRetrievalQuery(userPrompt + "\n" + currentAssetJson);
    const std::unordered_set<std::string> queryTokens = TokenizeWords(expandedQuery);

    json indexJson = json::parse(indexText, nullptr, false);
    if (!indexJson.is_discarded() && indexJson.is_object()) {
        std::vector<RetrievedContextItem> registryMatches;
        if (indexJson.contains("materials") &&
            indexJson["materials"].contains("allowed") &&
            indexJson["materials"]["allowed"].is_array()) {
            std::vector<std::string> allowedMaterials;
            for (const json& item : indexJson["materials"]["allowed"]) {
                if (item.is_string()) {
                    allowedMaterials.push_back(item.get<std::string>());
                }
            }
            context << "\n=== allowed_materials ===\n"
                    << JoinStrings(allowedMaterials, ", ") << "\n";
        }

        if (indexJson.contains("items") && indexJson["items"].is_array()) {
            for (const json& item : indexJson["items"]) {
                if (!item.is_object()) {
                    continue;
                }
                std::ostringstream summary;
                if (item.contains("id") && item["id"].is_string()) {
                    summary << "id: " << item["id"].get<std::string>() << "\n";
                }
                if (item.contains("path") && item["path"].is_string()) {
                    summary << "path: " << item["path"].get<std::string>() << "\n";
                }
                if (item.contains("description") && item["description"].is_string()) {
                    summary << "description: " << item["description"].get<std::string>() << "\n";
                }
                if (item.contains("category") && item["category"].is_string()) {
                    summary << "category: " << item["category"].get<std::string>() << "\n";
                }
                if (item.contains("tags")) {
                    const std::string tags = ExtractJsonStringArray(item["tags"]);
                    if (!tags.empty()) {
                        summary << "tags: " << tags << "\n";
                    }
                }
                const std::string summaryText = summary.str();
                const int score = CountTokenOverlap(queryTokens, TokenizeWords(summaryText));
                if (score > 0) {
                    registryMatches.push_back({"registry_match", summaryText, score});
                }
            }
        }

        std::sort(registryMatches.begin(), registryMatches.end(), [](const RetrievedContextItem& a, const RetrievedContextItem& b) {
            return a.score > b.score;
        });
        if (!registryMatches.empty()) {
            context << "\n=== matching_registry_entries ===\n";
            const size_t registryCount = std::min<size_t>(6, registryMatches.size());
            for (size_t i = 0; i < registryCount; ++i) {
                context << registryMatches[i].body << "\n";
            }
        }
    }

    std::vector<RetrievedContextItem> examples;
    if (std::filesystem::exists(objectRoot)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(objectRoot)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                continue;
            }
            const std::string filename = entry.path().filename().string();
            if (filename == "index.json" || filename == "catalog.json") {
                continue;
            }

            const std::string contents = ReadUtf8TextFile(entry.path().string());
            if (contents.empty()) {
                continue;
            }

            const std::string relativePath = Moon::Assets::NormalizeSlashes(std::filesystem::relative(entry.path(), objectRoot).string());
            json parsed = json::parse(contents, nullptr, false);
            std::string searchCorpus = relativePath + "\n" + contents;
            std::string summary = "path: " + relativePath + "\n";
            if (!parsed.is_discarded() && parsed.is_object()) {
                summary = BuildBlueprintMetadataSummary(parsed, relativePath);
                searchCorpus = summary + "\n" + contents;
            }

            int score = CountTokenOverlap(queryTokens, TokenizeWords(searchCorpus));
            if (relativePath.find("components/") != std::string::npos) {
                score += 2;
            }
            if (relativePath.find("parts/") != std::string::npos) {
                score += 1;
            }
            if (score <= 0) {
                continue;
            }

            std::ostringstream body;
            body << "summary:\n" << summary << "\n"
                 << "json:\n" << TruncateForPrompt(contents, 4500) << "\n";
            examples.push_back({relativePath, body.str(), score});
        }
    }

    std::sort(examples.begin(), examples.end(), [](const RetrievedContextItem& a, const RetrievedContextItem& b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        return a.label < b.label;
    });

    if (examples.empty()) {
        outError = "Object retrieval did not find any relevant examples";
        return false;
    }

    context << "\n=== retrieved_object_examples ===\n";
    const size_t exampleCount = std::min<size_t>(5, examples.size());
    for (size_t i = 0; i < exampleCount; ++i) {
        context << "\n--- example: " << examples[i].label << " (score=" << examples[i].score << ") ---\n"
                << examples[i].body;
    }

    outContext = context.str();
    return true;
}

bool BuildHiddenContext(AssetKind kind,
                        const std::string& userPrompt,
                        const std::string& currentAssetJson,
                        std::string& outContext,
                        std::string& outError) {
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
        return BuildObjectRetrievedContext(userPrompt, currentAssetJson, outContext, outError);
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
                                     bool strictSchema,
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
                {"strict", strictSchema},
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
    outPayload.rawText = resultText;
    return true;
}

bool ExecuteStructuredResponseRequestWithFallback(const std::string& instructionsText,
                                                  const std::string& userText,
                                                  const std::string& schemaName,
                                                  const json& schema,
                                                  bool strictSchema,
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
                                             strictSchema,
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
    if (!BuildHiddenContext(kind, userPrompt, currentAssetJson, hiddenContext, outError)) {
        return false;
    }

    const std::string assetLabel = (kind == AssetKind::Building) ? "building" : "object";
    std::string previousAttemptJson;
    std::string previousValidationError;
    std::string lastRawModelOutput;

    for (int attempt = 0; attempt < 2; ++attempt) {
        std::ostringstream instructions;
        instructions
            << "You are generating " << assetLabel << " asset JSON for the Moon editor.\n"
            << "Return a structured result whose asset_root field is a JSON object.\n"
            << "Prefer editing the current asset when one is provided instead of throwing it away.\n"
            << "Do not wrap JSON in markdown fences.\n";
        if (kind == AssetKind::Building) {
            instructions
                << "Generate semantic moon_building JSON only.\n"
                << "Keep circulation, floors, spaces, and typology validator-friendly.\n"
                << "For adjacency.importance use only 'required' or 'preferred'. Never use 'high', 'medium', 'low', or 'optional'.\n"
                << "Before answering, self-check that the building JSON matches the Moon building schema exactly.\n";
        } else {
            instructions
                << "Generate object blueprint JSON only.\n"
                << "The top-level asset_root must be a full blueprint object with schema_version plus root, and an id or name.\n"
                << "Reuse existing references and supported materials from the object index.\n"
                << "Use only supported node types: primitive, group, reference, csg, light, stair.\n"
                << "For primitive nodes, use only schema keywords such as 'cube', 'sphere', 'cylinder', 'capsule', 'cone', or 'torus'. Never use 'box'.\n"
                << "Primitive nodes must use type='primitive' and primitive='cube'|'sphere'|'cylinder' etc. Never use type='cube' or type='box'.\n"
                << "In transforms and overrides, use only plain numbers or simple $parameter_name references that already exist.\n"
                << "Never use array indexing or invented expression syntax such as $anchor[0], $anchor[1], $anchor[2], foo.bar, or nested lookup syntax.\n"
                << "If you need a wheel center or similar point, either write explicit numeric positions or define separate scalar parameters like front_wheel_x and front_wheel_y.\n"
                << "Do not index into anchors. If anchors exist, they must not be referenced with bracket syntax.\n"
                << "Prefer matching existing object asset conventions from the provided examples.\n";
            if (attempt > 0) {
                instructions
                    << "Your previous attempt failed Moon validation. Repair it conservatively so it passes the blueprint parser and builder.\n"
                    << "Keep the intended design, but change unsupported fields, node types, primitive declarations, expression syntax, or materials as needed.\n";
            }
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
        if (attempt > 0 && !previousAttemptJson.empty()) {
            userInput
                << "\nPrevious invalid generated JSON:\n" << previousAttemptJson << "\n"
                << "\nMoon validation error:\n" << previousValidationError << "\n"
                << "\nReturn repaired full JSON only.\n";
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
                        {"asset_root", {
                            {"type", "object"},
                            {"additionalProperties", true},
                            {"description", "The generated Moon asset JSON as a structured object."}
                        }},
                        {"strategy", {
                            {"type", "string"}
                        }},
                        {"notes", {
                            {"type", "array"},
                            {"items", {{"type", "string"}}}
                        }}
                    }},
                    {"required", json::array({"asset_root", "strategy", "notes"})},
                    {"additionalProperties", false}
                },
                false,
                payload,
                selectedModel,
                outError)) {
            return false;
        }

        std::string candidateAssetJson;
        if (!TryExtractGeneratedAssetJson(kind, payload.resultJson, candidateAssetJson)) {
            outError = "Structured OpenAI result did not contain asset_root";
            if (!payload.rawText.empty()) {
                MOON_LOG_ERROR(
                    "OpenAIAssetGenerator",
                    "Structured wrapper missing expected asset field for %s.\nRaw model output:\n%s",
                    assetLabel.c_str(),
                    TruncateForPrompt(payload.rawText, 8000).c_str());
                outError += "\n\nRaw model output:\n" + TruncateForPrompt(payload.rawText, 4000);
            }
            return false;
        }

        std::string validationError;
        bool valid = false;
        if (kind == AssetKind::Building) {
            valid = ValidateBuildingJson(candidateAssetJson, validationError);
        } else {
            NormalizeGeneratedObjectJson(candidateAssetJson, validationError);
            valid = ValidateObjectJson(candidateAssetJson, validationError);
        }

        if (valid) {
            outResult.assetJson = candidateAssetJson;
            outResult.strategy = payload.resultJson.value("strategy", "openai_generated");
            outResult.notes = payload.resultJson.value("notes", std::vector<std::string>());
            outResult.hiddenContextSummary = (kind == AssetKind::Object)
                ? "Injected core object rules plus dynamically retrieved registry entries and example blueprints chosen from the project."
                : "Injected asset generation guide, source-of-truth docs, and curated JSON examples.";
            outResult.debugContext = TruncateForPrompt(hiddenContext, 12000);
            outResult.rawModelOutput = TruncateForPrompt(payload.rawText, 12000);
            outResult.model = payload.model.empty() ? selectedModel : payload.model;
            outResult.responseId = payload.responseId;
            MOON_LOG_INFO(
                "OpenAIAssetGenerator",
                "Generated %s asset with model=%s responseId=%s strategy=%s\nContext:\n%s\n\nRaw model output:\n%s",
                assetLabel.c_str(),
                outResult.model.c_str(),
                outResult.responseId.c_str(),
                outResult.strategy.c_str(),
                outResult.debugContext.c_str(),
                outResult.rawModelOutput.c_str());
            return true;
        }

        previousAttemptJson = candidateAssetJson;
        previousValidationError = validationError;
        lastRawModelOutput = payload.rawText;
        outError = (kind == AssetKind::Building)
            ? "OpenAI returned invalid building JSON: " + validationError
            : "OpenAI returned invalid object JSON: " + validationError;
    }

    if (!lastRawModelOutput.empty()) {
        outError += "\n\nInjected context:\n" + TruncateForPrompt(hiddenContext, 6000);
        outError += "\n\nRaw model output:\n" + TruncateForPrompt(lastRawModelOutput, 6000);
    }

    MOON_LOG_ERROR(
        "OpenAIAssetGenerator",
        "Failed to generate %s asset.\nError: %s",
        assetLabel.c_str(),
        outError.c_str());

    return false;
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
        << "If an operation includes building_json, that building_json must itself be a complete valid moon_building asset.\n"
        << "Never output placeholder or partial building_json such as style:{} or mass:{}.\n"
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
                    {"scene_ops", {
                        {"type", "object"},
                        {"properties", {
                            {"operations", {
                                {"type", "array"},
                                {"items", {
                                    {"type", "object"},
                                    {"additionalProperties", true}
                                }}
                            }}
                        }},
                        {"required", json::array({"operations"})},
                        {"additionalProperties", false}
                    }},
                    {"strategy", {{"type", "string"}}},
                    {"notes", {
                        {"type", "array"},
                        {"items", {{"type", "string"}}}
                    }}
                }},
                {"required", json::array({"scene_ops", "strategy", "notes"})},
                {"additionalProperties", false}
            },
            false,
            payload,
            selectedModel,
            outError)) {
        return false;
    }

    if (!payload.resultJson.contains("scene_ops") || !payload.resultJson["scene_ops"].is_object()) {
        outError = "Structured OpenAI result did not contain scene_ops";
        return false;
    }

    json parsedOps;
    const std::string opsJson = payload.resultJson["scene_ops"].dump();
    if (!Moon::Editor::SceneDesign::ParseSceneEditOps(opsJson, parsedOps, outError)) {
        outError = "OpenAI returned invalid scene operations: " + outError;
        return false;
    }

    std::vector<std::string> repairedNotes = payload.resultJson.value("notes", std::vector<std::string>());
    if (!RepairSceneOperations(userPrompt, parsedOps, repairedNotes, outError)) {
        return false;
    }

    outResult.opsJson = parsedOps.dump(2);
    outResult.strategy = payload.resultJson.value("strategy", "openai_scene_ops");
    outResult.notes = repairedNotes;
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
