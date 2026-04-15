#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "../../core/Camera/Camera.h"
#include "../../external/DiligentEngine/DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include "../IRenderer.h"

namespace Moon {
class Material;
class Mesh;
class MeshRenderer;
class Scene;
class SceneNode;
class Skybox;
struct EnvironmentState;
}

namespace Diligent {
struct IBuffer;
struct IDeviceContext;
struct IPipelineState;
struct IRenderDevice;
struct IShader;
struct IShaderResourceBinding;
struct ISwapChain;
struct ITexture;
struct ITextureView;
struct LayoutElement;
using Uint32 = uint32_t;
}

class DiligentRenderer : public IRenderer {
public:
    DiligentRenderer();
    ~DiligentRenderer() override;

    bool Initialize(const RenderInitParams& params) override;
    void Shutdown() override;
    void Resize(uint32_t w, uint32_t h) override;

    void BeginFrame() override;
    void EndFrame() override;
    void RenderFrame() override;
    void SetViewportRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void ResetViewportRect();
    bool BeginPreviewPass(uint32_t width, uint32_t height);
    void EndPreviewPass();
    Diligent::ITextureView* GetPreviewSRV() const;

    void SetViewProjectionMatrix(const float* viewProj16) override;
    void SetMaterialParameters(Moon::Material* material);
    void SetCameraPosition(const Moon::Vector3& position);
    void SetCameraBasis(const Moon::Vector3& right, const Moon::Vector3& up);
    void UpdateSceneLights(Moon::Scene* scene);
    void SetEnvironmentState(const Moon::EnvironmentState* environmentState);
    void SetRenderingTransparent(bool transparent);
    void DrawMesh(Moon::Mesh* mesh, const Moon::Matrix4x4& worldMatrix) override;
    void DrawCube(const Moon::Matrix4x4& worldMatrix) override;

    void BindAlbedoTexture(const std::string& texturePath);
    void BindAOTexture(const std::string& texturePath);
    void BindRoughnessTexture(const std::string& texturePath);
    void BindMetalnessTexture(const std::string& texturePath);
    void BindNormalTexture(const std::string& texturePath);

    void UpdateSceneSkybox(Moon::Scene* scene);
    void RenderSkybox();
    void RenderPrecipitationVolume();

    void RenderShadowMap(Moon::Scene* scene, Moon::Camera* camera);
    void RenderPointShadowMap(Moon::Scene* scene);

    Diligent::IRenderDevice* GetDevice() const { return m_pDevice; }
    Diligent::IDeviceContext* GetContext() const { return m_pImmediateContext; }
    Diligent::ISwapChain* GetSwapChain() const { return m_pSwapChain; }

    void RenderSceneForPicking(Moon::Scene* scene, int vpX = 0, int vpY = 0, int vpW = 0, int vpH = 0);
    uint32_t ReadObjectIDAt(int x, int y);

private:
    enum class MaterialPipeline {
        DefaultLit,
        Water
    };

    struct MeshGPUResources {
        Diligent::RefCntAutoPtr<Diligent::IBuffer> VB;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> IB;
        size_t IndexCount = 0;
        size_t VertexCount = 0;
    };

    struct VSConstantsCPU {
        Moon::Matrix4x4 WorldViewProjT;
        Moon::Matrix4x4 WorldT;
    };

    struct ShadowVSConstantsCPU {
        Moon::Matrix4x4 WorldViewProjT;
    };

    struct PSMaterialCPU {
        float metallic = 0.0f;
        float roughness = 0.5f;
        float triplanarTiling = 0.5f;
        float mappingMode = 0.0f;
        Moon::Vector3 baseColor = Moon::Vector3(1.0f, 1.0f, 1.0f);
        float triplanarBlend = 4.0f;
        float hasNormalMap = 0.0f;
        float opacity = 1.0f;
        float useVertexColorTint = 0.0f;
        float alphaCutoff = 0.0f;
        Moon::Vector3 transmissionColor = Moon::Vector3(1.0f, 1.0f, 1.0f);
        float padding3 = 0.0f;
    };

    struct PSSceneCPU {
        Moon::Vector3 cameraPosition;
        float hasEnvironmentMap = 0.0f;

        Moon::Vector3 lightDirection;
        float padding2 = 0.0f;
        Moon::Vector3 lightColor;
        float lightIntensity = 0.0f;

