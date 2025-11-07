#include "DiligentRenderer.h"
#include "../core/Logging/Logger.h"
#include "../core/Mesh/Mesh.h"
#include <iostream>
#include <cmath>
#include <cstring>

// Diligent Engine includes
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
// (textures not used in this sample)
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Common/interface/RefCntAutoPtr.hpp"
#ifdef _WIN32
#include "Platforms/Win32/interface/Win32NativeWindow.h"
#endif

using namespace Diligent;

// Simple vertex data with position and color
struct Vertex {
    float position[3];
    float color[4];
};

// Cube geometry (24 vertices so each face can have its own color), 36 indices
static const Vertex CubeVertices[] = {
    // Front face (Z+)
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    // Back face (Z-)
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    // Left face (X-)
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    // Right face (X+)
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    // Top face (Y+)
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    // Bottom face (Y-)
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f, 1.0f}},
};

static const uint32_t CubeIndices[] = {
    // Front
    0, 1, 2, 0, 2, 3,
    // Back
    4, 5, 6, 4, 6, 7,
    // Left
    8, 9,10, 8,10,11,
    // Right
   12,13,14,12,14,15,
    // Top
   16,17,18,16,18,19,
    // Bottom
   20,21,22,20,22,23
};

// HLSL Vertex Shader
static const char* VSSource = R"(
cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

struct VSInput
{
    float3 Pos   : ATTRIB0;
    float4 Color : ATTRIB1;
};

struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float4 Color : COLOR0; 
};

void main(in VSInput VSIn, out PSInput PSIn) 
{
    PSIn.Pos   = mul(float4(VSIn.Pos, 1.0), g_WorldViewProj);
    PSIn.Color = VSIn.Color;
}
)";

// HLSL Pixel Shader
static const char* PSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float4 Color : COLOR0; 
};

struct PSOutput
{ 
    float4 Color : SV_TARGET; 
};

void main(in PSInput PSIn, out PSOutput PSOut)
{
    PSOut.Color = PSIn.Color;
}
)";

DiligentRenderer::DiligentRenderer() {
    m_StartTime = std::chrono::high_resolution_clock::now();
}

DiligentRenderer::~DiligentRenderer() {
    Shutdown();
}

bool DiligentRenderer::Initialize(const RenderInitParams& params) {
#ifdef _WIN32
    m_hWnd = static_cast<HWND>(params.windowHandle);
#endif
    m_Width = params.width;
    m_Height = params.height;

    MOON_LOG_INFO("DiligentRenderer", "Starting DiligentRenderer initialization...");
    MOON_LOG_INFO("DiligentRenderer", "Window size: %dx%d", params.width, params.height);

    try {
        MOON_LOG_INFO("DiligentRenderer", "Initializing DiligentRenderer...");
        
        // Create Diligent Engine factory
        auto* pFactoryD3D11 = GetEngineFactoryD3D11();
        
        // Create render device and device context
        EngineD3D11CreateInfo EngineCI;
        pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &m_pDevice, &m_pImmediateContext);
        MOON_LOG_INFO("DiligentRenderer", "D3D11 device and context created");
        
        // Create swap chain
        SwapChainDesc SCDesc;
        SCDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
        SCDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
        SCDesc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;
        SCDesc.BufferCount = 2;
        SCDesc.DefaultDepthValue = 1.0f;
        SCDesc.DefaultStencilValue = 0;
        SCDesc.IsPrimary = true;
        
#ifdef _WIN32
        Win32NativeWindow Window{m_hWnd};
        pFactoryD3D11->CreateSwapChainD3D11(m_pDevice, m_pImmediateContext, SCDesc, 
                                           FullScreenModeDesc{}, Window, &m_pSwapChain);
#endif
        MOON_LOG_INFO("DiligentRenderer", "Swap chain created");
        
        // Get render target and depth stencil views
        m_pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        m_pDSV = m_pSwapChain->GetDepthBufferDSV();
        
        // Create pipeline state and resources
        MOON_LOG_INFO("DiligentRenderer", "Creating vertex buffer...");
        CreateVertexBuffer();
        MOON_LOG_INFO("DiligentRenderer", "Creating index buffer...");
        CreateIndexBuffer();
        MOON_LOG_INFO("DiligentRenderer", "Creating uniform buffer...");
        CreateUniformBuffer();
        MOON_LOG_INFO("DiligentRenderer", "Creating pipeline state...");
        CreatePipelineState();
        
        MOON_LOG_INFO("DiligentRenderer", "DiligentRenderer initialized successfully!");
        return true;
    }
    catch (const std::exception& e) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to initialize DiligentRenderer: %s", e.what());
        return false;
    }
}

