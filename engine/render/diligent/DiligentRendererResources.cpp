#include "DiligentRenderer.h"
#include "../../core/Logging/Logger.h"
#include "../../core/Mesh/Mesh.h"
#include "../../core/Texture/TextureManager.h"

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/TextureView.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"

using namespace Diligent;

// ======= Mesh 缓存 =======
DiligentRenderer::MeshGPUResources* DiligentRenderer::GetOrCreateMeshResources(Moon::Mesh* mesh)
{
    auto it = m_MeshCache.find(mesh);
    if (it != m_MeshCache.end()) return &it->second;

    MeshGPUResources gpu{};
    const auto& vertices = mesh->GetVertices();
    const auto& indices = mesh->GetIndices();

    // VB
    BufferDesc vb{};
    vb.Name = "Mesh VB";
    vb.BindFlags = BIND_VERTEX_BUFFER;
    vb.Usage = USAGE_IMMUTABLE;
    vb.Size = static_cast<Uint32>(vertices.size() * sizeof(vertices[0]));
    BufferData vbData{ vertices.data(), vb.Size };
    m_pDevice->CreateBuffer(vb, &vbData, &gpu.VB);

    // IB
    BufferDesc ib{};
    ib.Name = "Mesh IB";
    ib.BindFlags = BIND_INDEX_BUFFER;
    ib.Usage = USAGE_IMMUTABLE;
    ib.Size = static_cast<Uint32>(indices.size() * sizeof(uint32_t));
    BufferData ibData{ indices.data(), ib.Size };
    m_pDevice->CreateBuffer(ib, &ibData, &gpu.IB);

    gpu.VertexCount = vertices.size();
    gpu.IndexCount = indices.size();

    auto [insIt, ok] = m_MeshCache.emplace(mesh, std::move(gpu));
    MOON_LOG_INFO("DiligentRenderer", "Mesh uploaded: %zu verts, %zu indices", insIt->second.VertexCount, insIt->second.IndexCount);
    return &insIt->second;
}

// ======= 纹理缓存 =======
DiligentRenderer::TextureGPUResources* DiligentRenderer::GetOrCreateTextureResources(const std::string& path, bool isSRGB)
{
    auto it = m_TextureCache.find(path);
    if (it != m_TextureCache.end()) return &it->second;

    Moon::TextureManager texMgr;
    std::shared_ptr<Moon::Texture> texture = texMgr.LoadTexture(path, isSRGB);
    if (!texture || !texture->GetData()) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to load texture: %s", path.c_str());
        return nullptr;
    }

    const Moon::TextureData* texData = texture->GetData().get();

    TEXTURE_FORMAT diligentFormat = TEX_FORMAT_RGBA8_UNORM;
    Uint32 pixelStride = 4;
    
    switch (texData->format) {
        case Moon::TextureFormat::R8:
            diligentFormat = TEX_FORMAT_R8_UNORM;
            pixelStride = 1;
            break;
        case Moon::TextureFormat::RG8:
            diligentFormat = TEX_FORMAT_RG8_UNORM;
            pixelStride = 2;
            break;
        case Moon::TextureFormat::RGB8:
            diligentFormat = isSRGB ? TEX_FORMAT_RGBA8_UNORM_SRGB : TEX_FORMAT_RGBA8_UNORM;
            pixelStride = 4;
            MOON_LOG_WARN("DiligentRenderer", "RGB8 format needs conversion to RGBA8: %s", path.c_str());
            break;
        case Moon::TextureFormat::RGBA8:
            diligentFormat = isSRGB ? TEX_FORMAT_RGBA8_UNORM_SRGB : TEX_FORMAT_RGBA8_UNORM;
            pixelStride = 4;
            break;
        case Moon::TextureFormat::RGBA16F:
            diligentFormat = TEX_FORMAT_RGBA16_FLOAT;
            pixelStride = 8;
            break;
        case Moon::TextureFormat::RGBA32F:
            diligentFormat = TEX_FORMAT_RGBA32_FLOAT;
            pixelStride = 16;
            break;
    }

    TextureDesc desc{};
    desc.Name = path.c_str();
    desc.Type = RESOURCE_DIM_TEX_2D;
    desc.Width = texData->width;
    desc.Height = texData->height;
    desc.Format = diligentFormat;
    desc.MipLevels = 1;
    desc.Usage = USAGE_DEFAULT;
    desc.BindFlags = BIND_SHADER_RESOURCE;

    TextureData initData{};
    TextureSubResData subRes{};
    subRes.pData = texData->pixels.data();
    subRes.Stride = texData->width * pixelStride;
    initData.pSubResources = &subRes;
    initData.NumSubresources = 1;

    TextureGPUResources gpu{};
    m_pDevice->CreateTexture(desc, &initData, &gpu.Texture);

    if (!gpu.Texture) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create GPU texture: %s", path.c_str());
        return nullptr;
    }

    // 创建采样器
    SamplerDesc samplerDesc{};
    samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = TEXTURE_ADDRESS_WRAP;
    
    RefCntAutoPtr<ISampler> pSampler;
    m_pDevice->CreateSampler(samplerDesc, &pSampler);

    // 创建带采样器的纹理视图
    gpu.SRV = gpu.Texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    gpu.SRV->SetSampler(pSampler);

    auto [insIt, ok] = m_TextureCache.emplace(path, std::move(gpu));
    MOON_LOG_INFO("DiligentRenderer", "Texture uploaded: %s (%dx%d, format=%d, sRGB=%d)", 
                  path.c_str(), texData->width, texData->height, (int)texData->format, (int)isSRGB);
    return &insIt->second;
}

