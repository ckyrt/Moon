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
    class Skybox;
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
    struct LayoutElement;
    using Uint32 = uint32_t;
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
    void SetMaterialParameters(float metallic, float roughness, const Moon::Vector3& baseColor);  // 设置 PBR 材质参数
    void SetCameraPosition(const Moon::Vector3& position);        // 设置相机位置（用于高光计算）
    void UpdateSceneLights(Moon::Scene* scene);                   // 更新场景光源数据
    void DrawMesh(Moon::Mesh* mesh, const Moon::Matrix4x4& worldMatrix) override;
    void DrawCube(const Moon::Matrix4x4& worldMatrix) override;
    
    void BindAlbedoTexture(const std::string& texturePath);       // 绑定 Albedo 贴图到当前 SRB
    void BindARMTexture(const std::string& texturePath);          // 绑定 ARM (AO+Roughness+Metallic) 贴图到当前 SRB
    void BindNormalTexture(const std::string& texturePath);       // 绑定法线贴图到当前 SRB
    
    void UpdateSceneSkybox(Moon::Scene* scene);                   // 从场景中查找并更新 Skybox 组件
    void RenderSkybox();  // 渲染 Skybox（在所有不透明物体之后调用）

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

    struct VSConstantsCPU { // 128B
        Moon::Matrix4x4 WorldViewProjT;
        Moon::Matrix4x4 WorldT;
    };
    struct PSMaterialCPU { // 16B 对齐（PBR 材质参数）
        float metallic = 0.0f;
        float roughness = 0.5f;
        float padding1[2] = { 0.0f, 0.0f };
        Moon::Vector3 baseColor = Moon::Vector3(1.0f, 1.0f, 1.0f);
        float padding2 = 0.0f;
    };
    struct PSSceneCPU { // 16B 对齐（场景参数：相机位置、光源等）
        Moon::Vector3 cameraPosition;
        float padding1 = 0.0f;
        
        // 主方向光（Directional Light）
        Moon::Vector3 lightDirection;      // 光源方向
        float padding2 = 0.0f;
        Moon::Vector3 lightColor;          // 光源颜色
        float lightIntensity = 0.0f;       // 光源强度（0 = 无光源）
    };
    struct PSConstantsCPU { // 16B 对齐（Picking）
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
    Diligent::RefCntAutoPtr<Diligent::IBuffer>          m_pPSMaterialConstants;  // PBR 材质参数
    Diligent::RefCntAutoPtr<Diligent::IBuffer>          m_pPSSceneConstants;     // 场景参数（相机位置等）
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>   m_pPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSRB;

    // ======== Skybox 渲染管线 ========
    Diligent::RefCntAutoPtr<Diligent::IBuffer>          m_pSkyboxVB;         // Skybox 立方体顶点缓冲
    Diligent::RefCntAutoPtr<Diligent::IBuffer>          m_pSkyboxIB;         // Skybox 立方体索引缓冲
    Diligent::RefCntAutoPtr<Diligent::IBuffer>          m_pSkyboxVSConstants; // VP 矩阵（移除平移）
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>   m_pSkyboxPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSkyboxSRB;

    // ======== IBL（Image-Based Lighting）资源 ========
    Diligent::RefCntAutoPtr<Diligent::ITexture>         m_pBRDF_LUT;         // BRDF 积分查找表（256x256 RG16F）
    Diligent::RefCntAutoPtr<Diligent::ITextureView>     m_pBRDF_LUT_SRV;     // BRDF LUT Shader Resource View
    Diligent::RefCntAutoPtr<Diligent::ITextureView>     m_pBRDF_LUT_RTV;     // BRDF LUT Render Target View
    
    Diligent::RefCntAutoPtr<Diligent::ITexture>         m_pEquirectHDR;      // Equirectangular HDR 2D 纹理
    Diligent::RefCntAutoPtr<Diligent::ITextureView>     m_pEquirectHDR_SRV;  // Equirectangular HDR SRV
    
    Diligent::RefCntAutoPtr<Diligent::ITexture>         m_pEnvironmentMap;   // 环境贴图 Cubemap
    Diligent::RefCntAutoPtr<Diligent::ITextureView>     m_pEnvironmentMapSRV; // 环境贴图 SRV
    
    Diligent::RefCntAutoPtr<Diligent::ITexture>         m_pIrradianceMap;    // 漫反射 Irradiance Cubemap
    Diligent::RefCntAutoPtr<Diligent::ITextureView>     m_pIrradianceMapSRV;
    
    Diligent::RefCntAutoPtr<Diligent::ITexture>         m_pPrefilteredEnvMap; // 预过滤镜面环境贴图（带 mip levels）
    Diligent::RefCntAutoPtr<Diligent::ITextureView>     m_pPrefilteredEnvMapSRV;

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

    // ======== 纹理缓存 ========
    struct TextureGPUResources {
        Diligent::RefCntAutoPtr<Diligent::ITexture>     Texture;
        Diligent::RefCntAutoPtr<Diligent::ITextureView> SRV;
    };
    std::unordered_map<std::string, TextureGPUResources> m_TextureCache; // key: texture path
    
    // 默认白色纹理（用于没有贴图的材质）
    Diligent::RefCntAutoPtr<Diligent::ITexture>     m_pDefaultWhiteTexture;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pDefaultWhiteTextureSRV;

    // ======== 相机 ========
    Moon::Matrix4x4 m_ViewProj; // row-major 输入
    
    // ======== 场景数据缓存 ========
    PSSceneCPU m_SceneDataCache;  // 缓存场景数据（相机位置 + 光源）

    // ======== 帮助函数 ========
    void CreateDeviceAndSwapchain(const RenderInitParams& params);
    void CreateVSConstants();
    void CreateDefaultWhiteTexture();  // 创建默认白色纹理
    void CreateMainPass();  // 主渲染管线 PSO
    void CreateSkyboxPass(); // Skybox 渲染管线 PSO
    void PrecomputeIBL();    // 预计算 IBL 资源（BRDF LUT, Irradiance Map, Prefiltered Env Map）
    void LoadEnvironmentMap(const char* filepath); // 加载 HDR 环境贴图
    void ConvertEquirectangularToCubemap(Diligent::ITexture* pEquirectangularMap); // 将 equirectangular HDR 转换为 Cubemap

    void CreatePickingStatic();      // 常量缓冲、PSO、SRB
    void CreateOrResizePickingRTs(); // RT/DS + 读回纹理

    MeshGPUResources* GetOrCreateMeshResources(Moon::Mesh* mesh);

    // 纹理管理
    TextureGPUResources* GetOrCreateTextureResources(const std::string& path, bool isSRGB);

    // 工具
    static Moon::Matrix4x4 Transpose(const Moon::Matrix4x4& m);
    template<typename T> void UpdateCB(Diligent::IBuffer* buf, const T& data);
    
    // Vertex Layout 辅助函数（避免手写重复声明，确保所有 PSO 使用一致的 layout）
    static void GetVertexLayout(Diligent::LayoutElement* outLayout, Diligent::Uint32& outNumElements);

    // 禁拷贝
    DiligentRenderer(const DiligentRenderer&) = delete;
    DiligentRenderer& operator=(const DiligentRenderer&) = delete;
};