void DiligentRenderer::CreatePipelineState() {
    // Create vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.Desc.Name = "Triangle vertex shader";
        ShaderCI.Source = VSSource;
        m_pDevice->CreateShader(ShaderCI, &pVS);
        MOON_LOG_INFO("DiligentRenderer", "Vertex shader created");
    }

    // Create pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    ShaderCI.Desc.Name = "Triangle pixel shader";
        ShaderCI.Source = PSSource;
        m_pDevice->CreateShader(ShaderCI, &pPS);
        MOON_LOG_INFO("DiligentRenderer", "Pixel shader created");
    }

    // Define vertex input layout
    LayoutElement LayoutElems[] = {
        {0, 0, 3, VT_FLOAT32, False, 0},      // Position
        {1, 0, 4, VT_FLOAT32, False, 12}      // Color
    };

    // Create graphics pipeline state
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = "Triangle PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_SOLID;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

    // 设置默认变量类型为 STATIC，这样 Constants 会被自动识别
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);
    MOON_LOG_INFO("DiligentRenderer", "Pipeline state created");

    // Bind constant buffer following official examples
    // Since we don't explicitly specify 'Constants' variable type, it uses default type STATIC
    // Static variables never change, bound directly through pipeline state object
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pVSConstants);
    MOON_LOG_INFO("DiligentRenderer", "Constants buffer bound to pipeline state");

    // Create shader resource binding object and bind all static resources
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
    MOON_LOG_INFO("DiligentRenderer", "Shader resource binding created");
}

void DiligentRenderer::CreateVertexBuffer() {
    BufferDesc VertBuffDesc;
    VertBuffDesc.Name = "Cube vertex buffer";
    VertBuffDesc.Usage = USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size = sizeof(CubeVertices);
    
    BufferData VBData;
    VBData.pData = CubeVertices;
    VBData.DataSize = sizeof(CubeVertices);
    
    m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_pVertexBuffer);
    MOON_LOG_INFO("DiligentRenderer", "Vertex buffer created: %zu vertices, %zu bytes", 
                  sizeof(CubeVertices)/sizeof(CubeVertices[0]), sizeof(CubeVertices));
}

void DiligentRenderer::CreateIndexBuffer() {
    BufferDesc IndBuffDesc;
    IndBuffDesc.Name = "Cube index buffer";
    IndBuffDesc.Usage = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size = sizeof(CubeIndices);
    
    BufferData IBData;
    IBData.pData = CubeIndices;
    IBData.DataSize = sizeof(CubeIndices);
    
    m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_pIndexBuffer);
    MOON_LOG_INFO("DiligentRenderer", "Index buffer created: %zu indices, %zu bytes", 
                  sizeof(CubeIndices)/sizeof(CubeIndices[0]), sizeof(CubeIndices));
}

void DiligentRenderer::CreateUniformBuffer() {
    // 按照官方示例创建常量缓冲区
    BufferDesc CBDesc;
    CBDesc.Name = "VS constants CB";
    CBDesc.Size = sizeof(Moon::Matrix4x4);  // Store only one 4x4 matrix
    CBDesc.Usage = USAGE_DYNAMIC;
    CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    m_pDevice->CreateBuffer(CBDesc, nullptr, &m_pVSConstants);
    MOON_LOG_INFO("DiligentRenderer", "Uniform buffer created");
}

void DiligentRenderer::UpdateTransforms() {
    // Use view-projection matrix from external camera with world transform
    auto now = std::chrono::high_resolution_clock::now();
    float seconds = std::chrono::duration<float>(now - m_StartTime).count();
    Moon::Matrix4x4 world = Moon::Matrix4x4::RotationY(seconds * 0.8f);
    Moon::Matrix4x4 worldViewProj = world * m_viewProj;

    // Transpose for HLSL column-major layout
    Moon::Matrix4x4 worldViewProjT;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            worldViewProjT.m[i][j] = worldViewProj.m[j][i];

    void* pMappedData = nullptr;
    m_pImmediateContext->MapBuffer(m_pVSConstants, MAP_WRITE, MAP_FLAG_DISCARD, pMappedData);
    if (pMappedData) {
        memcpy(pMappedData, &worldViewProjT.m[0][0], sizeof(Moon::Matrix4x4));
        m_pImmediateContext->UnmapBuffer(m_pVSConstants, MAP_WRITE);
    }
}

