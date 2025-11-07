#pragma once
#include "IRenderer.h"
#include "RenderCommon.h"
#include "../core/Camera/Camera.h"

/**
 * @brief 空渲染器 - 用于无图形环境或测试
 * 
 * 不执行任何实际渲染，但实现了完整的 IRenderer 接口
 */
class NullRenderer : public IRenderer {
public:
    bool Initialize(const RenderInitParams& params) override;
    void Shutdown() override;
    void Resize(uint32_t w, uint32_t h) override;
    
    void BeginFrame() override;
    void EndFrame() override;
    void RenderFrame() override;
    
    void SetViewProjectionMatrix(const float* viewProj16) override;
    void DrawCube(const Moon::Matrix4x4& worldMatrix) override;

private:
#ifdef _WIN32
    HWND hwnd_ = nullptr;
#endif
    unsigned tick_ = 0;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};
