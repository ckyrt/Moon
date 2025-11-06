#pragma once
#include "IRenderer.h"
#include "RenderCommon.h"
#include "../core/Camera/Camera.h"  // For Moon::Matrix4x4
#include <memory>
#include <chrono>

// Forward declarations for Diligent Engine
namespace Diligent
{
    struct IRenderDevice;
    struct IDeviceContext;
    struct ISwapChain;
    struct IBuffer;
    struct IPipelineState;
    struct IShaderResourceBinding;
    struct ITextureView;
}

class DiligentRenderer : public IRenderer {
public:
    DiligentRenderer();
    ~DiligentRenderer();

    bool Initialize(const RenderInitParams& params) override;
    void RenderFrame() override;
    void Resize(uint32_t w, uint32_t h) override;
    void Shutdown() override;
    
    // Set view-projection matrix from external camera
    void SetViewProjectionMatrix(const float* viewProj16);
    void ClearExternalViewProjection(); // Reset to identity matrix

private:
    // Diligent Engine core objects
    Diligent::IRenderDevice*     m_pDevice = nullptr;
    Diligent::IDeviceContext*    m_pImmediateContext = nullptr;
    Diligent::ISwapChain*        m_pSwapChain = nullptr;

    // Cube rendering resources
    Diligent::IBuffer*           m_pVertexBuffer = nullptr;
    Diligent::IBuffer*           m_pIndexBuffer = nullptr;
    Diligent::IBuffer*           m_pVSConstants = nullptr;
    Diligent::IPipelineState*    m_pPSO = nullptr;
    Diligent::IShaderResourceBinding* m_pSRB = nullptr;

    // Render target
    Diligent::ITextureView*      m_pRTV = nullptr;
    Diligent::ITextureView*      m_pDSV = nullptr;

    // Window and viewport info
#ifdef _WIN32
    HWND m_hWnd = nullptr;
#endif
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;

    // Animation
    std::chrono::high_resolution_clock::time_point m_StartTime;
    
    // View-projection matrix from external camera
    Moon::Matrix4x4 m_viewProj;
    
    // Helper methods
    void CreatePipelineState();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateUniformBuffer();
    void UpdateTransforms();

    struct VSConstants {
        Moon::Matrix4x4 WorldViewProj;
    };
};