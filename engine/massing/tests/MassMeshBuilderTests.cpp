#include <gtest/gtest.h>

#include "../MassMeshBuilder.h"
#include "../MassRuleParser.h"
#include "../../core/Assets/AssetPaths.h"
#include "../../core/Mesh/Mesh.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <fstream>

using namespace Moon::Massing;

namespace {

struct Bounds3 {
    Moon::Vector3 min;
    Moon::Vector3 max;
};

Bounds3 ComputeBounds(const Moon::Mesh& mesh) {
    Bounds3 bounds{
        Moon::Vector3(FLT_MAX, FLT_MAX, FLT_MAX),
        Moon::Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX)
    };

    for (const Moon::Vertex& vertex : mesh.GetVertices()) {
        bounds.min.x = std::min(bounds.min.x, vertex.position.x);
        bounds.min.y = std::min(bounds.min.y, vertex.position.y);
        bounds.min.z = std::min(bounds.min.z, vertex.position.z);
        bounds.max.x = std::max(bounds.max.x, vertex.position.x);
        bounds.max.y = std::max(bounds.max.y, vertex.position.y);
        bounds.max.z = std::max(bounds.max.z, vertex.position.z);
    }

    return bounds;
}

std::string ReadUtf8TextFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (contents.size() >= 3 &&
        static_cast<unsigned char>(contents[0]) == 0xEF &&
        static_cast<unsigned char>(contents[1]) == 0xBB &&
        static_cast<unsigned char>(contents[2]) == 0xBF) {
        contents.erase(0, 3);
    }
    return contents;
}

RuleSet ParseRuleSetOrFail(const std::string& jsonString) {
    RuleSet ruleSet;
    std::string error;
    EXPECT_TRUE(MassRuleParser::ParseFromString(jsonString, ruleSet, error)) << error;
    return ruleSet;
}

MassBuildResult BuildRuleSetOrFail(const RuleSet& ruleSet) {
    MassBuildResult result;
    std::string error;
    EXPECT_TRUE(MassMeshBuilder::Build(ruleSet, result, error)) << error;
    EXPECT_FALSE(result.items.empty());
    return result;
}

void ExpectMeshLooksValid(const std::shared_ptr<Moon::Mesh>& mesh) {
    ASSERT_TRUE(mesh);
    EXPECT_TRUE(mesh->IsValid());
    EXPECT_GT(mesh->GetVertexCount(), 0u);
    EXPECT_GT(mesh->GetIndexCount(), 0u);
    EXPECT_EQ(mesh->GetIndexCount() % 3, 0u);

    for (const Moon::Vertex& vertex : mesh->GetVertices()) {
        EXPECT_TRUE(std::isfinite(vertex.position.x));
        EXPECT_TRUE(std::isfinite(vertex.position.y));
        EXPECT_TRUE(std::isfinite(vertex.position.z));
        EXPECT_TRUE(std::isfinite(vertex.normal.x));
        EXPECT_TRUE(std::isfinite(vertex.normal.y));
        EXPECT_TRUE(std::isfinite(vertex.normal.z));

        const float normalLength = vertex.normal.Length();
        EXPECT_NEAR(normalLength, 1.0f, 0.05f);
    }
}

uint64_t HashCombine(uint64_t seed, uint64_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

uint64_t HashFloat(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(float));
    return static_cast<uint64_t>(bits);
}

uint64_t HashMesh(const Moon::Mesh& mesh) {
    uint64_t hash = 1469598103934665603ULL;
    for (const Moon::Vertex& vertex : mesh.GetVertices()) {
        hash = HashCombine(hash, HashFloat(vertex.position.x));
        hash = HashCombine(hash, HashFloat(vertex.position.y));
        hash = HashCombine(hash, HashFloat(vertex.position.z));
        hash = HashCombine(hash, HashFloat(vertex.normal.x));
        hash = HashCombine(hash, HashFloat(vertex.normal.y));
        hash = HashCombine(hash, HashFloat(vertex.normal.z));
        hash = HashCombine(hash, HashFloat(vertex.uv.x));
        hash = HashCombine(hash, HashFloat(vertex.uv.y));
    }
    for (uint32_t index : mesh.GetIndices()) {
        hash = HashCombine(hash, static_cast<uint64_t>(index));
    }
    return hash;
}

} // namespace

TEST(MassRuleParserTests, SupportsInlineProfileAndHeight) {
    const char* jsonString = R"({
      "version": 1,
      "root": {
        "type": "extrude",
        "name": "TowerMass",
        "material": "concrete",
        "profile": {
          "closed": true,
          "points": [
            { "x": -8, "y": -8 },
            { "x": 8, "y": -8 },
            { "x": 8, "y": 8 },
            { "x": -8, "y": 8 }
          ]
        },
        "height": 36
      }
    })";

    RuleSet ruleSet = ParseRuleSetOrFail(jsonString);
    ASSERT_EQ(ruleSet.root.type, RuleNodeType::Extrude);
    ASSERT_EQ(ruleSet.root.profiles.size(), 1u);
    EXPECT_FLOAT_EQ(ruleSet.root.params["height"].get<float>(), 36.0f);
}

