#pragma once

#include <cstdint>
#include <unordered_map>
#include <chrono>

// Moon Engine
#include "../core/Camera/Camera.h"

// Moon forward declarations
namespace Moon {
    class Mesh;
    class Scene;
    class SceneNode;
    class MeshRenderer;
}

// IRenderer 接口
#include "IRenderer.h"

// Diligent Engine
#include "../../external/DiligentEngine/DiligentCore/Common/interface/RefCntAutoPtr.hpp"

namespace Diligent {
    struct IRenderDevice;
    struct IDeviceContext;
    struct ISwapChain;
    struct IBuffer;
    struct IShader;
    struct IPipelineState;
    struct IShaderResourceBinding;
    struct ITexture;
    struct ITextureView;
}

class DiligentRenderer : public IRenderer {
public:
    DiligentRenderer();
    ~DiligentRenderer() override;

    // === IRenderer ===
    bool Initialize(const RenderInitParams& params) override;
    void Shutdown() override;
    void Resize(uint32_t w, uint32_t h) override;

    void BeginFrame() override;
    void EndFrame() override;
    void RenderFrame() override;

    void SetViewProjectionMatrix(const float* viewProj16) override;
    void DrawMesh(Moon::Mesh* mesh, const Moon::Matrix4x4& worldMatrix) override;
    void DrawCube(const Moon::Matrix4x4& worldMatrix) override;

    // 提供给 ImGui 等
    Diligent::IRenderDevice* GetDevice()   const { return m_pDevice; }
    Diligent::IDeviceContext* GetContext()  const { return m_pImmediateContext; }
    Diligent::ISwapChain* GetSwapChain()const { return m_pSwapChain; }

    // Picking
    void     RenderSceneForPicking(Moon::Scene* scene);
    uint32_t ReadObjectIDAt(int x, int y);

private:
    // ======== 内部类型 ========
    struct MeshGPUResources {
        Diligent::RefCntAutoPtr<Diligent::IBuffer> VB;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> IB;
        size_t IndexCount = 0;
        size_t VertexCount = 0;
    };

    struct VSConstantsCPU { // 64B
        Moon::Matrix4x4 WorldViewProjT;
    };
    struct PSConstantsCPU { // 16B 对齐
        uint32_t ObjectID = 0;
        uint32_t pad[3] = { 0,0,0 };
    };

    // ======== 设备与交换链 ========
#ifdef _WIN32
    void* m_hWnd = nullptr; // HWND
#endif
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>  m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pImmediateContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>     m_pSwapChain;

    // Backbuffer RTV/DSV 由 SwapChain 持有（不要 Release）
    Diligent::ITextureView* m_pRTV = nullptr;
    Diligent::ITextureView* m_pDSV = nullptr;

    uint32_t m_Width = 0, m_Height = 0;

    // ======== 主渲染管线 ========
    Diligent::RefCntAutoPtr<Diligent::IBuffer>          m_pVSConstants;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>   m_pPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSRB;

    // ======== Picking 管线 ========
    // 纹理本体 + 视图
    Diligent::RefCntAutoPtr<Diligent::ITexture>     m_pPickingRT;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pPickingRTV; // R32_UINT
    Diligent::RefCntAutoPtr<Diligent::ITexture>     m_pPickingDS;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pPickingDSV; // D32_FLOAT

    // 读回 1x1 纹理（复用）
    Diligent::RefCntAutoPtr<Diligent::ITexture>     m_pPickingReadback;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>          m_pPickingPSConstants;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>   m_pPickingPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pPickingSRB;

    // ======== Mesh 缓存 ========
    std::unordered_map<Moon::Mesh*, MeshGPUResources> m_MeshCache;

    // ======== 相机 ========
    Moon::Matrix4x4 m_ViewProj; // row-major 输入

    // ======== 帮助函数 ========
    void CreateDeviceAndSwapchain(const RenderInitParams& params);
    void CreateVSConstants();
    void CreateMainPass();  // 主渲染管线 PSO

    void CreatePickingStatic();      // 常量缓冲、PSO、SRB
    void CreateOrResizePickingRTs(); // RT/DS + 读回纹理

    MeshGPUResources* GetOrCreateMeshResources(Moon::Mesh* mesh);

    // 工具
    static Moon::Matrix4x4 Transpose(const Moon::Matrix4x4& m);
    template<typename T> void UpdateCB(Diligent::IBuffer* buf, const T& data);

    // 禁拷贝
    DiligentRenderer(const DiligentRenderer&) = delete;
    DiligentRenderer& operator=(const DiligentRenderer&) = delete;
};
