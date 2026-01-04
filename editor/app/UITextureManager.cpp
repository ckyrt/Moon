#include "UITextureManager.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "../engine/core/Logging/Logger.h"

using namespace Diligent;

UITextureManager::UITextureManager(IRenderDevice* device, IDeviceContext* context)
    : m_pDevice(device), m_pContext(context), m_Width(0), m_Height(0)
{
}

void UITextureManager::Initialize(int width, int height)
{
    if (width <= 0 || height <= 0) {
        MOON_LOG_ERROR("UITextureManager", "Invalid texture size: %dx%d", width, height);
        return;
    }

    m_Width = width;
    m_Height = height;
    CreateTexture();

    MOON_LOG_INFO("UITextureManager", "Initialized with size %dx%d", width, height);
}

void UITextureManager::UpdateTextureData(const void* buffer, int width, int height)
{
    if (!buffer) {
        MOON_LOG_ERROR("UITextureManager", "Null buffer");
        return;
    }

    if (!m_pUITexture) {
        MOON_LOG_ERROR("UITextureManager", "Texture not created");
        return;
    }
    
    // 如果CEF渲染尺寸与纹理不匹配，自动调整纹理（CEF可能需要几帧来调整到正确尺寸）
    if (width != m_Width || height != m_Height) {
        MOON_LOG_INFO("UITextureManager", "CEF render size changed: %dx%d -> %dx%d, adjusting texture", 
            m_Width, m_Height, width, height);
        m_Width = width;
        m_Height = height;
        CreateTexture();
    }

    // 更新纹理数据
    TextureSubResData subresData;
    subresData.pData = buffer;
    subresData.Stride = width * 4;  // BGRA = 4 bytes per pixel

    Box updateBox;
    updateBox.MinX = 0;
    updateBox.MaxX = width;
    updateBox.MinY = 0;
    updateBox.MaxY = height;

    m_pContext->UpdateTexture(
        m_pUITexture,
        0, 0,
        updateBox,
        subresData,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION
    );
}

void UITextureManager::Resize(int width, int height)
{
    if (width <= 0 || height <= 0) {
        MOON_LOG_INFO("UITextureManager", "Invalid resize dimensions: %dx%d", width, height);
        return;
    }

    if (width == m_Width && height == m_Height) {
        return;  // 尺寸没变，不需要重新创建
    }

    MOON_LOG_INFO("UITextureManager", "Resizing texture from %dx%d to %dx%d", 
        m_Width, m_Height, width, height);

    m_Width = width;
    m_Height = height;
    CreateTexture();
}

void UITextureManager::CreateTexture()
{
    // 释放旧纹理
    m_pUITexture.Release();
    m_pUITextureSRV.Release();

    if (m_Width <= 0 || m_Height <= 0) {
        return;
    }

    // 创建纹理
    TextureDesc texDesc;
    texDesc.Name = "CEF UI Texture";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = m_Width;
    texDesc.Height = m_Height;
    texDesc.Format = TEX_FORMAT_BGRA8_UNORM_SRGB;  // 使用BGRA格式匹配CEF的OSR输出
    texDesc.MipLevels = 1;
    texDesc.Usage = USAGE_DEFAULT;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = CPU_ACCESS_NONE;

    m_pDevice->CreateTexture(texDesc, nullptr, &m_pUITexture);

    if (!m_pUITexture) {
        MOON_LOG_ERROR("UITextureManager", "Failed to create texture");
        return;
    }

    // 创建 SRV
    m_pUITextureSRV = m_pUITexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    MOON_LOG_INFO("UITextureManager", "Created texture %dx%d", m_Width, m_Height);
}
