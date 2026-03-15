#include <gtest/gtest.h>

#include "../MassMeshBuilder.h"
#include "../MassRuleParser.h"
#include "../../core/Assets/AssetPaths.h"
#include "../../core/Mesh/Mesh.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_map>

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

uint64_t QuantizePosition(const Moon::Vector3& position) {
    const auto quantize = [](float value) -> uint32_t {
        return static_cast<uint32_t>(std::lround((value + 2048.0f) * 1000.0f));
    };
    return (static_cast<uint64_t>(quantize(position.x)) << 42) ^
           (static_cast<uint64_t>(quantize(position.y)) << 21) ^
           static_cast<uint64_t>(quantize(position.z));
}

bool HasSplitNormalsAtSharedPosition(const Moon::Mesh& mesh, float maxDot = 0.8f) {
    std::unordered_map<uint64_t, std::vector<Moon::Vector3>> normalsByPosition;
    for (const Moon::Vertex& vertex : mesh.GetVertices()) {
        normalsByPosition[QuantizePosition(vertex.position)].push_back(vertex.normal);
    }

    for (const auto& pair : normalsByPosition) {
        const std::vector<Moon::Vector3>& normals = pair.second;
        for (size_t i = 0; i < normals.size(); ++i) {
            for (size_t j = i + 1; j < normals.size(); ++j) {
                if (Moon::Vector3::Dot(normals[i], normals[j]) < maxDot) {
                    return true;
                }
            }
        }
    }
    return false;
}

float AverageOutwardDot(const Moon::Mesh& mesh) {
    const Bounds3 bounds = ComputeBounds(mesh);
    const Moon::Vector3 center(
        0.5f * (bounds.min.x + bounds.max.x),
        0.5f * (bounds.min.y + bounds.max.y),
        0.5f * (bounds.min.z + bounds.max.z)
    );

    float dotSum = 0.0f;
    size_t dotCount = 0;
    for (const Moon::Vertex& vertex : mesh.GetVertices()) {
        Moon::Vector3 outward = vertex.position - center;
        const float outwardLength = outward.Length();
        if (outwardLength <= 0.001f) {
            continue;
        }
        outward = outward * (1.0f / outwardLength);
        dotSum += Moon::Vector3::Dot(vertex.normal, outward);
        ++dotCount;
    }

    return dotCount > 0 ? dotSum / static_cast<float>(dotCount) : 0.0f;
}

uint32_t CountBoundaryEdges(const Moon::Mesh& mesh) {
    std::unordered_map<uint64_t, uint32_t> edgeUseCount;
    const std::vector<Moon::Vertex>& vertices = mesh.GetVertices();
    const std::vector<uint32_t>& indices = mesh.GetIndices();
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const uint64_t tri[3] = {
            QuantizePosition(vertices[indices[i + 0]].position),
            QuantizePosition(vertices[indices[i + 1]].position),
            QuantizePosition(vertices[indices[i + 2]].position)
        };
        for (int edge = 0; edge < 3; ++edge) {
            const uint64_t a = tri[edge];
            const uint64_t b = tri[(edge + 1) % 3];
            const uint64_t lo = std::min(a, b);
            const uint64_t hi = std::max(a, b);
            const uint64_t key = lo ^ (hi << 1);
            edgeUseCount[key] += 1;
        }
    }

    uint32_t boundaryEdges = 0;
    for (const auto& pair : edgeUseCount) {
        if (pair.second == 1) {
            boundaryEdges += 1;
        }
    }
    return boundaryEdges;
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
    EXPECT_EQ(CountBoundaryEdges(*result.items.front().mesh), 0u);
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
    EXPECT_EQ(CountBoundaryEdges(*result.items.front().mesh), 0u);
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
    EXPECT_EQ(CountBoundaryEdges(*result.items.front().mesh), 0u);
}