        Moon::Vector3 pointLightPosition;
        float pointLightRange = 0.0f;
        Moon::Vector3 pointLightColor;
        float pointLightIntensity = 0.0f;
        Moon::Vector3 pointLightAttenuation;
        float paddingPoint = 0.0f;

        Moon::Vector3 fogColor = Moon::Vector3(0.65f, 0.75f, 0.90f);
        float fogDensity = 0.0f;
        Moon::Vector3 skyColor = Moon::Vector3(0.20f, 0.40f, 0.60f);
        float fogEnabled = 0.0f;
        float cloudCoverage = 0.0f;
        float wetness = 0.0f;
        float windStrength = 0.0f;
        float timeSeconds = 0.0f;
        float precipitationIntensity = 0.0f;
        float snowAmount = 0.0f;
        float scenePadding0 = 0.0f;
        float scenePadding1 = 0.0f;
    };

    struct SkyboxConstantsCPU {
        Moon::Matrix4x4 ViewProjT;
        Moon::Vector3 SkyZenithColor = Moon::Vector3(0.18f, 0.35f, 0.75f);
        float UseProceduralSky = 0.0f;
        Moon::Vector3 SkyHorizonColor = Moon::Vector3(0.85f, 0.65f, 0.45f);
        float SkyIntensity = 1.0f;
        Moon::Vector3 SunDirection = Moon::Vector3(0.0f, 1.0f, 0.0f);
        float StarIntensity = 0.0f;
        Moon::Vector3 SunColor = Moon::Vector3(1.0f, 0.95f, 0.85f);
        float SunDiscSize = 0.9992f;
        Moon::Vector3 MoonDirection = Moon::Vector3(0.0f, -1.0f, 0.0f);
        float MoonIntensity = 0.0f;
        Moon::Vector3 MoonColor = Moon::Vector3(0.70f, 0.78f, 0.92f);
        float CloudCoverage = 0.0f;
        float TimeSeconds = 0.0f;
        float Padding0 = 0.0f;
        float Padding1 = 0.0f;
        float Padding2 = 0.0f;
    };

    struct ShadowConstantsCPU {
        Moon::Matrix4x4 WorldToShadowClipT;
        float shadowMapTexelSize[2] = {0.0f, 0.0f};
        float shadowBias = 0.0015f;
        float shadowStrength = 1.0f;
    };

    struct PointShadowConstantsCPU {
        Moon::Vector3 lightPosition;
        float invRange = 0.0f;
        float bias = 0.002f;
        float strength = 0.0f;
        float padding[2] = {0.0f, 0.0f};
    };

    struct PSConstantsCPU {
        uint32_t ObjectID = 0;
        uint32_t pad[3] = {0, 0, 0};
    };

#ifdef _WIN32
    void* m_hWnd = nullptr;
#endif
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pImmediateContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> m_pSwapChain;

    Diligent::ITextureView* m_pRTV = nullptr;
    Diligent::ITextureView* m_pDSV = nullptr;

    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    uint32_t m_ViewportX = 0;
    uint32_t m_ViewportY = 0;
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pVSConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pPSMaterialConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pPSSceneConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pShadowConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pPointShadowConstants;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSRB;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pTransparentPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pTransparentSRB;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pWaterTransparentPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pWaterTransparentSRB;

    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pShadowVSConstants;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pShadowPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pShadowSRB;
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pShadowMap;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pShadowMapSRV;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pShadowMapDSV;
    uint32_t m_ShadowMapSize = 2048;
    Moon::Matrix4x4 m_LightViewProj;
    bool m_IsRenderingShadow = false;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPointShadowPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pPointShadowSRB;
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pPointShadowCube;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pPointShadowCubeSRV;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pPointShadowCubeRTV[6];
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pPointShadowDepth;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pPointShadowDepthDSV;
    uint32_t m_PointShadowMapSize = 1024;
    Moon::Matrix4x4 m_PointLightFaceViewProj;
    bool m_IsRenderingPointShadow = false;
    bool m_PointLightCastsShadows = false;

    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pSkyboxVB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pSkyboxIB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pSkyboxVSConstants;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pSkyboxPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSkyboxSRB;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPrecipitationVolumePSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pPrecipitationVolumeSRB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pPrecipitationVB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pPrecipitationIB;

    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pBRDF_LUT;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pBRDF_LUT_SRV;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pBRDF_LUT_RTV;

    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pEquirectHDR;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pEquirectHDR_SRV;

    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pEnvironmentMap;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pEnvironmentMapSRV;

    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pIrradianceMap;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pIrradianceMapSRV;

    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pPrefilteredEnvMap;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pPrefilteredEnvMapSRV;

    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pPickingRT;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pPickingRTV;
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pPickingDS;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pPickingDSV;
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pPickingReadback;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pPickingPSConstants;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPickingPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pPickingSRB;

