#include <gtest/gtest.h>

#include "building/BuildingQualityChecks.h"
#include "building/BuildingPipeline.h"
#include "building/BuildingToObjectBlueprintConverter.h"
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

const json* FindNodeByNameRecursive(const json& node, const std::string& name) {
    if (!node.is_object()) {
        return nullptr;
    }

    if (node.value("name", std::string()) == name) {
        return &node;
    }

    if (node.contains("left")) {
        if (const json* found = FindNodeByNameRecursive(node["left"], name)) {
            return found;
        }
    }
    if (node.contains("right")) {
        if (const json* found = FindNodeByNameRecursive(node["right"], name)) {
            return found;
        }
    }
    if (node.contains("children") && node["children"].is_array()) {
        for (const auto& child : node["children"]) {
            if (const json* found = FindNodeByNameRecursive(child, name)) {
                return found;
            }
        }
    }
    return nullptr;
}

void CollectNodesByPrefixRecursive(const json& node,
                                   const std::string& prefix,
                                   std::vector<const json*>& outNodes) {
    if (!node.is_object()) {
        return;
    }

    const std::string name = node.value("name", std::string());
    if (name.rfind(prefix, 0) == 0) {
        outNodes.push_back(&node);
    }

    if (node.contains("left")) {
        CollectNodesByPrefixRecursive(node["left"], prefix, outNodes);
    }
    if (node.contains("right")) {
        CollectNodesByPrefixRecursive(node["right"], prefix, outNodes);
    }
    if (node.contains("children") && node["children"].is_array()) {
        for (const auto& child : node["children"]) {
            CollectNodesByPrefixRecursive(child, prefix, outNodes);
        }
    }
}

float GetTopOfFloor(const Moon::Building::BuildingDefinition& definition, int floorLevel) {
    const float base = Moon::Building::GetFloorBaseHeight(definition, floorLevel);
    for (const auto& floor : definition.floors) {
        if (floor.level == floorLevel) {
            return base + floor.floorHeight;
        }
    }
    return base;
}

float GetRenderedSlabThickness(const Moon::Building::GeneratedBuilding& building) {
    return building.floorPlates.empty() ? 0.05f : 0.18f;
}

float GetWallBaseHeight(const Moon::Building::GeneratedBuilding& building, int floorLevel) {
    return Moon::Building::GetFloorBaseHeight(building.definition, floorLevel) + GetRenderedSlabThickness(building);
}

float GetExpectedWindowBaseHeight(const Moon::Building::GeneratedBuilding& building,
                                  const Moon::Building::Window& window) {
    return GetWallBaseHeight(building, window.floorLevel) + window.sillHeight;
}

void ExpectBottomAndTopWithinStory(const json& node,
                                   float expectedBottomCm,
                                   float expectedTopLimitCm,
                                   float elementHeightCm,
                                   const std::string& path,
                                   const std::string& label) {
    ASSERT_TRUE(node.contains("transform"));
    ASSERT_TRUE(node["transform"].contains("position"));
    const float actualBottomCm = node["transform"]["position"][1].get<float>();

    EXPECT_NEAR(actualBottomCm, expectedBottomCm, 0.25f)
        << label << " bottom height mismatch for " << path;
    EXPECT_LE(actualBottomCm + elementHeightCm, expectedTopLimitCm + 0.25f)
        << label << " pierces upper slab/story limit for " << path;
}

struct StoryBand {
    float bottomCm = 0.0f;
    float topLimitCm = 0.0f;
};

const StoryBand* FindMatchingStoryBand(const std::vector<StoryBand>& bands, float actualBottomCm) {
    for (const StoryBand& band : bands) {
        if (std::abs(band.bottomCm - actualBottomCm) <= 0.25f) {
            return &band;
        }
    }
    return nullptr;
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
        EXPECT_TRUE(pipeline.ProcessBuilding(content, building, error))
            << "Building asset failed: " << error;
    }
}

TEST(BuildingAssetPresetTests, AllBuildingJsonFilesPassQualityChecks) {
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
        ASSERT_TRUE(pipeline.ProcessBuilding(content, building, error))
            << "Building asset failed pipeline: " << error;

        const auto report = Moon::Building::EvaluateBuildingQuality(building);
        std::ostringstream issues;
        for (const auto& qualityError : report.errors) {
            issues << "[" << qualityError.code << "] " << qualityError.message
                   << " floor=" << qualityError.floorLevel << "\n";
        }

        EXPECT_TRUE(report.passed) << issues.str();
        EXPECT_TRUE(report.errors.empty()) << issues.str();
    }
}

