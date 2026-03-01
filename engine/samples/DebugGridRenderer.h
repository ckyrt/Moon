#pragma once

#include <memory>

class DiligentRenderer;

namespace Moon {
class PerspectiveCamera;
}

namespace HelloEngineImGui {

class DebugGridRenderer {
public:
    DebugGridRenderer();
    ~DebugGridRenderer();

    DebugGridRenderer(const DebugGridRenderer&) = delete;
    DebugGridRenderer& operator=(const DebugGridRenderer&) = delete;

    void Initialize(DiligentRenderer* renderer);
    void Shutdown();

    void Render(Moon::PerspectiveCamera* camera);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace HelloEngineImGui