TEST(MassMeshBuilderTests, ExtrudeSplitsNormalsAcrossHardEdges) {
    const RuleSet ruleSet = ParseRuleSetOrFail(R"({
      "version": 1,
      "root": {
        "type": "extrude",
        "params": { "height": 12 },
        "profiles": [
          { "closed": true, "points": [[-4, -4], [4, -4], [4, 4], [-4, 4]] }
        ]
      }
    })");

    MassBuildResult result = BuildRuleSetOrFail(ruleSet);
    ASSERT_EQ(result.items.size(), 1u);
    const std::shared_ptr<Moon::Mesh>& mesh = result.items.front().mesh;
    ExpectMeshLooksValid(mesh);

    EXPECT_GT(mesh->GetVertexCount(), 8u);
    EXPECT_TRUE(HasSplitNormalsAtSharedPosition(*mesh));
}

TEST(MassMeshBuilderTests, LoftAddsIntermediateRingsByDefault) {
    const RuleSet ruleSet = ParseRuleSetOrFail(R"({
      "version": 1,
      "root": {
        "type": "loft",
        "params": { "levels": [0, 18, 36] },
        "profiles": [
          { "closed": true, "points": [[-8, -8], [8, -8], [8, 8], [-8, 8]] },
          { "closed": true, "points": [[-6, -9], [9, -6], [6, 9], [-9, 6]] },
          { "closed": true, "points": [[-4, -7], [7, -4], [4, 7], [-7, 4]] }
        ]
      }
    })");

    MassBuildResult result = BuildRuleSetOrFail(ruleSet);
    ASSERT_EQ(result.items.size(), 1u);
    const std::shared_ptr<Moon::Mesh>& mesh = result.items.front().mesh;
    ExpectMeshLooksValid(mesh);

    EXPECT_GT(mesh->GetVertexCount(), 40u);
    EXPECT_EQ(CountBoundaryEdges(*mesh), 0u);
}

TEST(MassMeshBuilderTests, LoftNormalsPointOutwardOnAverage) {
    const RuleSet ruleSet = ParseRuleSetOrFail(R"({
      "version": 1,
      "root": {
        "type": "loft",
        "params": {
          "levels": [0, 18, 36],
          "segments_per_span": 6
        },
        "profiles": [
          { "closed": true, "points": [[-8, -6], [-6, -8], [6, -8], [8, -6], [8, 6], [6, 8], [-6, 8], [-8, 6]] },
          { "closed": true, "points": [[-7.2, -5.6], [-5.2, -8.7], [7.0, -9.0], [9.0, -6.1], [7.0, 7.4], [5.0, 9.0], [-7.0, 8.5], [-9.1, 5.8]] },
          { "closed": true, "points": [[-5.8, -4.4], [-4.0, -6.7], [5.4, -7.0], [7.0, -4.8], [5.8, 5.8], [3.8, 7.1], [-5.2, 6.8], [-7.1, 4.7]] }
        ]
      }
    })");

    MassBuildResult result = BuildRuleSetOrFail(ruleSet);
    ASSERT_EQ(result.items.size(), 1u);
    const std::shared_ptr<Moon::Mesh>& mesh = result.items.front().mesh;
    ExpectMeshLooksValid(mesh);

    EXPECT_GT(AverageOutwardDot(*mesh), 0.2f);
}

TEST(MassMeshBuilderTests, DeformTwistSubdividesBeforeWarping) {
    const RuleSet ruleSet = ParseRuleSetOrFail(R"({
      "version": 1,
      "root": {
        "type": "deform",
        "params": { "mode": "twist", "angle": 42, "subdivide": 2 },
        "children": [
          {
            "type": "extrude",
            "params": { "height": 28 },
            "profiles": [
              { "closed": true, "points": [[-5, -5], [5, -5], [5, 5], [-5, 5]] }
            ]
          }
        ]
      }
    })");

    MassBuildResult result = BuildRuleSetOrFail(ruleSet);
    ASSERT_EQ(result.items.size(), 1u);
    const std::shared_ptr<Moon::Mesh>& mesh = result.items.front().mesh;
    ExpectMeshLooksValid(mesh);

    EXPECT_GT(mesh->GetVertexCount(), 24u);
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



