#include "DiligentRenderer.h"
#include <iostream>
#include <vector>
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
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/TextureView.h"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Common/interface/RefCntAutoPtr.hpp"

using namespace Diligent;

// Cube vertex data with position and color
struct Vertex {
    float position[3];
    float color[4];  // 改为 4 个分量匹配 float4
};

// Cube vertices (8 vertices for a cube)
static const Vertex CubeVertices[] = {
    // Front face
    {{-1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}, // 0: Red
    {{ 1.0f, -1.0f,  1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}}, // 1: Green
    {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}, // 2: Blue
    {{-1.0f,  1.0f,  1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}, // 3: Yellow
    
    // Back face
    {{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}}, // 4: Magenta
    {{ 1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}}, // 5: Cyan
    {{ 1.0f,  1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 6: White
    {{-1.0f,  1.0f, -1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}}  // 7: Gray
};

// Cube indices (12 triangles = 36 indices)
static const uint32_t CubeIndices[] = {
    // Front face
    0, 1, 2,  2, 3, 0,
    // Right face
    1, 5, 6,  6, 2, 1,
    // Back face
    7, 6, 5,  5, 4, 7,
    // Left face
    4, 0, 3,  3, 7, 4,
    // Bottom face
    4, 5, 1,  1, 0, 4,
    // Top face
    3, 2, 6,  6, 7, 3
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
    
    result.m[0][0] = 1.0f / (aspect * tanHalfFov);
    result.m[1][1] = 1.0f / tanHalfFov;
    result.m[2][2] = farZ / (farZ - nearZ);
    result.m[2][3] = 1.0f;
    result.m[3][2] = -(farZ * nearZ) / (farZ - nearZ);
    result.m[3][3] = 0.0f;
    
    return result;
}

DiligentRenderer::Matrix4x4 DiligentRenderer::Matrix4x4::LookAt(float eyeX, float eyeY, float eyeZ, 
                                                               float atX, float atY, float atZ,
                                                               float upX, float upY, float upZ) {
    Matrix4x4 result;
    
    // Calculate forward vector (from eye to target)
    float fX = atX - eyeX;
    float fY = atY - eyeY;
    float fZ = atZ - eyeZ;
    
    // Normalize forward
    float fLen = sqrt(fX*fX + fY*fY + fZ*fZ);
    fX /= fLen; fY /= fLen; fZ /= fLen;
    
    // Calculate right vector (cross product of forward and up)
    float rX = fY * upZ - fZ * upY;
    float rY = fZ * upX - fX * upZ;
    float rZ = fX * upY - fY * upX;
    
    // Normalize right
    float rLen = sqrt(rX*rX + rY*rY + rZ*rZ);
    rX /= rLen; rY /= rLen; rZ /= rLen;
    
    // Recalculate up vector
    upX = rY * fZ - rZ * fY;
    upY = rZ * fX - rX * fZ;
    upZ = rX * fY - rY * fX;
    
    // Build view matrix
    result.m[0][0] = rX;  result.m[0][1] = upX; result.m[0][2] = -fX; result.m[0][3] = 0.0f;
    result.m[1][0] = rY;  result.m[1][1] = upY; result.m[1][2] = -fY; result.m[1][3] = 0.0f;
    result.m[2][0] = rZ;  result.m[2][1] = upZ; result.m[2][2] = -fZ; result.m[2][3] = 0.0f;
    result.m[3][0] = -(rX*eyeX + rY*eyeY + rZ*eyeZ);
    result.m[3][1] = -(upX*eyeX + upY*eyeY + upZ*eyeZ);
    result.m[3][2] = -(-fX*eyeX + -fY*eyeY + -fZ*eyeZ);
    result.m[3][3] = 1.0f;
    
    return result;
}

DiligentRenderer::Matrix4x4 DiligentRenderer::Matrix4x4::RotationY(float angle) {
    Matrix4x4 result;
    float c = cos(angle);
    float s = sin(angle);
    
    result.m[0][0] = c;    result.m[0][2] = s;
    result.m[2][0] = -s;   result.m[2][2] = c;
    
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

    try {
        std::cout << "Initializing DiligentRenderer..." << std::endl;
        
        // Create Diligent Engine factory
        auto* pFactoryD3D11 = GetEngineFactoryD3D11();
        
        // Create render device and device context
        EngineD3D11CreateInfo EngineCI;
        pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &m_pDevice, &m_pImmediateContext);
        std::cout << "D3D11 device and context created" << std::endl;
        
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
        std::cout << "Swap chain created" << std::endl;
        
        // Get render target and depth stencil views
        m_pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        m_pDSV = m_pSwapChain->GetDepthBufferDSV();
        
        // Create pipeline state and resources
        std::cout << "Creating vertex buffer..." << std::endl;
        CreateVertexBuffer();
        std::cout << "Creating index buffer..." << std::endl;
        CreateIndexBuffer();
        std::cout << "Creating uniform buffer..." << std::endl;
        CreateUniformBuffer();
        std::cout << "Creating pipeline state..." << std::endl;
        CreatePipelineState();
        
        std::cout << "DiligentRenderer initialized successfully!" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize DiligentRenderer: " << e.what() << std::endl;
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
        ShaderCI.Desc.Name = "Cube vertex shader";
        ShaderCI.Source = VSSource;
        m_pDevice->CreateShader(ShaderCI, &pVS);
        std::cout << "Vertex shader created" << std::endl;
    }

    // Create pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.Desc.Name = "Cube pixel shader";
        ShaderCI.Source = PSSource;
        m_pDevice->CreateShader(ShaderCI, &pPS);
        std::cout << "Pixel shader created" << std::endl;
    }

    // Define vertex input layout
    LayoutElement LayoutElems[] = {
        {0, 0, 3, VT_FLOAT32, False, 0},  // Position
        {1, 0, 4, VT_FLOAT32, False, 12}  // Color (改为 4 个分量)
    };

    // Create graphics pipeline state
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = "Cube PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

    // 设置默认变量类型为 STATIC，这样 Constants 会被自动识别
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);
    std::cout << "Pipeline state created" << std::endl;

    // 按照官方示例的方式绑定常量缓冲区
    // 由于我们没有显式指定 'Constants' 变量的类型，它会使用默认类型 STATIC
    // Static 变量永远不会改变，通过 pipeline state object 直接绑定
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pVSConstants);
    std::cout << "Constants buffer bound to pipeline state" << std::endl;

    // Create shader resource binding object and bind all static resources
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
    std::cout << "Shader resource binding created" << std::endl;
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
}

