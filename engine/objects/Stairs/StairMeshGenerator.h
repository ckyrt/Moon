#pragma once

#include "../../core/CSG/CSGBuilder.h"

namespace Moon {
namespace Object {

struct StairBuildParams {
    float width = 1.2f;
    int stepCount = 10;
    float treadDepth = 0.28f;
    float stepHeight = 0.18f;
    float treadThickness = 0.04f;
    float stringerThickness = 0.04f;
    float stringerHeight = 0.12f;
    float railHeight = 0.92f;
    float railOffset = 0.03f;
    float postSpacing = 0.95f;
    float postWidth = 0.04f;
    float handrailWidth = 0.04f;
    float handrailHeight = 0.04f;
    bool leftRail = true;
    bool rightRail = true;
    std::string treadMaterial;
    std::string stringerMaterial;
    std::string railMaterial;
};

class StairMeshGenerator {
public:
    static Moon::CSG::BuildResult BuildStraightStair(const StairBuildParams& params,
                                                     std::string& outError);
};

} // namespace Object
} // namespace Moon