void DiligentRenderer::BeginFrame() {
    if (!m_pDevice || !m_pSwapChain) {
        return;
    }
    
    // Set render targets
    m_pImmediateContext->SetRenderTargets(1, &m_pRTV, m_pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // Clear buffers
    const float ClearColor[] = {0.2f, 0.4f, 0.6f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(m_pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(m_pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // Set viewport
    Viewport VP;
    VP.TopLeftX = 0;
    VP.TopLeftY = 0;
    VP.Width = static_cast<float>(m_Width);
    VP.Height = static_cast<float>(m_Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    m_pImmediateContext->SetViewports(1, &VP, 0, 0);
    
    // Set pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    
    // Set vertex and index buffers
    Uint64 offset = 0;
    IBuffer* pBuffs[] = {m_pVertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_pIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void DiligentRenderer::EndFrame() {
    if (!m_pDevice || !m_pSwapChain) {
        return;
    }
    
    m_pSwapChain->Present();
}

DiligentRenderer::MeshGPUResources* 
DiligentRenderer::GetOrCreateMeshResources(Moon::Mesh* mesh) {
    // 检查缓存中是否已有该 Mesh 的 GPU 资源
    auto it = m_meshCache.find(mesh);
    if (it != m_meshCache.end()) {
        // 已缓存，直接返回
        return &it->second;
    }
    
    // 首次渲染：创建 GPU 资源
    MeshGPUResources gpu;
    
    const auto& vertices = mesh->GetVertices();
    const auto& indices = mesh->GetIndices();
    
    // 创建顶点缓冲区
    BufferDesc VertBuffDesc;
    VertBuffDesc.Name = "Mesh Vertex Buffer";
    VertBuffDesc.Usage = USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size = vertices.size() * sizeof(Moon::Vertex);
    
    BufferData VBData;
    VBData.pData = vertices.data();
    VBData.DataSize = VertBuffDesc.Size;
    
    m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &gpu.vertexBuffer);
    gpu.vertexCount = vertices.size();
    
    // 创建索引缓冲区
    BufferDesc IndBuffDesc;
    IndBuffDesc.Name = "Mesh Index Buffer";
    IndBuffDesc.Usage = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size = indices.size() * sizeof(uint32_t);
    
    BufferData IBData;
    IBData.pData = indices.data();
    IBData.DataSize = IndBuffDesc.Size;
    
    m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &gpu.indexBuffer);
    gpu.indexCount = indices.size();
    
    // 插入缓存
    m_meshCache[mesh] = gpu;
    
    MOON_LOG_INFO("DiligentRenderer", "Uploaded Mesh to GPU: %zu vertices, %zu indices (cached)", 
                  gpu.vertexCount, gpu.indexCount);
    
    return &m_meshCache[mesh];
}

void DiligentRenderer::DrawMesh(Moon::Mesh* mesh, const Moon::Matrix4x4& worldMatrix) {
    if (!m_pDevice || !m_pSwapChain || !mesh || !mesh->IsValid()) {
        return;
    }
    
    // 获取或创建 GPU 资源（自动缓存）
    MeshGPUResources* gpu = GetOrCreateMeshResources(mesh);
    
    // 计算变换矩阵
    Moon::Matrix4x4 worldViewProj = worldMatrix * m_viewProj;
    
    // 转置为 HLSL 列主序
    Moon::Matrix4x4 worldViewProjT;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            worldViewProjT.m[i][j] = worldViewProj.m[j][i];
    
    // 更新常量缓冲区
    void* pMappedData = nullptr;
    m_pImmediateContext->MapBuffer(m_pVSConstants, MAP_WRITE, MAP_FLAG_DISCARD, pMappedData);
    if (pMappedData) {
        memcpy(pMappedData, &worldViewProjT.m[0][0], sizeof(Moon::Matrix4x4));
        m_pImmediateContext->UnmapBuffer(m_pVSConstants, MAP_WRITE);
    }
    
    // 设置顶点和索引缓冲区（使用缓存的 GPU 资源）
    Uint64 offset = 0;
    IBuffer* pBuffs[] = {gpu->vertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, 
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(gpu->indexBuffer, 0, 
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // 提交着色器资源
    m_pImmediateContext->CommitShaderResources(m_pSRB, 
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // 绘制
    DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType = VT_UINT32;
    DrawAttrs.NumIndices = static_cast<Uint32>(gpu->indexCount);
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

void DiligentRenderer::DrawCube(const Moon::Matrix4x4& worldMatrix) {
    if (!m_pDevice || !m_pSwapChain) {
        return;
    }
    
    // Calculate world-view-projection matrix
    Moon::Matrix4x4 worldViewProj = worldMatrix * m_viewProj;
    
    // Transpose for HLSL column-major layout
    Moon::Matrix4x4 worldViewProjT;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            worldViewProjT.m[i][j] = worldViewProj.m[j][i];
    
    // Update constant buffer
    void* pMappedData = nullptr;
    m_pImmediateContext->MapBuffer(m_pVSConstants, MAP_WRITE, MAP_FLAG_DISCARD, pMappedData);
    if (pMappedData) {
        memcpy(pMappedData, &worldViewProjT.m[0][0], sizeof(Moon::Matrix4x4));
        m_pImmediateContext->UnmapBuffer(m_pVSConstants, MAP_WRITE);
    }
    
    // Commit shader resources
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // Draw cube
    DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType = VT_UINT32;
    DrawAttrs.NumIndices = sizeof(CubeIndices)/sizeof(CubeIndices[0]);
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

void DiligentRenderer::RenderFrame() {
    if (!m_pDevice || !m_pSwapChain) {
        return;
    }
    
    // Legacy single cube rendering with animation
    UpdateTransforms();
    
    BeginFrame();
    
    // Draw animated cube
    auto now = std::chrono::high_resolution_clock::now();
    float seconds = std::chrono::duration<float>(now - m_StartTime).count();
    Moon::Matrix4x4 world = Moon::Matrix4x4::RotationY(seconds * 0.8f);
    DrawCube(world);
    
    EndFrame();
}

void DiligentRenderer::Resize(uint32_t w, uint32_t h) {
    if (m_Width == w && m_Height == h) return;
    
    MOON_LOG_INFO("DiligentRenderer", "Resizing viewport from %ux%u to %ux%u", m_Width, m_Height, w, h);
    
    m_Width = w;
    m_Height = h;
    
    if (m_pSwapChain) {
        m_pSwapChain->Resize(m_Width, m_Height);
        m_pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        m_pDSV = m_pSwapChain->GetDepthBufferDSV();
        MOON_LOG_INFO("DiligentRenderer", "Viewport resize completed");
    }
}

void DiligentRenderer::SetViewProjectionMatrix(const float* viewProj16) {
    if (viewProj16) {
        // Copy the 16 floats into our matrix (assuming row-major from Moon::Matrix4x4)
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                m_viewProj.m[i][j] = viewProj16[i * 4 + j];
            }
        }
    }
}

void DiligentRenderer::ReleaseMeshResources(Moon::Mesh* mesh) {
    auto it = m_meshCache.find(mesh);
    if (it != m_meshCache.end()) {
        if (it->second.vertexBuffer) {
            it->second.vertexBuffer->Release();
        }
        if (it->second.indexBuffer) {
            it->second.indexBuffer->Release();
        }
        m_meshCache.erase(it);
        MOON_LOG_INFO("DiligentRenderer", "Released GPU resources for Mesh");
    }
}

void DiligentRenderer::ClearAllMeshResources() {
    MOON_LOG_INFO("DiligentRenderer", "Clearing all Mesh GPU resources (%zu meshes cached)", m_meshCache.size());
    
    for (auto& pair : m_meshCache) {
        if (pair.second.vertexBuffer) {
            pair.second.vertexBuffer->Release();
        }
        if (pair.second.indexBuffer) {
            pair.second.indexBuffer->Release();
        }
    }
    m_meshCache.clear();
}

void DiligentRenderer::Shutdown() {
    MOON_LOG_INFO("DiligentRenderer", "Shutting down DiligentRenderer");
    
    // 首先清理所有 Mesh 的 GPU 资源
    ClearAllMeshResources();
    
    // Release resources in reverse order of creation
    if (m_pSRB) {
        m_pSRB->Release();
        m_pSRB = nullptr;
    }
    if (m_pPSO) {
        m_pPSO->Release();
        m_pPSO = nullptr;
    }
    if (m_pVSConstants) {
        m_pVSConstants->Release();
        m_pVSConstants = nullptr;
    }
    if (m_pIndexBuffer) {
        m_pIndexBuffer->Release();
        m_pIndexBuffer = nullptr;
    }
    if (m_pVertexBuffer) {
        m_pVertexBuffer->Release();
        m_pVertexBuffer = nullptr;
    }
    
    // Note: m_pRTV and m_pDSV are owned by the swap chain
    m_pRTV = nullptr;
    m_pDSV = nullptr;
    
    if (m_pSwapChain) {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }
    if (m_pImmediateContext) {
        m_pImmediateContext->Release();
        m_pImmediateContext = nullptr;
    }
    if (m_pDevice) {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }
    
    MOON_LOG_INFO("DiligentRenderer", "DiligentRenderer shutdown completed");
    
#ifdef _WIN32
    m_hWnd = nullptr;
#endif
}
