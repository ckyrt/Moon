#include <gtest/gtest.h>

#include "building/BuildingPipeline.h"
#include "core/Assets/AssetPaths.h"
#include "core/CSG/CSGBuilder.h"
#include "core/Object/Blueprint.h"
#include "core/Object/BlueprintLoader.h"
#include "../../external/nlohmann/json.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>

using json = nlohmann::json;

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

std::string DescribeBestEffortReport(const Moon::Building::BestEffortGenerationReport& report) {
    std::ostringstream buffer;
    buffer << "best-effort used=" << (report.usedBestEffort ? "true" : "false");

    if (!report.repairNotes.empty()) {
        buffer << "\nrepair notes:";
        for (const auto& note : report.repairNotes) {
            buffer << "\n  - " << note;
        }
    }

    if (!report.adjustedSpaces.empty()) {
        buffer << "\nadjusted spaces:";
        for (const auto& adjusted : report.adjustedSpaces) {
            buffer << "\n  - floor " << adjusted.floorLevel << " space '" << adjusted.spaceId
                   << "' (" << adjusted.spaceType << "): " << adjusted.reason;
        }
    }

    if (!report.skippedSpaces.empty()) {
        buffer << "\nskipped spaces:";
        for (const auto& skipped : report.skippedSpaces) {
            buffer << "\n  - floor " << skipped.floorLevel << " space '" << skipped.spaceId
                   << "' (" << skipped.spaceType << "): " << skipped.reason;
        }
    }

    return buffer.str();
}

void CollectJsonFilesRecursive(const std::string& root, std::vector<std::string>& files) {
    WIN32_FIND_DATAA findData;
    HANDLE handle = FindFirstFileA((root + "\\*").c_str(), &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        const std::string name = findData.cFileName;
        if (name == "." || name == "..") {
            continue;
        }

        const std::string fullPath = root + "\\" + name;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            CollectJsonFilesRecursive(fullPath, files);
            continue;
        }

        if (name.size() >= 5 && name.substr(name.size() - 5) == ".json") {
            files.push_back(fullPath);
        }
    } while (FindNextFileA(handle, &findData));

    FindClose(handle);
}

std::vector<std::string> CollectJsonFiles(const std::string& root) {
    std::vector<std::string> files;
    CollectJsonFilesRecursive(root, files);
    std::sort(files.begin(), files.end());
    return files;
}

class PreloadedBlueprintDatabase : public Moon::Object::BlueprintDatabase {
public:
    bool LoadObjectIndex(std::string& outError) {
        return LoadIndex(Moon::Assets::BuildObjectPath("index.json"), outError);
    }

    bool ReadObjectIndex(json& outRoot, std::string& outError) {
        std::ifstream file(Moon::Assets::BuildObjectPath("index.json"));
        if (!file.is_open()) {
            outError = "Failed to open object index";
            return false;
        }

        try {
            outRoot = json::parse(file);
        } catch (const json::exception& e) {
            outError = std::string("Failed to parse object index: ") + e.what();
            return false;
        }

        if (!outRoot.contains("items") || !outRoot["items"].is_array()) {
            outError = "Object index missing 'items' array";
            return false;
        }

        return true;
    }
};

} // namespace

TEST(BuildingAssetPresetTests, AllBuildingJsonFilesProcessThroughPipeline) {
    const std::string root = Moon::Assets::BuildAssetPath("building");
    const auto files = CollectJsonFiles(root);
    ASSERT_FALSE(files.empty()) << "No building json files found under " << root;

    Moon::Building::BuildingPipeline pipeline;

    for (const auto& path : files) {
        if (path.size() >= 12 && path.substr(path.size() - 12) == "catalog.json") {
            continue;
        }

        SCOPED_TRACE(path);
        const std::string content = ReadUtf8TextFile(path);
        ASSERT_FALSE(content.empty()) << "Failed to read building asset";

        Moon::Building::GeneratedBuilding building;
        std::string error;
        if (!pipeline.ProcessBuilding(content, building, error)) {
            Moon::Building::GeneratedBuilding bestEffortBuilding;
            Moon::Building::BestEffortGenerationReport report;
            std::string bestEffortError;
            pipeline.ProcessBuildingBestEffort(content, bestEffortBuilding, report, bestEffortError);

            FAIL() << "Building asset failed: " << error
                   << "\nBest-effort follow-up: " << bestEffortError
                   << "\n" << DescribeBestEffortReport(report);
        }
    }
}

TEST(ObjectAssetPresetTests, AllObjectJsonFilesParseAndBuild) {
    const std::string root = Moon::Assets::BuildObjectPath("");
    const auto files = CollectJsonFiles(root);
    ASSERT_FALSE(files.empty()) << "No object json files found under " << root;

    PreloadedBlueprintDatabase database;
    std::string indexError;
    ASSERT_TRUE(database.LoadObjectIndex(indexError)) << indexError;

    json indexRoot;
    ASSERT_TRUE(database.ReadObjectIndex(indexRoot, indexError)) << indexError;
    for (const auto& item : indexRoot["items"]) {
        if (!item.contains("path") || !item["path"].is_string()) {
            continue;
        }

        const std::string relativePath = item["path"].get<std::string>();
        const std::string fullPath = Moon::Assets::BuildObjectPath(relativePath);
        SCOPED_TRACE(fullPath);

        std::ifstream referencedFile(fullPath);
        EXPECT_TRUE(referencedFile.is_open())
            << "Object index entry points to a missing file";
    }

    Moon::CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(&database);

    for (const auto& path : files) {
        if ((path.size() >= 10 && path.substr(path.size() - 10) == "index.json") ||
            (path.size() >= 12 && path.substr(path.size() - 12) == "catalog.json")) {
            continue;
        }

        SCOPED_TRACE(path);
        const std::string content = ReadUtf8TextFile(path);
        ASSERT_FALSE(content.empty()) << "Failed to read object asset";

        std::string parseError;
        auto blueprint = Moon::Object::BlueprintLoader::ParseFromString(content, parseError);
        ASSERT_TRUE(blueprint) << "Object asset parse failed: " << parseError;

        std::unordered_map<std::string, float> params;
        std::string buildError;
        const Moon::CSG::BuildResult result = builder.Build(blueprint.get(), params, buildError);
        EXPECT_TRUE(!result.meshes.empty() || !result.lights.empty())
            << "Object asset build failed: " << buildError;
    }
}
