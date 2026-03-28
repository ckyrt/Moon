#pragma once

#include "../Math/Quaternion.h"
#include "../Math/Vector3.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Moon {
namespace Object {

enum class NodeType {
    Primitive,
    Csg,
    Group,
    Reference,
    Light,
    Stair
};

enum class PrimitiveType {
    Cube,
    Sphere,
    Cylinder,
    Capsule,
    Cone,
    Torus
};

enum class CsgOp {
    Union,
    Subtract,
    Intersect
};

enum class GroupOutputMode {
    Separate,
    Merge
};

struct ValueExpr {
    enum class Kind {
        Constant,
        ParamRef,
        Expression
    };

    Kind kind;
    float constantValue;
    std::string paramName;
    std::string expression;

    static ValueExpr Constant(float value) {
        ValueExpr expr;
        expr.kind = Kind::Constant;
        expr.constantValue = value;
        return expr;
    }

    static ValueExpr ParamRef(const std::string& name) {
        ValueExpr expr;
        expr.kind = Kind::ParamRef;
        expr.paramName = name;
        return expr;
    }

    static ValueExpr Expression(const std::string& exprStr) {
        ValueExpr expr;
        expr.kind = Kind::Expression;
        expr.expression = exprStr;
        return expr;
    }

    ValueExpr()
        : kind(Kind::Constant)
        , constantValue(0.0f) {
    }
};

struct TransformTRS {
    ValueExpr positionX, positionY, positionZ;
    ValueExpr rotationX, rotationY, rotationZ;
    ValueExpr scaleX, scaleY, scaleZ;

    TransformTRS()
        : positionX(ValueExpr::Constant(0.0f))
        , positionY(ValueExpr::Constant(0.0f))
        , positionZ(ValueExpr::Constant(0.0f))
        , rotationX(ValueExpr::Constant(0.0f))
        , rotationY(ValueExpr::Constant(0.0f))
        , rotationZ(ValueExpr::Constant(0.0f))
        , scaleX(ValueExpr::Constant(1.0f))
        , scaleY(ValueExpr::Constant(1.0f))
        , scaleZ(ValueExpr::Constant(1.0f)) {
    }
};

struct ParameterDef {
    enum class Type {
        Float,
        Int,
        Bool
    };

    Type type;
    float defaultValue;
    float minValue;
    float maxValue;

    ParameterDef()
        : type(Type::Float)
        , defaultValue(0.0f)
        , minValue(-1e6f)
        , maxValue(1e6f) {
    }
};

struct CsgOptions {
    std::string solver;
    float weldEpsilon;
    bool recomputeNormals;

    CsgOptions()
        : solver("fast")
        , weldEpsilon(1e-5f)
        , recomputeNormals(true) {
    }
};

struct Node;

struct PrimitiveNode {
    PrimitiveType primitive;
    std::unordered_map<std::string, ValueExpr> params;
    TransformTRS localTransform;
    std::string material;

    PrimitiveNode()
        : primitive(PrimitiveType::Cube) {
    }
};

struct CsgNode {
    CsgOp operation;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
    CsgOptions options;

    CsgNode()
        : operation(CsgOp::Union) {
    }
};

struct AttachDef {
    std::string selfAnchor;
    std::string targetPath;
    std::string targetAnchor;
    bool hasAttach = false;
};

struct GroupNode {
    std::vector<std::unique_ptr<Node>> children;
    std::vector<std::string> childNames;
    GroupOutputMode outputMode;
    TransformTRS localTransform;

    GroupNode()
        : outputMode(GroupOutputMode::Separate) {
    }
};

struct RefNode {
    std::string refId;
    TransformTRS localTransform;
    std::unordered_map<std::string, ValueExpr> overrides;
    AttachDef attach;
};

struct LightNode {
    enum class Type {
        Directional,
        Point,
        Spot
    };

    Type type = Type::Point;
    TransformTRS localTransform;
    ValueExpr colorR = ValueExpr::Constant(1.0f);
    ValueExpr colorG = ValueExpr::Constant(1.0f);
    ValueExpr colorB = ValueExpr::Constant(1.0f);
    ValueExpr intensity = ValueExpr::Constant(1.0f);
    ValueExpr range = ValueExpr::Constant(200.0f);
    ValueExpr attenuationConstant = ValueExpr::Constant(1.0f);
    ValueExpr attenuationLinear = ValueExpr::Constant(0.0f);
    ValueExpr attenuationQuadratic = ValueExpr::Constant(0.0f);
    ValueExpr spotInnerConeAngle = ValueExpr::Constant(15.0f);
    ValueExpr spotOuterConeAngle = ValueExpr::Constant(30.0f);
    bool castShadows = false;
};

struct StairNode {
    std::unordered_map<std::string, ValueExpr> params;
    TransformTRS localTransform;
    std::string treadMaterial;
    std::string stringerMaterial;
    std::string railMaterial;
    bool leftRail = true;
    bool rightRail = true;
};

struct Node {
    NodeType type;

    union {
        PrimitiveNode* primitive;
        CsgNode* csg;
        GroupNode* group;
        RefNode* ref;
        LightNode* light;
        StairNode* stair;
    } data;

    Node()
        : type(NodeType::Primitive) {
        data.primitive = nullptr;
    }

    explicit Node(NodeType t)
        : type(t) {
        data.primitive = nullptr;
        switch (t) {
        case NodeType::Primitive: data.primitive = new PrimitiveNode(); break;
        case NodeType::Csg: data.csg = new CsgNode(); break;
        case NodeType::Group: data.group = new GroupNode(); break;
        case NodeType::Reference: data.ref = new RefNode(); break;
        case NodeType::Light: data.light = new LightNode(); break;
        case NodeType::Stair: data.stair = new StairNode(); break;
        }
    }

    ~Node() {
        switch (type) {
        case NodeType::Primitive: delete data.primitive; break;
        case NodeType::Csg: delete data.csg; break;
        case NodeType::Group: delete data.group; break;
        case NodeType::Reference: delete data.ref; break;
        case NodeType::Light: delete data.light; break;
        case NodeType::Stair: delete data.stair; break;
        }
    }

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    Node(Node&& other) noexcept
        : type(other.type)
        , data(other.data) {
        other.data.primitive = nullptr;
    }

    Node& operator=(Node&& other) noexcept {
        if (this != &other) {
            this->~Node();
            type = other.type;
            data = other.data;
            other.data.primitive = nullptr;
        }
        return *this;
    }

    PrimitiveNode* AsPrimitive() { return type == NodeType::Primitive ? data.primitive : nullptr; }
    CsgNode* AsCsg() { return type == NodeType::Csg ? data.csg : nullptr; }
    GroupNode* AsGroup() { return type == NodeType::Group ? data.group : nullptr; }
    RefNode* AsRef() { return type == NodeType::Reference ? data.ref : nullptr; }
    LightNode* AsLight() { return type == NodeType::Light ? data.light : nullptr; }
    StairNode* AsStair() { return type == NodeType::Stair ? data.stair : nullptr; }
};

struct BlueprintMetadata {
    std::string id;
    std::string category;
    std::vector<std::string> tags;
    int schemaVersion;

    BlueprintMetadata()
        : schemaVersion(1) {
    }
};

} // namespace Object
} // namespace Moon
