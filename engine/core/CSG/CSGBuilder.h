#pragma once

#include "../Geometry/Path.h"
#include "../Mesh/Mesh.h"
#include "../Object/Blueprint.h"
#include "../Object/BlueprintTypes.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Moon {
namespace CSG {

class ParameterScope {
public:
    ParameterScope()
        : m_parent(nullptr) {
    }

    explicit ParameterScope(ParameterScope* parent)
        : m_parent(parent) {
    }

    void SetValue(const std::string& name, float value) {
        m_values[name] = value;
    }

    bool GetValue(const std::string& name, float& outValue) const {
        auto it = m_values.find(name);
        if (it != m_values.end()) {
            outValue = it->second;
            return true;
        }
        return m_parent ? m_parent->GetValue(name, outValue) : false;
    }

    ParameterScope CreateChild() {
        return ParameterScope(this);
    }

private:
    ParameterScope* m_parent;
    std::unordered_map<std::string, float> m_values;
};

struct ResolvedTransform {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;

    ResolvedTransform()
        : position(0, 0, 0)
        , rotation(Quaternion::Identity())
        , scale(1, 1, 1) {
    }

    ResolvedTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scl)
        : position(pos)
        , rotation(rot)
        , scale(scl) {
    }
};

struct MeshItem {
    std::shared_ptr<Mesh> mesh;
    std::string material;
    ResolvedTransform worldTransform;
    bool requiresFlatShading = true;

    MeshItem() = default;
    MeshItem(std::shared_ptr<Mesh> m, const std::string& mat, const ResolvedTransform& trans,
             bool flatShade = true)
        : mesh(m)
        , material(mat)
        , worldTransform(trans)
        , requiresFlatShading(flatShade) {
    }
};

struct LightItem {
    enum class Type {
        Directional,
        Point,
        Spot
    };

    Type type = Type::Point;
    Vector3 color = Vector3(1, 1, 1);
    float intensity = 1.0f;
    float range = 0.0f;
    Vector3 attenuation = Vector3(1.0f, 0.0f, 0.0f);
    float spotInnerConeAngle = 15.0f;
    float spotOuterConeAngle = 30.0f;
    bool castShadows = false;
    ResolvedTransform worldTransform;
};

struct BuildResult {
    std::vector<MeshItem> meshes;
    std::vector<LightItem> lights;

    void AddMesh(const MeshItem& item) {
        meshes.push_back(item);
    }

    void AddLight(const LightItem& item) {
        lights.push_back(item);
    }

    void Merge(BuildResult&& other) {
        meshes.insert(meshes.end(),
            std::make_move_iterator(other.meshes.begin()),
            std::make_move_iterator(other.meshes.end()));
        lights.insert(lights.end(),
            std::make_move_iterator(other.lights.begin()),
            std::make_move_iterator(other.lights.end()));
    }
};

class CSGBuilder {
public:
    CSGBuilder();
    ~CSGBuilder();

    void SetBlueprintDatabase(Object::BlueprintDatabase* db) {
        m_database = db;
    }

    BuildResult Build(const Object::Blueprint* blueprint,
                      const std::unordered_map<std::string, float>& parameterOverrides,
                      std::string& outError);

private:
    BuildResult BuildNode(const Object::Node* node, ParameterScope& scope, std::string& outError);
    BuildResult BuildPrimitive(const Object::PrimitiveNode* prim, ParameterScope& scope, std::string& outError);
    BuildResult BuildCSG(const Object::CsgNode* csg, ParameterScope& scope, std::string& outError);
    BuildResult BuildGroup(const Object::GroupNode* group, ParameterScope& scope, std::string& outError);
    BuildResult BuildReference(const Object::RefNode* ref, ParameterScope& scope, std::string& outError);
    BuildResult BuildLight(const Object::LightNode* light, ParameterScope& scope, std::string& outError);
    BuildResult BuildStair(const Object::StairNode* stair, ParameterScope& scope, std::string& outError);

    std::unordered_map<std::string, Vector3> EvaluateAnchors(
        const Object::Blueprint* blueprint, ParameterScope& scope, std::string& outError);
    float EvaluateStringExpr(const std::string& exprStr, ParameterScope& scope, std::string& outError);
    float ResolveValue(const Object::ValueExpr& expr, ParameterScope& scope, std::string& outError);
    float EvaluateExpression(const std::string& exprStr, ParameterScope& scope, std::string& outError);
    void ResolveTransform(const Object::TransformTRS& transformExpr, ParameterScope& scope,
                          Vector3& outPosition, Quaternion& outRotation, Vector3& outScale,
                          std::string& outError);
    void ApplyTransform(BuildResult& result, const ResolvedTransform& transform) const;

    Object::BlueprintDatabase* m_database;
    int m_buildDepth = 0;
};

} // namespace CSG
} // namespace Moon