void DiligentRenderer::CreateUniformBuffer() {
    // 按照官方示例创建常量缓冲区
    BufferDesc CBDesc;
    CBDesc.Name = "VS constants CB";
    CBDesc.Size = sizeof(Matrix4x4);  // 只存储一个 4x4 矩阵
    CBDesc.Usage = USAGE_DYNAMIC;
    CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    m_pDevice->CreateBuffer(CBDesc, nullptr, &m_pVSConstants);
    std::cout << "Uniform buffer created" << std::endl;
}

void DiligentRenderer::UpdateTransforms() {
    // Calculate time-based rotation
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(currentTime - m_StartTime).count();
    
    // Create transformation matrices
    Matrix4x4 world = Matrix4x4::RotationY(time * 1.2f) * Matrix4x4::Translation(0.0f, 0.0f, 0.0f);
    Matrix4x4 view = Matrix4x4::LookAt(0.0f, 3.0f, -5.0f,  // Eye position
                                      0.0f, 0.0f, 0.0f,    // Look at point
                                      0.0f, 1.0f, 0.0f);   // Up vector
    Matrix4x4 proj = Matrix4x4::Perspective(3.14159f * 0.25f, // 45 degrees
                                          (float)m_Width / (float)m_Height,
                                          0.1f, 100.0f);
    
    Matrix4x4 worldViewProj = world * view * proj;
    
    // Update uniform buffer - 按照官方示例的方式
    void* pMappedData = nullptr;
    m_pImmediateContext->MapBuffer(m_pVSConstants, MAP_WRITE, MAP_FLAG_DISCARD, pMappedData);
    if (pMappedData) {
        // 直接复制矩阵数据
        memcpy(pMappedData, &worldViewProj.m[0][0], sizeof(Matrix4x4));
        m_pImmediateContext->UnmapBuffer(m_pVSConstants, MAP_WRITE);
    }
}

void DiligentRenderer::RenderFrame() {
    if (!m_pDevice || !m_pSwapChain) {
        std::cerr << "Device or SwapChain is null!" << std::endl;
        return;
    }
    
    // Update transformations
    UpdateTransforms();
    
    // Set render targets
    m_pImmediateContext->SetRenderTargets(1, &m_pRTV, m_pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // Clear render target and depth buffer
    const float ClearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
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
    
    // Check if all resources are valid
    if (!m_pPSO || !m_pVertexBuffer || !m_pIndexBuffer || !m_pSRB) {
        static bool errorLogged = false;
        if (!errorLogged) {
            std::cerr << "Missing render resources!" << std::endl;
            std::cerr << "PSO: " << (m_pPSO ? "OK" : "NULL") << std::endl;
            std::cerr << "VertexBuffer: " << (m_pVertexBuffer ? "OK" : "NULL") << std::endl;
            std::cerr << "IndexBuffer: " << (m_pIndexBuffer ? "OK" : "NULL") << std::endl;
            std::cerr << "SRB: " << (m_pSRB ? "OK" : "NULL") << std::endl;
            errorLogged = true;
        }
        m_pSwapChain->Present();
        return;
    }
    
    // Set pipeline state and vertex/index buffers
    m_pImmediateContext->SetPipelineState(m_pPSO);
    
    Uint64 offset = 0;
    IBuffer* pBuffs[] = {m_pVertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_pIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // Commit shader resources
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // Draw the cube
    DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType = VT_UINT32;
    DrawAttrs.NumIndices = _countof(CubeIndices);
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
    
    // Present
    m_pSwapChain->Present();
}

void DiligentRenderer::Resize(uint32_t w, uint32_t h) {
    if (m_Width == w && m_Height == h) return;
    
    m_Width = w;
    m_Height = h;
    
    if (m_pSwapChain) {
        m_pSwapChain->Resize(m_Width, m_Height);
        m_pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        m_pDSV = m_pSwapChain->GetDepthBufferDSV();
    }
}

void DiligentRenderer::Shutdown() {
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
    
#ifdef _WIN32
    m_hWnd = nullptr;
#endif
}