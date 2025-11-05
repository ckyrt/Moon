#pragma once
#include "IRenderer.h"
#include "RenderCommon.h"
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
    
    // Helper methods
    void CreatePipelineState();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateUniformBuffer();
    void UpdateTransforms();
    
    // Matrix helper methods
    struct Matrix4x4 {
        float m[4][4];
        Matrix4x4();
        static Matrix4x4 Identity();
        static Matrix4x4 Perspective(float fov, float aspect, float nearZ, float farZ);
        static Matrix4x4 LookAt(float eyeX, float eyeY, float eyeZ, 
                               float atX, float atY, float atZ,
                               float upX, float upY, float upZ);
        static Matrix4x4 RotationY(float angle);
        static Matrix4x4 Translation(float x, float y, float z);
        Matrix4x4 operator*(const Matrix4x4& other) const;
    };

    struct VSConstants {
        Matrix4x4 WorldViewProj;
    };
};