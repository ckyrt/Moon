#include "StairMeshGenerator.h"

#include "../../core/CSG/CSGOperations.h"
#include "../../core/Geometry/Path.h"
#include "../../core/Geometry/PathMeshBuilder.h"

#include <algorithm>

namespace Moon {
namespace Object {

using namespace Moon::CSG;
using namespace Moon::Geometry;

namespace {

void AddBox(BuildResult& result,
            float width,
            float height,
            float depth,
            const std::string& material,
            const Vector3& position,
            const Quaternion& rotation = Quaternion::Identity()) {
    auto mesh = CreateCSGBox(width, height, depth,
        Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(1, 1, 1), false);
    result.AddMesh(MeshItem(mesh, material, ResolvedTransform(position, rotation, Vector3(1, 1, 1))));
}

void AddRailSide(BuildResult& result,
                 const StairBuildParams& params,
                 float totalRise,
                 float totalRun,
                 float sideX) {
    const std::vector<Vector3> handrailPoints = {
        Vector3(sideX, params.stepHeight + params.railHeight, params.treadDepth * 0.5f),
        Vector3(sideX, totalRise + params.railHeight, totalRun - params.treadDepth * 0.5f)
    };
    const Path3D handrailPath = Path3D::FromPolyline(handrailPoints);
    if (!handrailPath.IsValid()) {
        return;
    }

    PathPlacementOptions placement;
    placement.spacing = params.postSpacing;
    placement.includeEnds = true;
    const std::vector<PathFrame> postFrames = GenerateUniformPlacements(handrailPath, placement);
    for (const auto& frame : postFrames) {
        const int stepIndex = std::max(0, std::min(params.stepCount - 1,
            static_cast<int>(std::floor(frame.position.z / std::max(params.treadDepth, 1e-4f)))));
        const float treadTopY = (stepIndex + 1) * params.stepHeight;
        const float postHeight = frame.position.y - treadTopY;
        AddBox(result, params.postWidth, std::max(postHeight, 0.05f), params.postWidth,
            params.railMaterial,
            Vector3(sideX, treadTopY + postHeight * 0.5f, frame.position.z));
    }

    std::shared_ptr<Mesh> handrailMesh(
        PathMeshBuilder::SweepRectangleAlongPath(handrailPath, params.handrailWidth, params.handrailHeight));
    if (handrailMesh && handrailMesh->IsValid()) {
        result.AddMesh(MeshItem(handrailMesh, params.railMaterial,
            ResolvedTransform(Vector3(0, 0, 0), Quaternion::Identity(), Vector3(1, 1, 1)),
            false));
    }
}

} // namespace

BuildResult StairMeshGenerator::BuildStraightStair(const StairBuildParams& params,
                                                   std::string& outError) {
    BuildResult result;
    if (params.stepCount <= 0) {
        outError = "Stair requires at least one step";
        return result;
    }

    const float totalRun = params.stepCount * params.treadDepth;
    const float totalRise = params.stepCount * params.stepHeight;

    for (int index = 0; index < params.stepCount; ++index) {
        const float stepY = (index + 1) * params.stepHeight - params.treadThickness * 0.5f;
        const float stepZ = index * params.treadDepth + params.treadDepth * 0.5f;
        AddBox(result, params.width, params.treadThickness, params.treadDepth,
            params.treadMaterial, Vector3(0, stepY, stepZ));
    }

    // New stringer generation: two narrow side carriages placed below the tread nosing line.
    const float carriageX = params.width * 0.5f - params.stringerThickness * 0.5f;
    const float carriageBottomOffset = params.treadThickness + params.stringerHeight * 0.5f;
    const std::vector<Vector3> leftStringerPoints = {
        Vector3(-carriageX, params.stepHeight - carriageBottomOffset, params.treadDepth * 0.35f),
        Vector3(-carriageX, totalRise - carriageBottomOffset, totalRun - params.treadDepth * 0.35f)
    };
    const std::vector<Vector3> rightStringerPoints = {
        Vector3(carriageX, params.stepHeight - carriageBottomOffset, params.treadDepth * 0.35f),
        Vector3(carriageX, totalRise - carriageBottomOffset, totalRun - params.treadDepth * 0.35f)
    };

    for (const auto& pathPoints : {leftStringerPoints, rightStringerPoints}) {
        const Path3D path = Path3D::FromPolyline(pathPoints);
        if (!path.IsValid()) {
            continue;
        }

        std::shared_ptr<Mesh> stringerMesh(
            PathMeshBuilder::SweepRectangleAlongPath(path, params.stringerThickness, params.stringerHeight));
        if (!stringerMesh || !stringerMesh->IsValid()) {
            outError = "Failed to build stair stringer sweep";
            return BuildResult();
        }

        result.AddMesh(MeshItem(stringerMesh, params.stringerMaterial,
            ResolvedTransform(Vector3(0, 0, 0), Quaternion::Identity(), Vector3(1, 1, 1)),
            false));
    }

    if (params.leftRail) {
        AddRailSide(result, params, totalRise, totalRun, -(params.width * 0.5f + params.railOffset));
    }
    if (params.rightRail) {
        AddRailSide(result, params, totalRise, totalRun, params.width * 0.5f + params.railOffset);
    }

    return result;
}

} // namespace Object
} // namespace Moon
