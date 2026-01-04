#pragma once

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/TextureView.h"
#include "Common/interface/RefCntAutoPtr.hpp"

// UI 纹理管理器
// 管理 CEF 离屏渲染的纹理，负责创建、更新和获取 UI 纹理
class UITextureManager {
public:
    UITextureManager(Diligent::IRenderDevice* device, Diligent::IDeviceContext* context);
    ~UITextureManager() = default;

    // 初始化纹理（设置大小）
    void Initialize(int width, int height);

    // 更新纹理数据（从 CEF OnPaint 回调调用）
    // buffer: BGRA 格式的像素数据
    void UpdateTextureData(const void* buffer, int width, int height);

    // 调整纹理大小
    void Resize(int width, int height);

    // 获取纹理 SRV（用于渲染）
    Diligent::ITextureView* GetTextureSRV() const { return m_pUITextureSRV; }

    // 获取纹理尺寸
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

    // 检查是否已初始化
    bool IsInitialized() const { return m_pUITexture != nullptr; }

private:
    void CreateTexture();

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pContext;
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pUITexture;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pUITextureSRV;
    int m_Width = 0;
    int m_Height = 0;
};
