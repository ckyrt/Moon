#pragma once

#include "UITextureManager.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Common/interface/RefCntAutoPtr.hpp"

// UI 渲染器
// 负责将 UI 纹理渲染到屏幕（全屏四边形）
class UIRenderer {
public:
    UIRenderer(Diligent::IRenderDevice* device, Diligent::IDeviceContext* context);
    ~UIRenderer() = default;

    // 初始化管线和资源
    bool Initialize();

    // 渲染 UI 纹理
    void RenderUI(
        UITextureManager* textureManager,
        Diligent::ITextureView* renderTargetView
    );

private:
    void CreatePipelineState();
    void CreateFullscreenQuad();

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pContext;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSRB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pVertexBuffer;
    bool m_Initialized = false;
};
