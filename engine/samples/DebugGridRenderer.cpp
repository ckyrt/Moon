#include "DebugGridRenderer.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "../core/Camera/PerspectiveCamera.h"
#include "../render/diligent/DiligentRenderer.h"
#include "../render/diligent/DiligentRendererUtils.h"

#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"

namespace HelloEngineImGui {

struct DebugGridVertex {
    float pos[3];
    float color[4];
};

struct DebugGridVSConstantsCPU {
    Moon::Matrix4x4 ViewProjT;
};

static void AppendGrid(std::vector<DebugGridVertex>& outVerts,
                       float camX, float camZ,
                       float spacing, float halfSize,
                       float y,
                       float r, float g, float b, float a)
{
    if (spacing <= 0.0f || halfSize <= 0.0f) return;

    const float snapX = std::floor(camX / spacing) * spacing;
    const float snapZ = std::floor(camZ / spacing) * spacing;

    const float xMin = snapX - halfSize;
    const float xMax = snapX + halfSize;
    const float zMin = snapZ - halfSize;
    const float zMax = snapZ + halfSize;

    const float xStart = std::floor(xMin / spacing) * spacing;
    const float xEnd   = std::ceil (xMax / spacing) * spacing;
    const float zStart = std::floor(zMin / spacing) * spacing;
    const float zEnd   = std::ceil (zMax / spacing) * spacing;

    auto pushLine = [&](float x0, float z0, float x1, float z1) {
        DebugGridVertex v0{};
        v0.pos[0] = x0; v0.pos[1] = y; v0.pos[2] = z0;
        v0.color[0] = r; v0.color[1] = g; v0.color[2] = b; v0.color[3] = a;
        DebugGridVertex v1{};
        v1.pos[0] = x1; v1.pos[1] = y; v1.pos[2] = z1;
        v1.color[0] = r; v1.color[1] = g; v1.color[2] = b; v1.color[3] = a;
        outVerts.push_back(v0);
        outVerts.push_back(v1);
    };

    for (float x = xStart; x <= xEnd + 0.0001f; x += spacing) {
        pushLine(x, zStart, x, zEnd);
    }
    for (float z = zStart; z <= zEnd + 0.0001f; z += spacing) {
        pushLine(xStart, z, xEnd, z);
    }
}

struct DebugGridRenderer::Impl {
    DiligentRenderer* renderer = nullptr;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>               vsConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>               vb;
    size_t                                                   vbCapacityVerts = 0;

    void EnsureResources()
    {
        if (!renderer || pso) return;

        auto* device = renderer->GetDevice();
        if (!device) return;

        std::string vsCode = DiligentRendererUtils::LoadShaderSource("DebugGrid.vs.hlsl");
        std::string psCode = DiligentRendererUtils::LoadShaderSource("DebugGrid.ps.hlsl");
        if (vsCode.empty() || psCode.empty()) {
            MOON_LOG_ERROR("DebugGridRenderer", "Failed to load DebugGrid shaders");
            return;
        }

        Diligent::RefCntAutoPtr<Diligent::IShader> vs;
        {
            Diligent::ShaderCreateInfo ci{};
            ci.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            ci.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
            ci.Desc.Name = "DebugGrid VS";
            ci.Source = vsCode.c_str();
            device->CreateShader(ci, &vs);
        }

        Diligent::RefCntAutoPtr<Diligent::IShader> ps;
        {
            Diligent::ShaderCreateInfo ci{};
            ci.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            ci.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
            ci.Desc.Name = "DebugGrid PS";
            ci.Source = psCode.c_str();
            device->CreateShader(ci, &ps);
        }

        Diligent::GraphicsPipelineStateCreateInfo pci{};
        pci.PSODesc.Name = "DebugGrid PSO";
        pci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
        pci.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        pci.GraphicsPipeline.NumRenderTargets = 1;
        pci.GraphicsPipeline.RTVFormats[0] = renderer->GetSwapChain()->GetDesc().ColorBufferFormat;
        pci.GraphicsPipeline.DSVFormat = renderer->GetSwapChain()->GetDesc().DepthBufferFormat;
        pci.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_LINE_LIST;
        pci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
        pci.GraphicsPipeline.DepthStencilDesc.DepthEnable = Diligent::True;
        pci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = Diligent::False;
        pci.GraphicsPipeline.DepthStencilDesc.DepthFunc = Diligent::COMPARISON_FUNC_LESS_EQUAL;

        pci.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = Diligent::True;
        pci.GraphicsPipeline.BlendDesc.RenderTargets[0].SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
        pci.GraphicsPipeline.BlendDesc.RenderTargets[0].DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        pci.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendOp = Diligent::BLEND_OPERATION_ADD;
        pci.GraphicsPipeline.BlendDesc.RenderTargets[0].SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
        pci.GraphicsPipeline.BlendDesc.RenderTargets[0].DestBlendAlpha = Diligent::BLEND_FACTOR_ZERO;
        pci.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;

        Diligent::LayoutElement layout[2];
        layout[0].InputIndex = 0;
        layout[0].BufferSlot = 0;
        layout[0].NumComponents = 3;
        layout[0].ValueType = Diligent::VT_FLOAT32;
        layout[0].IsNormalized = Diligent::False;
        layout[0].RelativeOffset = 0;

        layout[1].InputIndex = 1;
        layout[1].BufferSlot = 0;
        layout[1].NumComponents = 4;
        layout[1].ValueType = Diligent::VT_FLOAT32;
        layout[1].IsNormalized = Diligent::False;
        layout[1].RelativeOffset = sizeof(float) * 3;

        pci.GraphicsPipeline.InputLayout.LayoutElements = layout;
        pci.GraphicsPipeline.InputLayout.NumElements = 2;

        pci.pVS = vs;
        pci.pPS = ps;

        device->CreateGraphicsPipelineState(pci, &pso);
        if (!pso) {
            MOON_LOG_ERROR("DebugGridRenderer", "Failed to create DebugGrid PSO");
            return;
        }

        {
            Diligent::BufferDesc cb{};
            cb.Name = "DebugGrid VS Constants";
            cb.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
            cb.Usage = Diligent::USAGE_DYNAMIC;
            cb.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
            cb.Size = sizeof(DebugGridVSConstantsCPU);
            device->CreateBuffer(cb, nullptr, &vsConstants);
        }

        if (auto* cbVar = pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")) {
            cbVar->Set(vsConstants);
        }

        pso->CreateShaderResourceBinding(&srb, true);

        vbCapacityVerts = 1024;
        {
            Diligent::BufferDesc vbDesc{};
            vbDesc.Name = "DebugGrid VB";
            vbDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
            vbDesc.Usage = Diligent::USAGE_DYNAMIC;
            vbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
            vbDesc.Size = static_cast<Diligent::Uint32>(vbCapacityVerts * sizeof(DebugGridVertex));
            device->CreateBuffer(vbDesc, nullptr, &vb);
        }
    }

