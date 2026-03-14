#pragma once

#include "../../external/nlohmann/json.hpp"
#include <array>
#include <string>
#include <vector>

namespace Moon {
namespace Massing {

using json = nlohmann::json;

enum class RuleNodeType {
    Primitive,
    Extrude,
    Revolve,
    Sweep,
    Loft,
    Csg,
    Deform,
    Array,
    Group
};

enum class PrimitiveType {
    Cube,
    Sphere,
    Cylinder,
    Capsule,
    Cone,
    Torus
};

enum class CsgOperation {
    Union,
    Subtract,
    Intersect
};

using Vec2 = std::array<float, 2>;
using Vec3 = std::array<float, 3>;

struct RuleTransform {
    Vec3 position {0.0f, 0.0f, 0.0f};
    Vec3 rotation {0.0f, 0.0f, 0.0f};
    Vec3 scale {1.0f, 1.0f, 1.0f};
};

struct Curve2D {
    std::string id;
    bool closed = false;
    std::vector<Vec2> points;
};

struct Curve3D {
    std::string id;
    bool closed = false;
    std::vector<Vec3> points;
};

struct RuleNode {
    std::string id;
    std::string name;
    RuleNodeType type = RuleNodeType::Primitive;
    PrimitiveType primitive = PrimitiveType::Cube;
    CsgOperation csgOperation = CsgOperation::Union;
    RuleTransform transform;
    std::string material;
    json params = json::object();
    json editor = json::object();
    std::vector<Curve2D> profiles;
    std::vector<Curve3D> paths;
    std::vector<RuleNode> children;
};

struct RuleSet {
    int schemaVersion = 1;
    int version = 1;
    std::string name;
    std::string description;
    std::string unit = "meter";
    json ai = json::object();
    json editor = json::object();
    RuleNode root;
};

const char* ToString(RuleNodeType type);
const char* ToString(PrimitiveType type);
const char* ToString(CsgOperation op);

bool TryParseRuleNodeType(const std::string& value, RuleNodeType& outType);
bool TryParsePrimitiveType(const std::string& value, PrimitiveType& outType);
bool TryParseCsgOperation(const std::string& value, CsgOperation& outOperation);

} // namespace Massing
} // namespace Moon
