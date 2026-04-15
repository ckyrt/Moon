#pragma once

#include "../../engine/core/Math/Vector3.h"

#include <string>

class EngineCore;

namespace Moon {
class Material;
class SceneNode;
namespace CSG {
struct BuildResult;
}
namespace Tooling {

class ObjectCopilotPreview {
public:
    struct PreviewStats {
        bool valid = false;
        Moon::Vector3 min = Moon::Vector3(0.0f, 0.0f, 0.0f);
        Moon::Vector3 max = Moon::Vector3(0.0f, 0.0f, 0.0f);
        Moon::Vector3 size = Moon::Vector3(0.0f, 0.0f, 0.0f);
        float axisLength = 0.0f;
    };

    explicit ObjectCopilotPreview(EngineCore* engine);

    bool RebuildFromObjectJson(const std::string& objectJson, std::string& outError);
    void Clear();
    const PreviewStats& GetPreviewStats() const { return m_previewStats; }

private:
    struct Bounds3 {
        Moon::Vector3 min = Moon::Vector3(0.0f, 0.0f, 0.0f);
        Moon::Vector3 max = Moon::Vector3(0.0f, 0.0f, 0.0f);
        bool valid = false;
    };

    static void ExpandBounds(Bounds3& bounds, const Moon::Vector3& point);
    static Bounds3 ComputeBounds(const Moon::CSG::BuildResult& buildResult);
    static Moon::Material* AddPreviewMaterial(Moon::SceneNode* node, const std::string& materialName);
    static Moon::Material* AddHelperMaterial(Moon::SceneNode* node,
                                             const Moon::Vector3& color,
                                             float opacity,
                                             float roughness = 0.9f);

    void FocusCamera(const Bounds3& bounds);
    void EnsurePreviewEnvironment();
    void AddPreviewHelpers(const Bounds3& bounds);
    void UpdatePreviewStats(const Bounds3& bounds);

    EngineCore* m_engine = nullptr;
    Moon::SceneNode* m_previewRoot = nullptr;
    PreviewStats m_previewStats;
};

} // namespace Tooling
} // namespace Moon