    std::unordered_map<uint64_t, MeshGPUResources> m_MeshCache;

    struct TextureGPUResources {
        Diligent::RefCntAutoPtr<Diligent::ITexture> Texture;
        Diligent::RefCntAutoPtr<Diligent::ITextureView> SRV;
    };
    std::unordered_map<std::string, TextureGPUResources> m_TextureCache;

    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pDefaultWhiteTexture;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pDefaultWhiteTextureSRV;
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pPreviewColor;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pPreviewRTV;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pPreviewSRV;
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pPreviewDepth;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pPreviewDSV;
    uint32_t m_PreviewWidth = 0;
    uint32_t m_PreviewHeight = 0;

    Moon::Matrix4x4 m_ViewProj;
    PSSceneCPU m_SceneDataCache;
    Moon::Vector3 m_CameraRight = Moon::Vector3(1.0f, 0.0f, 0.0f);
    Moon::Vector3 m_CameraUp = Moon::Vector3(0.0f, 1.0f, 0.0f);

    bool m_IsRenderingTransparent = false;
    MaterialPipeline m_ActiveMaterialPipeline = MaterialPipeline::DefaultLit;
    bool m_HasEnvironmentState = false;
    bool m_RenderProceduralSky = false;
    std::chrono::steady_clock::time_point m_SkyStartTime;

    void CreateDeviceAndSwapchain(const RenderInitParams& params);
    void EnsurePreviewRenderTarget(uint32_t width, uint32_t height);
    void CreateVSConstants();
    void CreateDefaultWhiteTexture();
    void CreateMainPass();
    void CreateTransparentPass();
    void CreateWaterTransparentPass();
    void CreateShadowPass();
    void CreateShadowMapResources();
    void CreatePointShadowPass();
    void CreatePointShadowMapResources();
    void CreateSkyboxPass();
    void CreatePrecipitationVolumePass();
    void CreatePrecipitationVolumeBuffers();
    void PrecomputeIBL();
    void LoadEnvironmentMap(const char* filepath);
    void ConvertEquirectangularToCubemap(Diligent::ITexture* pEquirectangularMap);

    void CreatePickingStatic();
    void CreateOrResizePickingRTs();
    Diligent::RefCntAutoPtr<Diligent::IShader> CreateShaderFromFile(
        const char* filename,
        Diligent::SHADER_TYPE shaderType,
        const char* debugName);
    bool BindStaticBuffer(
        Diligent::IPipelineState* pso,
        Diligent::SHADER_TYPE shaderType,
        const char* variableName,
        Diligent::IBuffer* buffer,
        const char* passName,
        bool required = true);
    bool BindMutableTexture(
        Diligent::IShaderResourceBinding* srb,
        Diligent::SHADER_TYPE shaderType,
        const char* variableName,
        Diligent::ITextureView* textureView,
        const char* passName,
        bool required = true);
    bool CreateSurfacePass(
        const char* passName,
        const char* vsFile,
        const char* psFile,
        bool enableBlending,
        bool bindMaterialToVS,
        Diligent::RefCntAutoPtr<Diligent::IPipelineState>& outPSO,
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& outSRB);
    void BindSharedSurfaceBuffers(
        Diligent::IPipelineState* pso,
        const char* passName,
        bool bindMaterialToVS);
    void BindSharedSurfaceTextures(
        Diligent::IShaderResourceBinding* srb,
        const char* passName);

    MeshGPUResources* GetOrCreateMeshResources(Moon::Mesh* mesh);
    TextureGPUResources* GetOrCreateTextureResources(const std::string& path, bool isSRGB);

    static Moon::Matrix4x4 Transpose(const Moon::Matrix4x4& m);
    template <typename T>
    void UpdateCB(Diligent::IBuffer* buf, const T& data);
    static void GetVertexLayout(Diligent::LayoutElement* outLayout, Diligent::Uint32& outNumElements);

    DiligentRenderer(const DiligentRenderer&) = delete;
    DiligentRenderer& operator=(const DiligentRenderer&) = delete;
};