TEST(MassMeshBuilderTests, ExtrudeBuildMatchesRequestedHeight) {
    const RuleSet ruleSet = ParseRuleSetOrFail(R"({
      "version": 1,
      "root": {
        "type": "extrude",
        "params": { "height": 36 },
        "profiles": [
          { "closed": true, "points": [[-8, -8], [8, -8], [8, 8], [-8, 8]] }
        ]
      }
    })");

    MassBuildResult result = BuildRuleSetOrFail(ruleSet);
    ASSERT_EQ(result.items.size(), 1u);
    ExpectMeshLooksValid(result.items.front().mesh);

    const Bounds3 bounds = ComputeBounds(*result.items.front().mesh);
    EXPECT_NEAR(bounds.min.y, 0.0f, 0.001f);
    EXPECT_NEAR(bounds.max.y, 36.0f, 0.001f);
    EXPECT_NEAR(bounds.max.x - bounds.min.x, 16.0f, 0.001f);
    EXPECT_NEAR(bounds.max.z - bounds.min.z, 16.0f, 0.001f);
}

TEST(MassMeshBuilderTests, ArrayProducesExpectedInstanceCount) {
    const RuleSet ruleSet = ParseRuleSetOrFail(R"({
      "version": 1,
      "root": {
        "type": "array",
        "params": {
          "count_x": 3,
          "count_z": 2,
          "spacing_x": 10,
          "spacing_z": 12
        },
        "children": [
          {
            "type": "primitive",
            "primitive": "cube",
            "params": {
              "size_x": 2,
              "size_y": 4,
              "size_z": 2
            }
          }
        ]
      }
    })");

    MassBuildResult result = BuildRuleSetOrFail(ruleSet);
    EXPECT_EQ(result.items.size(), 6u);
    for (const MassBuildItem& item : result.items) {
        ExpectMeshLooksValid(item.mesh);
    }
}

TEST(MassMeshBuilderTests, SweepProducesVolumeAlongPath) {
    const RuleSet ruleSet = ParseRuleSetOrFail(R"({
      "version": 1,
      "root": {
        "type": "sweep",
        "profiles": [
          { "closed": true, "points": [[-1, -1], [1, -1], [1, 1], [-1, 1]] }
        ],
        "paths": [
          { "closed": false, "points": [[-10, 0, 0], [-5, 5, 0], [0, 8, 0], [5, 5, 0], [10, 0, 0]] }
        ]
      }
    })");

    MassBuildResult result = BuildRuleSetOrFail(ruleSet);
    ASSERT_EQ(result.items.size(), 1u);
    ExpectMeshLooksValid(result.items.front().mesh);

    const Bounds3 bounds = ComputeBounds(*result.items.front().mesh);
    EXPECT_GT(bounds.max.x - bounds.min.x, 18.0f);
    EXPECT_GT(bounds.max.y - bounds.min.y, 8.0f);
    EXPECT_GT(bounds.max.z - bounds.min.z, 1.5f);
}

TEST(MassMeshBuilderTests, LoftUsesLevelHeights) {
    const RuleSet ruleSet = ParseRuleSetOrFail(R"({
      "version": 1,
      "root": {
        "type": "loft",
        "params": { "levels": [0, 12, 30] },
        "profiles": [
          { "closed": true, "points": [[-8, -8], [8, -8], [8, 8], [-8, 8]] },
          { "closed": true, "points": [[-6, -7], [7, -6], [6, 7], [-7, 6]] },
          { "closed": true, "points": [[-4, -5], [5, -4], [4, 5], [-5, 4]] }
        ]
      }
    })");

    MassBuildResult result = BuildRuleSetOrFail(ruleSet);
    ASSERT_EQ(result.items.size(), 1u);
    ExpectMeshLooksValid(result.items.front().mesh);

    const Bounds3 bounds = ComputeBounds(*result.items.front().mesh);
    EXPECT_NEAR(bounds.min.y, 0.0f, 0.001f);
    EXPECT_NEAR(bounds.max.y, 30.0f, 0.001f);
}

TEST(MassMeshBuilderTests, AllSamplePresetsParseAndBuild) {
    const char* presetFiles[] = {
        "complex_twisted_tower.json",
        "extrude_box_tower.json",
        "extrude_courtyard.json",
        "revolve_dome.json",
        "sweep_arch.json",
        "loft_twist_tower.json",
        "deform_twisted_block.json",
        "array_tower_cluster.json",
        "csg_podium_tower.json"
    };

    for (const char* presetFile : presetFiles) {
        const std::string path = Moon::Assets::BuildAssetPath(std::string("massing/") + presetFile);
        SCOPED_TRACE(path);
        RuleSet ruleSet = ParseRuleSetOrFail(ReadUtf8TextFile(path));
        MassBuildResult result = BuildRuleSetOrFail(ruleSet);
        EXPECT_FALSE(result.items.empty());
        for (const MassBuildItem& item : result.items) {
            ExpectMeshLooksValid(item.mesh);
        }
    }
}

TEST(MassMeshBuilderTests, RepeatedBuildOfSameRuleSetIsDeterministic) {
    const std::string jsonString = ReadUtf8TextFile(Moon::Assets::BuildAssetPath("massing/complex_twisted_tower.json"));
    const RuleSet ruleSet = ParseRuleSetOrFail(jsonString);

    MassBuildResult first = BuildRuleSetOrFail(ruleSet);
    MassBuildResult second = BuildRuleSetOrFail(ruleSet);

    ASSERT_EQ(first.items.size(), second.items.size());
    for (size_t i = 0; i < first.items.size(); ++i) {
        ASSERT_TRUE(first.items[i].mesh);
        ASSERT_TRUE(second.items[i].mesh);
        EXPECT_EQ(first.items[i].mesh->GetVertexCount(), second.items[i].mesh->GetVertexCount());
        EXPECT_EQ(first.items[i].mesh->GetIndexCount(), second.items[i].mesh->GetIndexCount());
        EXPECT_EQ(HashMesh(*first.items[i].mesh), HashMesh(*second.items[i].mesh));
    }
}

