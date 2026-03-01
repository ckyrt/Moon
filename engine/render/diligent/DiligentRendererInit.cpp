#include "DiligentRenderer.h"
#include "DiligentRendererUtils.h"
#include "../../core/Logging/Logger.h"

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/TextureView.h"

#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Platforms/Win32/interface/Win32NativeWindow.h"
#endif

using namespace Diligent;

void DiligentRenderer::CreateDeviceAndSwapchain(const RenderInitParams& params)
{
    auto* pFactory = GetEngineFactoryD3D11();

    EngineD3D11CreateInfo ci{};
    pFactory->CreateDeviceAndContextsD3D11(ci, &m_pDevice, &m_pImmediateContext);
    MOON_LOG_INFO("DiligentRenderer", "D3D11 device/context created");

    SwapChainDesc sc{};
    sc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
    sc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    sc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;
    sc.BufferCount = 2;
#ifdef _WIN32
    Win32NativeWindow wnd{ static_cast<HWND>(params.windowHandle) };
    pFactory->CreateSwapChainD3D11(m_pDevice, m_pImmediateContext, sc, FullScreenModeDesc{}, wnd, &m_pSwapChain);
#else
#error "This sample currently targets Win32 only."
#endif
    MOON_LOG_INFO("DiligentRenderer", "SwapChain created");

    m_pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    m_pDSV = m_pSwapChain->GetDepthBufferDSV();
}

void DiligentRenderer::CreateVSConstants()
{
    BufferDesc cb{};
    cb.Name = "VS Constants";
    cb.BindFlags = BIND_UNIFORM_BUFFER;
    cb.Usage = USAGE_DYNAMIC;
    cb.CPUAccessFlags = CPU_ACCESS_WRITE;
    cb.Size = sizeof(VSConstantsCPU);
    m_pDevice->CreateBuffer(cb, nullptr, &m_pVSConstants);

    // Shadow VS constants (depth-only pass)
    BufferDesc shadowVS{};
    shadowVS.Name = "Shadow VS Constants";
    shadowVS.BindFlags = BIND_UNIFORM_BUFFER;
    shadowVS.Usage = USAGE_DYNAMIC;
    shadowVS.CPUAccessFlags = CPU_ACCESS_WRITE;
    shadowVS.Size = sizeof(ShadowVSConstantsCPU);
    m_pDevice->CreateBuffer(shadowVS, nullptr, &m_pShadowVSConstants);
    
    // 创建 PS 材质常量缓冲区
    BufferDesc psCB{};
    psCB.Name = "PS Material Constants";
    psCB.BindFlags = BIND_UNIFORM_BUFFER;
    psCB.Usage = USAGE_DYNAMIC;
    psCB.CPUAccessFlags = CPU_ACCESS_WRITE;
    psCB.Size = sizeof(PSMaterialCPU);
    m_pDevice->CreateBuffer(psCB, nullptr, &m_pPSMaterialConstants);
    
    // 创建 PS 场景常量缓冲区
    BufferDesc psSceneCB{};
    psSceneCB.Name = "PS Scene Constants";
    psSceneCB.BindFlags = BIND_UNIFORM_BUFFER;
    psSceneCB.Usage = USAGE_DYNAMIC;
    psSceneCB.CPUAccessFlags = CPU_ACCESS_WRITE;
    psSceneCB.Size = sizeof(PSSceneCPU);
    m_pDevice->CreateBuffer(psSceneCB, nullptr, &m_pPSSceneConstants);

    // 创建 Shadow 常量缓冲区（主渲染 pass 的 PS 采样 shadow map 用）
    BufferDesc shadowCB{};
    shadowCB.Name = "Shadow Constants";
    shadowCB.BindFlags = BIND_UNIFORM_BUFFER;
    shadowCB.Usage = USAGE_DYNAMIC;
    shadowCB.CPUAccessFlags = CPU_ACCESS_WRITE;
    shadowCB.Size = sizeof(ShadowConstantsCPU);
    m_pDevice->CreateBuffer(shadowCB, nullptr, &m_pShadowConstants);

    // 创建 Point Shadow 常量缓冲区（点光源阴影 cubemap 生成 + 主渲染采样用）
    BufferDesc pointShadowCB{};
    pointShadowCB.Name = "Point Shadow Constants";
    pointShadowCB.BindFlags = BIND_UNIFORM_BUFFER;
    pointShadowCB.Usage = USAGE_DYNAMIC;
    pointShadowCB.CPUAccessFlags = CPU_ACCESS_WRITE;
    pointShadowCB.Size = sizeof(PointShadowConstantsCPU);
    m_pDevice->CreateBuffer(pointShadowCB, nullptr, &m_pPointShadowConstants);
}

void DiligentRenderer::CreateDefaultWhiteTexture()
{
    // 创建 1x1 白色纹理（RGBA8 SRGB）
    unsigned char whitePixel[4] = { 255, 255, 255, 255 };
    
    TextureDesc desc{};
    desc.Name = "Default White Texture";
    desc.Type = RESOURCE_DIM_TEX_2D;
    desc.Width = 1;
    desc.Height = 1;
    desc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
    desc.MipLevels = 1;
    desc.Usage = USAGE_IMMUTABLE;
    desc.BindFlags = BIND_SHADER_RESOURCE;
    
    TextureData initData{};
    TextureSubResData subRes{};
    subRes.pData = whitePixel;
    subRes.Stride = 4;  // 4 bytes per pixel
    initData.pSubResources = &subRes;
    initData.NumSubresources = 1;
    
    m_pDevice->CreateTexture(desc, &initData, &m_pDefaultWhiteTexture);
    
    // 创建采样器
    SamplerDesc samplerDesc{};
    samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = TEXTURE_ADDRESS_WRAP;
    
    RefCntAutoPtr<ISampler> pDefaultSampler;
    m_pDevice->CreateSampler(samplerDesc, &pDefaultSampler);
    
    // 创建带采样器的纹理视图
    TextureViewDesc viewDesc{};
    viewDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
    viewDesc.TextureDim = RESOURCE_DIM_TEX_2D;
    viewDesc.Format = desc.Format;
    
    m_pDefaultWhiteTexture->CreateView(viewDesc, &m_pDefaultWhiteTextureSRV);
    m_pDefaultWhiteTextureSRV->SetSampler(pDefaultSampler);
    
    MOON_LOG_INFO("DiligentRenderer", "Default white texture created with sampler");
}