// ======= 绑定纹理 =======
void DiligentRenderer::BindAlbedoTexture(const std::string& texturePath)
{
    // 根据当前渲染状态选择SRB
    auto* srb = m_IsRenderingTransparent ? m_pTransparentSRB.RawPtr() : m_pSRB.RawPtr();
    
    if (texturePath.empty() || !srb) {
        if (m_pDefaultWhiteTextureSRV) {
            auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_AlbedoMap");
            if (var) {
                var->Set(m_pDefaultWhiteTextureSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
        }
        return;
    }
    
    auto* texGPU = GetOrCreateTextureResources(texturePath, true);
    if (texGPU && texGPU->SRV) {
        auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_AlbedoMap");
        if (var) {
            var->Set(texGPU->SRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        }
    } else {
        if (m_pDefaultWhiteTextureSRV) {
            auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_AlbedoMap");
            if (var) {
                var->Set(m_pDefaultWhiteTextureSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
        }
    }
}

void DiligentRenderer::BindAOTexture(const std::string& texturePath)
{
    auto* srb = m_IsRenderingTransparent ? m_pTransparentSRB.RawPtr() : m_pSRB.RawPtr();
    
    if (texturePath.empty() || !srb) {
        if (m_pDefaultWhiteTextureSRV) {
            auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_AOMap");
            if (var) {
                var->Set(m_pDefaultWhiteTextureSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
        }
        return;
    }
    
    auto* texGPU = GetOrCreateTextureResources(texturePath, false);
    if (texGPU && texGPU->SRV) {
        auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_AOMap");
        if (var) {
            var->Set(texGPU->SRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        }
    } else {
        if (m_pDefaultWhiteTextureSRV) {
            auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_AOMap");
            if (var) {
                var->Set(m_pDefaultWhiteTextureSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
        }
    }
}

void DiligentRenderer::BindRoughnessTexture(const std::string& texturePath)
{
    auto* srb = m_IsRenderingTransparent ? m_pTransparentSRB.RawPtr() : m_pSRB.RawPtr();
    
    if (texturePath.empty() || !srb) {
        if (m_pDefaultWhiteTextureSRV) {
            auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_RoughnessMap");
            if (var) {
                var->Set(m_pDefaultWhiteTextureSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
        }
        return;
    }
    
    auto* texGPU = GetOrCreateTextureResources(texturePath, false);
    if (texGPU && texGPU->SRV) {
        auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_RoughnessMap");
        if (var) {
            var->Set(texGPU->SRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        }
    } else {
        if (m_pDefaultWhiteTextureSRV) {
            auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_RoughnessMap");
            if (var) {
                var->Set(m_pDefaultWhiteTextureSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
        }
    }
}

void DiligentRenderer::BindMetalnessTexture(const std::string& texturePath)
{
    auto* srb = m_IsRenderingTransparent ? m_pTransparentSRB.RawPtr() : m_pSRB.RawPtr();
    
    if (texturePath.empty() || !srb) {
        if (m_pDefaultWhiteTextureSRV) {
            auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_MetalnessMap");
            if (var) {
                var->Set(m_pDefaultWhiteTextureSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
        }
        return;
    }
    
    auto* texGPU = GetOrCreateTextureResources(texturePath, false);
    if (texGPU && texGPU->SRV) {
        auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_MetalnessMap");
        if (var) {
            var->Set(texGPU->SRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        }
    } else {
        if (m_pDefaultWhiteTextureSRV) {
            auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_MetalnessMap");
            if (var) {
                var->Set(m_pDefaultWhiteTextureSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
        }
    }
}

void DiligentRenderer::BindNormalTexture(const std::string& texturePath)
{
    auto* srb = m_IsRenderingTransparent ? m_pTransparentSRB.RawPtr() : m_pSRB.RawPtr();
    
    if (texturePath.empty() || !srb) {
        if (m_pDefaultWhiteTextureSRV) {
            auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_NormalMap");
            if (var) {
                var->Set(m_pDefaultWhiteTextureSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
        }
        return;
    }
    
    auto* texGPU = GetOrCreateTextureResources(texturePath, false);
    if (texGPU && texGPU->SRV) {
        auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_NormalMap");
        if (var) {
            var->Set(texGPU->SRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        }
    } else {
        if (m_pDefaultWhiteTextureSRV) {
            auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_NormalMap");
            if (var) {
                var->Set(m_pDefaultWhiteTextureSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
        }
    }
}
