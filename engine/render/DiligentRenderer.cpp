#include "DiligentRenderer.h"
#include "../core/Logging/Logger.h"
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

// Matrix4x4 implementation
DiligentRenderer::Matrix4x4::Matrix4x4() {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            m[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

DiligentRenderer::Matrix4x4 DiligentRenderer::Matrix4x4::Identity() {
    return Matrix4x4();
}

DiligentRenderer::Matrix4x4 DiligentRenderer::Matrix4x4::Perspective(float fov, float aspect, float nearZ, float farZ) {
    Matrix4x4 result;
    float tanHalfFov = tan(fov * 0.5f);
    float height = 1.0f / tanHalfFov;
    float width = height / aspect;
    float fRange = farZ / (farZ - nearZ);
    
    result.m[0][0] = width;
    result.m[1][1] = height;
    result.m[2][2] = fRange;
    result.m[2][3] = 1.0f;
    result.m[3][2] = -fRange * nearZ;
    result.m[3][3] = 0.0f;
    
    return result;
}

DiligentRenderer::Matrix4x4 DiligentRenderer::Matrix4x4::LookAt(float eyeX, float eyeY, float eyeZ, 
                                                               float atX, float atY, float atZ,
                                                               float upX, float upY, float upZ) {
    Matrix4x4 result;
    
    float fX = atX - eyeX;
    float fY = atY - eyeY;
    float fZ = atZ - eyeZ;
    float fLen = sqrt(fX*fX + fY*fY + fZ*fZ);
    fX /= fLen; fY /= fLen; fZ /= fLen;
    
    float rX = upY * fZ - upZ * fY;
    float rY = upZ * fX - upX * fZ;
    float rZ = upX * fY - upY * fX;
    float rLen = sqrt(rX*rX + rY*rY + rZ*rZ);
    rX /= rLen; rY /= rLen; rZ /= rLen;
    
    float uX = fY * rZ - fZ * rY;
    float uY = fZ * rX - fX * rZ;
    float uZ = fX * rY - fY * rX;
    
    result.m[0][0] = rX;   result.m[0][1] = uX;   result.m[0][2] = fX;  result.m[0][3] = 0.0f;
    result.m[1][0] = rY;   result.m[1][1] = uY;   result.m[1][2] = fY;  result.m[1][3] = 0.0f;
    result.m[2][0] = rZ;   result.m[2][1] = uZ;   result.m[2][2] = fZ;  result.m[2][3] = 0.0f;
    result.m[3][0] = -(rX*eyeX + rY*eyeY + rZ*eyeZ);
    result.m[3][1] = -(uX*eyeX + uY*eyeY + uZ*eyeZ);
    result.m[3][2] = -(fX*eyeX + fY*eyeY + fZ*eyeZ);
    result.m[3][3] = 1.0f;
    
    return result;
}

DiligentRenderer::Matrix4x4 DiligentRenderer::Matrix4x4::RotationY(float angle) {
    Matrix4x4 result;
    float c = cos(angle);
    float s = sin(angle);
    
    result.m[0][0] = c;    result.m[0][2] = -s;
    result.m[2][0] = s;    result.m[2][2] = c;
    
    return result;
}

DiligentRenderer::Matrix4x4 DiligentRenderer::Matrix4x4::Translation(float x, float y, float z) {
    Matrix4x4 result;
    result.m[3][0] = x;
    result.m[3][1] = y;
    result.m[3][2] = z;
    return result;
}

DiligentRenderer::Matrix4x4 DiligentRenderer::Matrix4x4::operator*(const Matrix4x4& other) const {
    Matrix4x4 result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.m[i][j] = 0.0f;
            for (int k = 0; k < 4; k++) {
                result.m[i][j] += m[i][k] * other.m[k][j];
            }
        }
    }
    return result;
}

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
    CBDesc.Size = sizeof(Matrix4x4);  // Store only one 4x4 matrix
    CBDesc.Usage = USAGE_DYNAMIC;
    CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    m_pDevice->CreateBuffer(CBDesc, nullptr, &m_pVSConstants);
    MOON_LOG_INFO("DiligentRenderer", "Uniform buffer created");
}

void DiligentRenderer::UpdateTransforms() {
    auto now = std::chrono::high_resolution_clock::now();
    float seconds = std::chrono::duration<float>(now - m_StartTime).count();

    Matrix4x4 world = Matrix4x4::RotationY(seconds * 0.8f);
    Matrix4x4 view = Matrix4x4::LookAt(0.0f, 0.0f, -3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    float aspect = (m_Height != 0) ? (static_cast<float>(m_Width) / static_cast<float>(m_Height)) : 1.0f;
    Matrix4x4 proj = Matrix4x4::Perspective(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 100.0f);

    Matrix4x4 worldViewProj = world * view * proj;
    Matrix4x4 worldViewProjT;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            worldViewProjT.m[i][j] = worldViewProj.m[j][i];

    void* pMappedData = nullptr;
    m_pImmediateContext->MapBuffer(m_pVSConstants, MAP_WRITE, MAP_FLAG_DISCARD, pMappedData);
    if (pMappedData) {
        memcpy(pMappedData, &worldViewProjT.m[0][0], sizeof(Matrix4x4));
        m_pImmediateContext->UnmapBuffer(m_pVSConstants, MAP_WRITE);
    }
}

void DiligentRenderer::RenderFrame() {
    if (!m_pDevice || !m_pSwapChain) {
        return;
    }
    
    UpdateTransforms();
    
    m_pImmediateContext->SetRenderTargets(1, &m_pRTV, m_pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    const float ClearColor[] = {0.2f, 0.4f, 0.6f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(m_pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(m_pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    Viewport VP;
    VP.TopLeftX = 0;
    VP.TopLeftY = 0;
    VP.Width = static_cast<float>(m_Width);
    VP.Height = static_cast<float>(m_Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    m_pImmediateContext->SetViewports(1, &VP, 0, 0);
    
    m_pImmediateContext->SetPipelineState(m_pPSO);
    
    Uint64 offset = 0;
    IBuffer* pBuffs[] = {m_pVertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_pIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType = VT_UINT32;
    DrawAttrs.NumIndices = sizeof(CubeIndices)/sizeof(CubeIndices[0]);
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
    
    m_pSwapChain->Present();
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

void DiligentRenderer::Shutdown() {
    MOON_LOG_INFO("DiligentRenderer", "Shutting down DiligentRenderer");
    
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
