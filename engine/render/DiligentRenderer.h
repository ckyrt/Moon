#pragma once
#include "IRenderer.h"
#include "RenderCommon.h"
#include "../core/Camera/Camera.h"  // For Moon::Matrix4x4
#include <memory>
#include <chrono>
#include <unordered_map>

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

// Forward declarations for Moon Engine
namespace Moon {
    class Mesh;
}

class DiligentRenderer : public IRenderer {
public:
    DiligentRenderer();
    ~DiligentRenderer();

    // === IRenderer 接口实现 ===
    
    bool Initialize(const RenderInitParams& params) override;
    void Shutdown() override;
    void Resize(uint32_t w, uint32_t h) override;
    
    void BeginFrame() override;
    void EndFrame() override;
    void RenderFrame() override;
    
    void SetViewProjectionMatrix(const float* viewProj16) override;
    void DrawMesh(Moon::Mesh* mesh, const Moon::Matrix4x4& worldMatrix) override;
    void DrawCube(const Moon::Matrix4x4& worldMatrix) override;
    
    // Mesh GPU 资源管理
    void ReleaseMeshResources(Moon::Mesh* mesh);
    void ClearAllMeshResources();

    // ✅ 添加：获取 Diligent 对象用于 ImGui 初始化
    Diligent::IRenderDevice* GetDevice() const { return m_pDevice; }
    Diligent::IDeviceContext* GetContext() const { return m_pImmediateContext; }
    Diligent::ISwapChain* GetSwapChain() const { return m_pSwapChain; }

private:
    // Mesh GPU 资源结构
    struct MeshGPUResources {
        Diligent::IBuffer* vertexBuffer = nullptr;
        Diligent::IBuffer* indexBuffer = nullptr;
        size_t vertexCount = 0;
        size_t indexCount = 0;
    };
    
    // Mesh 到 GPU 资源的映射表（Renderer 内部维护）
    std::unordered_map<Moon::Mesh*, MeshGPUResources> m_meshCache;
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
    
    // Mesh 资源管理辅助方法
    MeshGPUResources* GetOrCreateMeshResources(Moon::Mesh* mesh);

    struct VSConstants {
        Moon::Matrix4x4 WorldViewProj;
    };
};