TEST(BuildingAssetPresetTests, AllBuildingJsonFilesExportWallsAndOpeningsWithinStoryBounds) {
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
        ASSERT_TRUE(pipeline.ProcessBuilding(content, building, error))
            << "Building asset failed pipeline: " << error;

        const std::string blueprintJson = Moon::Building::BuildingToObjectBlueprintConverter::Convert(building);
        ASSERT_FALSE(blueprintJson.empty());

        const json rootNode = json::parse(blueprintJson);
        ASSERT_TRUE(rootNode.contains("root"));

        std::vector<StoryBand> wallBands;
        wallBands.reserve(building.walls.size());
        for (const Moon::Building::WallSegment& wall : building.walls) {
            wallBands.push_back({
                GetWallBaseHeight(building, wall.floorLevel) * 100.0f,
                GetTopOfFloor(building.definition, wall.floorLevel) * 100.0f
            });
        }

        std::vector<StoryBand> doorBands;
        doorBands.reserve(building.doors.size());
        for (const Moon::Building::Door& door : building.doors) {
            doorBands.push_back({
                GetWallBaseHeight(building, door.floorLevel) * 100.0f,
                GetTopOfFloor(building.definition, door.floorLevel) * 100.0f
            });
        }

        std::vector<StoryBand> windowBands;
        windowBands.reserve(building.windows.size());
        for (const Moon::Building::Window& window : building.windows) {
            windowBands.push_back({
                GetExpectedWindowBaseHeight(building, window) * 100.0f,
                GetTopOfFloor(building.definition, window.floorLevel) * 100.0f
            });
        }

        std::vector<const json*> wallNodes;
        std::vector<const json*> doorNodes;
        std::vector<const json*> windowNodes;
        CollectNodesByPrefixRecursive(rootNode["root"], "wall_panel_", wallNodes);
        CollectNodesByPrefixRecursive(rootNode["root"], "door_", doorNodes);
        CollectNodesByPrefixRecursive(rootNode["root"], "window_", windowNodes);

        if (!building.walls.empty()) {
            EXPECT_FALSE(wallNodes.empty()) << path;
        }
        if (!building.doors.empty()) {
            EXPECT_FALSE(doorNodes.empty()) << path;
        }
        if (!building.windows.empty()) {
            EXPECT_FALSE(windowNodes.empty()) << path;
        }

        for (const json* node : wallNodes) {
            ASSERT_TRUE(node->contains("transform"));
            const float actualBottomCm = (*node)["transform"]["position"][1].get<float>();
            const StoryBand* band = FindMatchingStoryBand(wallBands, actualBottomCm);
            ASSERT_NE(band, nullptr) << "Wall base height does not align to any story band for " << path;

            float heightCm = 0.0f;
            if (node->contains("size")) {
                heightCm = (*node)["size"][1].get<float>();
            } else if (node->contains("overrides") && (*node)["overrides"].contains("h")) {
                heightCm = (*node)["overrides"]["h"].get<float>();
            }
            ExpectBottomAndTopWithinStory(*node, band->bottomCm, band->topLimitCm, heightCm, path, "Wall");
        }

        for (const json* node : doorNodes) {
            ASSERT_TRUE(node->contains("transform"));
            const float actualBottomCm = (*node)["transform"]["position"][1].get<float>();
            const StoryBand* band = FindMatchingStoryBand(doorBands, actualBottomCm);
            ASSERT_NE(band, nullptr) << "Door base height does not align to any story band for " << path;

            const float heightCm = node->contains("overrides") && (*node)["overrides"].contains("door_height")
                ? (*node)["overrides"]["door_height"].get<float>()
                : 0.0f;
            ExpectBottomAndTopWithinStory(*node, band->bottomCm, band->topLimitCm, heightCm, path, "Door");
        }

        for (const json* node : windowNodes) {
            ASSERT_TRUE(node->contains("transform"));
            const float actualBottomCm = (*node)["transform"]["position"][1].get<float>();
            const StoryBand* band = FindMatchingStoryBand(windowBands, actualBottomCm);
            ASSERT_NE(band, nullptr) << "Window base height does not align to any story band for " << path;

            const float heightCm = node->contains("overrides") && (*node)["overrides"].contains("h")
                ? (*node)["overrides"]["h"].get<float>()
                : 0.0f;
            ExpectBottomAndTopWithinStory(*node, band->bottomCm, band->topLimitCm, heightCm, path, "Window");
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
