#include "MassRules.h"

namespace Moon {
namespace Massing {

const char* ToString(RuleNodeType type) {
    switch (type) {
        case RuleNodeType::Primitive: return "primitive";
        case RuleNodeType::Extrude: return "extrude";
        case RuleNodeType::Revolve: return "revolve";
        case RuleNodeType::Sweep: return "sweep";
        case RuleNodeType::Loft: return "loft";
        case RuleNodeType::Csg: return "csg";
        case RuleNodeType::Deform: return "deform";
        case RuleNodeType::Array: return "array";
        case RuleNodeType::Group: return "group";
        default: return "primitive";
    }
}

const char* ToString(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::Cube: return "cube";
        case PrimitiveType::Sphere: return "sphere";
        case PrimitiveType::Cylinder: return "cylinder";
        case PrimitiveType::Capsule: return "capsule";
        case PrimitiveType::Cone: return "cone";
        case PrimitiveType::Torus: return "torus";
        default: return "cube";
    }
}

const char* ToString(CsgOperation op) {
    switch (op) {
        case CsgOperation::Union: return "union";
        case CsgOperation::Subtract: return "subtract";
        case CsgOperation::Intersect: return "intersect";
        default: return "union";
    }
}

bool TryParseRuleNodeType(const std::string& value, RuleNodeType& outType) {
    if (value == "primitive") { outType = RuleNodeType::Primitive; return true; }
    if (value == "extrude") { outType = RuleNodeType::Extrude; return true; }
    if (value == "revolve") { outType = RuleNodeType::Revolve; return true; }
    if (value == "sweep") { outType = RuleNodeType::Sweep; return true; }
    if (value == "loft") { outType = RuleNodeType::Loft; return true; }
    if (value == "csg") { outType = RuleNodeType::Csg; return true; }
    if (value == "deform") { outType = RuleNodeType::Deform; return true; }
    if (value == "array") { outType = RuleNodeType::Array; return true; }
    if (value == "group") { outType = RuleNodeType::Group; return true; }
    return false;
}

bool TryParsePrimitiveType(const std::string& value, PrimitiveType& outType) {
    if (value == "cube") { outType = PrimitiveType::Cube; return true; }
    if (value == "sphere") { outType = PrimitiveType::Sphere; return true; }
    if (value == "cylinder") { outType = PrimitiveType::Cylinder; return true; }
    if (value == "capsule") { outType = PrimitiveType::Capsule; return true; }
    if (value == "cone") { outType = PrimitiveType::Cone; return true; }
    if (value == "torus") { outType = PrimitiveType::Torus; return true; }
    return false;
}

bool TryParseCsgOperation(const std::string& value, CsgOperation& outOperation) {
    if (value == "union") { outOperation = CsgOperation::Union; return true; }
    if (value == "subtract") { outOperation = CsgOperation::Subtract; return true; }
    if (value == "intersect") { outOperation = CsgOperation::Intersect; return true; }
    return false;
}

} // namespace Massing
} // namespace Moon