    void ReleaseResources()
    {
        vb.Release();
        vsConstants.Release();
        srb.Release();
        pso.Release();
        vbCapacityVerts = 0;
    }
};

DebugGridRenderer::DebugGridRenderer() : m_Impl(std::make_unique<Impl>()) {}
DebugGridRenderer::~DebugGridRenderer() { Shutdown(); }

void DebugGridRenderer::Initialize(DiligentRenderer* renderer)
{
    m_Impl->renderer = renderer;
}

void DebugGridRenderer::Shutdown()
{
    if (!m_Impl) return;
    m_Impl->ReleaseResources();
    m_Impl->renderer = nullptr;
}

void DebugGridRenderer::Render(Moon::PerspectiveCamera* camera)
{
    if (!camera) return;

    m_Impl->EnsureResources();
    if (!m_Impl->pso || !m_Impl->vb || !m_Impl->vsConstants || !m_Impl->renderer) return;

    const auto camPos = camera->GetPosition();
    const float y = 0.001f; // slight offset to reduce z-fighting

    const float fineSpacing = 1.0f;
    const float fineHalfSize = 35.0f;
    const float coarseSpacing = 10.0f;
    const float coarseHalfSize = 350.0f;

    std::vector<DebugGridVertex> verts;
    verts.reserve(2048);

    AppendGrid(verts, camPos.x, camPos.z, coarseSpacing, coarseHalfSize, y,
               0.65f, 0.65f, 0.65f, 0.25f);
    AppendGrid(verts, camPos.x, camPos.z, fineSpacing, fineHalfSize, y,
               0.75f, 0.75f, 0.75f, 0.35f);

    if (verts.empty()) return;

    if (verts.size() > m_Impl->vbCapacityVerts) {
        const size_t doubled = m_Impl->vbCapacityVerts * 2;
        m_Impl->vbCapacityVerts = (doubled > verts.size()) ? doubled : verts.size();

        Diligent::BufferDesc vbDesc{};
        vbDesc.Name = "DebugGrid VB";
        vbDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
        vbDesc.Usage = Diligent::USAGE_DYNAMIC;
        vbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        vbDesc.Size = static_cast<Diligent::Uint32>(m_Impl->vbCapacityVerts * sizeof(DebugGridVertex));
        m_Impl->vb.Release();
        m_Impl->renderer->GetDevice()->CreateBuffer(vbDesc, nullptr, &m_Impl->vb);
        if (!m_Impl->vb) return;
    }

    DebugGridVSConstantsCPU cb{};
    cb.ViewProjT = DiligentRendererUtils::Transpose(camera->GetViewProjectionMatrix());
    DiligentRendererUtils::UpdateConstantBuffer(m_Impl->vsConstants, m_Impl->renderer->GetContext(), &cb, sizeof(cb));

    {
        void* pData = nullptr;
        m_Impl->renderer->GetContext()->MapBuffer(m_Impl->vb, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, verts.data(), verts.size() * sizeof(DebugGridVertex));
        m_Impl->renderer->GetContext()->UnmapBuffer(m_Impl->vb, Diligent::MAP_WRITE);
    }

    auto* ctx = m_Impl->renderer->GetContext();
    ctx->SetPipelineState(m_Impl->pso);
    ctx->CommitShaderResources(m_Impl->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::IBuffer* vbs[] = { m_Impl->vb };
    Diligent::Uint64 offset = 0;
    ctx->SetVertexBuffers(0, 1, vbs, &offset,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                          Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);

    Diligent::DrawAttribs da{};
    da.NumVertices = static_cast<Diligent::Uint32>(verts.size());
    da.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
    ctx->Draw(da);
}

} // namespace HelloEngineImGui
