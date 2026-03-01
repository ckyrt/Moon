#pragma once

#include "../Math/Vector3.h"
#include "../Math/Quaternion.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Moon {
namespace CSG {

/**
 * @brief Node 类型枚举
 */
enum class NodeType {
    Primitive,   // 基础几何体
    Csg,         // 布尔运算
    Group,       // 组节点
    Reference,   // 引用其他 Blueprint
    Light        // 光源节点（不产生 mesh，输出 LightItem）
};

/**
 * @brief 基础几何体类型
 */
enum class PrimitiveType {
    Cube,
    Sphere,
    Cylinder,
    Capsule,
    Cone,
    Torus
};

/**
 * @brief CSG 运算类型（与 CSGOperations.h 的 Operation 对应）
 */
enum class CsgOp {
    Union,      // 并集
    Subtract,   // 差集
    Intersect   // 交集
};

/**
 * @brief Group 输出模式
 */
enum class GroupOutputMode {
    Separate,   // 输出多个独立 mesh（推荐）
    Merge       // 合并为单个 mesh（后续实现）
};

/**
 * @brief 参数值表达式
 * 
 * 支持：
 * - 常量: 0.5
 * - 变量引用: "$wheel_radius"
 * - 表达式: "$radius - $thickness", "$width * 2", "10 + $offset"
 */
struct ValueExpr {
    enum class Kind {
        Constant,   // 数字常量
        ParamRef,   // 参数引用 "$name"
        Expression  // 表达式字符串（运行时解析）
    };

    Kind kind;
    float constantValue;
    std::string paramName;
    std::string expression; // 存储原始表达式字符串

    // 构造常量
    static ValueExpr Constant(float value) {
        ValueExpr expr;
        expr.kind = Kind::Constant;
        expr.constantValue = value;
        return expr;
    }

    // 构造参数引用
    static ValueExpr ParamRef(const std::string& name) {
        ValueExpr expr;
        expr.kind = Kind::ParamRef;
        expr.paramName = name;
        return expr;
    }

    // 构造表达式
    static ValueExpr Expression(const std::string& exprStr) {
        ValueExpr expr;
        expr.kind = Kind::Expression;
        expr.expression = exprStr;
        return expr;
    }

    ValueExpr() : kind(Kind::Constant), constantValue(0.0f) {}
};

/**
 * @brief Transform (Position, Rotation, Scale)
 * 支持参数化：每个分量可以是常量或参数引用
 */
struct TransformTRS {
    ValueExpr positionX, positionY, positionZ;
    ValueExpr rotationX, rotationY, rotationZ; // Euler angles in degrees
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
        , scaleZ(ValueExpr::Constant(1.0f))
    {}
};

/**
 * @brief 参数定义
 */
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
        , maxValue(1e6f)
    {}
};

/**
 * @brief CSG 运算选项
 */
struct CsgOptions {
    std::string solver;         // "fast" 或 "precise"
    float weldEpsilon;          // 顶点合并容差
    bool recomputeNormals;      // 是否重新计算法线

    CsgOptions()
        : solver("fast")
        , weldEpsilon(1e-5f)
        , recomputeNormals(true)
    {}
};

// 前向声明
struct Node;

/**
 * @brief Primitive 节点数据
 */
struct PrimitiveNode {
    PrimitiveType primitive;
    std::unordered_map<std::string, ValueExpr> params;
    TransformTRS localTransform;
    std::string material;

    PrimitiveNode() : primitive(PrimitiveType::Cube) {}
};

/**
 * @brief CSG 节点数据
 */
struct CsgNode {
    CsgOp operation;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
    CsgOptions options;

    CsgNode() : operation(CsgOp::Union) {}
};

/**
 * @brief Attach 约束定义
 *
 * 指定"把本节点的 self_anchor 对齐到 target_path 节点的 target_anchor"。
 * v1 限制：target_path 只支持 "../<name>"（同一 group 内的已命名 sibling，
 * 且 target 必须出现在 children 列表的前面）。
 */
struct AttachDef {
    std::string selfAnchor;     // 本节点要对齐的 anchor 名称，例如 "top_center"
    std::string targetPath;     // 目标路径，例如 "../table_top"
    std::string targetAnchor;   // 目标节点的 anchor 名称，例如 "bottom_center"
    // v1 offset 固定为 (0,0,0)，后续可扩展为 ValueExpr
    bool hasAttach = false;
};

/**
 * @brief Group 节点数据
 */
struct GroupNode {
    std::vector<std::unique_ptr<Node>> children;
    std::vector<std::string> childNames;        // 与 children 一一对应，空字符串表示无名
    GroupOutputMode outputMode;

    GroupNode() : outputMode(GroupOutputMode::Separate) {}
};

/**
 * @brief Reference 节点数据
 */
struct RefNode {
    std::string refId;                                      // 引用的 Blueprint ID
    TransformTRS localTransform;
    std::unordered_map<std::string, ValueExpr> overrides;   // 参数覆盖
    AttachDef attach;                                       // attach 约束（可选）

    RefNode() = default;
};

/**
 * @brief Light 节点数据
 *
 * Light 节点本身不参与 CSG/mesh 运算，只在 BuildResult.lights 输出。
 *
 * 约定：
 * - position 单位为 cm（与其他节点一致），Build 时会转换为米。
 * - intensity 为无单位标量（沿用 Light 组件语义）。
 * - range 单位为 cm（Build 时转换为米）。
 */
struct LightNode {
    enum class Type {
        Directional,
        Point,
        Spot
    };

    Type type = Type::Point;
    TransformTRS localTransform;

    // 基本属性
    ValueExpr colorR = ValueExpr::Constant(1.0f);
    ValueExpr colorG = ValueExpr::Constant(1.0f);
    ValueExpr colorB = ValueExpr::Constant(1.0f);
    ValueExpr intensity = ValueExpr::Constant(1.0f);

    // Point/Spot
    ValueExpr range = ValueExpr::Constant(200.0f); // cm
    ValueExpr attenuationConstant  = ValueExpr::Constant(1.0f);
    ValueExpr attenuationLinear    = ValueExpr::Constant(0.0f);
    ValueExpr attenuationQuadratic = ValueExpr::Constant(0.0f);

    // Spot
    ValueExpr spotInnerConeAngle = ValueExpr::Constant(15.0f); // degrees
    ValueExpr spotOuterConeAngle = ValueExpr::Constant(30.0f); // degrees

    bool castShadows = false;
};

/**
 * @brief 通用 Node 结构
 */
struct Node {
    NodeType type;
    
    // 使用指针存储不同类型的节点数据
    union {
        PrimitiveNode* primitive;
        CsgNode* csg;
        GroupNode* group;
        RefNode* ref;
        LightNode* light;
    } data;

    Node() : type(NodeType::Primitive) {
        data.primitive = nullptr;
    }
    
    explicit Node(NodeType t) : type(t) {
        data.primitive = nullptr;
        switch (t) {
            case NodeType::Primitive: data.primitive = new PrimitiveNode(); break;
            case NodeType::Csg: data.csg = new CsgNode(); break;
            case NodeType::Group: data.group = new GroupNode(); break;
            case NodeType::Reference: data.ref = new RefNode(); break;
            case NodeType::Light: data.light = new LightNode(); break;
        }
    }
    
    ~Node() {
        switch (type) {
            case NodeType::Primitive: delete data.primitive; break;
            case NodeType::Csg: delete data.csg; break;
            case NodeType::Group: delete data.group; break;
            case NodeType::Reference: delete data.ref; break;
            case NodeType::Light: delete data.light; break;
        }
    }
    
    // 禁止拷贝，允许移动
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&& other) noexcept : type(other.type), data(other.data) {
        other.data.primitive = nullptr;
    }
    Node& operator=(Node&& other) noexcept {
        if (this != &other) {
            // 清理当前数据
            this->~Node();
            type = other.type;
            data = other.data;
            other.data.primitive = nullptr;
        }
        return *this;
    }

    // 辅助访问方法
    PrimitiveNode* AsPrimitive() {
        return type == NodeType::Primitive ? data.primitive : nullptr;
    }
    CsgNode* AsCsg() {
        return type == NodeType::Csg ? data.csg : nullptr;
    }
    GroupNode* AsGroup() {
        return type == NodeType::Group ? data.group : nullptr;
    }
    RefNode* AsRef() {
        return type == NodeType::Reference ? data.ref : nullptr;
    }

    LightNode* AsLight() {
        return type == NodeType::Light ? data.light : nullptr;
    }
};

/**
 * @brief Blueprint 元数据
 */
struct BlueprintMetadata {
    std::string id;
    std::string category;
    std::vector<std::string> tags;
    int schemaVersion;

    BlueprintMetadata() : schemaVersion(1) {}
};

} // namespace CSG
} // namespace Moon
