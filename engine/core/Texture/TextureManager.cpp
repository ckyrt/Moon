#include "TextureManager.h"
#include "../Assets/AssetPaths.h"
#include "../Logging/Logger.h"

// 声明 stb_image 函数（实际定义在 DiligentRenderer.cpp 中）
extern "C" {
    unsigned char* stbi_load(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels);
    void stbi_image_free(void* retval_from_stbi_load);
    const char* stbi_failure_reason(void);
}

#include <cstring>
#include <fstream>

#ifdef _WIN32
#include <direct.h>  // for _getcwd
#else
#include <unistd.h>  // for getcwd
#endif

namespace Moon {

// 静态成员变量定义
std::string TextureManager::s_resourceBasePath = "";

std::shared_ptr<Texture> TextureManager::LoadTexture(const std::string& filepath, bool sRGB)
{
    // 检查缓存
    auto it = m_textureCache.find(filepath);
    if (it != m_textureCache.end()) {
        MOON_LOG_INFO("TextureManager", "Texture loaded from cache: %s", filepath.c_str());
        return it->second;
    }
    
    // 加载纹理数据
    auto textureData = LoadTextureData(filepath, sRGB);
    if (!textureData || !textureData->IsValid()) {
        MOON_LOG_ERROR("TextureManager", "Failed to load texture: %s", filepath.c_str());
        return nullptr;
    }
    
    // 创建纹理句柄
    auto texture = std::make_shared<Texture>(filepath, textureData);
    m_textureCache[filepath] = texture;
    
    MOON_LOG_INFO("TextureManager", "Texture loaded: %s (%dx%d, format: %s, sRGB: %s)", 
                  filepath.c_str(), 
                  textureData->width, 
                  textureData->height,
                  textureData->format == TextureFormat::RGBA8 ? "RGBA8" : "Unknown",
                  textureData->sRGB ? "true" : "false");
    
    return texture;
}

std::shared_ptr<Texture> TextureManager::GetTexture(const std::string& filepath)
{
    auto it = m_textureCache.find(filepath);
    if (it != m_textureCache.end()) {
        return it->second;
    }
    return nullptr;
}

void TextureManager::ClearCache()
{
    m_textureCache.clear();
    MOON_LOG_INFO("TextureManager", "Texture cache cleared");
}

std::shared_ptr<TextureData> TextureManager::LoadTextureData(const std::string& filepath, bool sRGB)
{
    // 构建完整路径：优先使用显式资源根，否则使用固定 assets 根路径
    std::string fullPath;
    if (!s_resourceBasePath.empty()) {
        fullPath = s_resourceBasePath + "/assets/textures/" + filepath;
    } else {
        fullPath = Moon::Assets::BuildTexturePath(filepath);
    }
    MOON_LOG_INFO("TextureManager", "Attempting to load texture: %s", fullPath.c_str());
    
    // 使用 stb_image 加载图像
    int width, height, channels;
    unsigned char* imageData = stbi_load(fullPath.c_str(), &width, &height, &channels, 4); // 强制 RGBA
    
    if (!imageData) {
        MOON_LOG_ERROR("TextureManager", "Failed to load image: %s (reason: %s)", 
                       fullPath.c_str(), stbi_failure_reason());
        return nullptr;
    }
    
    MOON_LOG_INFO("TextureManager", "Successfully loaded texture: %s (%dx%d, %d channels)", 
                  fullPath.c_str(), width, height, channels);
    
    // 创建纹理数据
    auto textureData = std::make_shared<TextureData>();
    textureData->width = width;
    textureData->height = height;
    textureData->format = TextureFormat::RGBA8;  // stb_image 强制加载为 RGBA
    textureData->sRGB = sRGB;
    textureData->generateMipmaps = true;
    textureData->filterMode = TextureFilter::Trilinear;
    textureData->wrapMode = TextureWrapMode::Repeat;
    
    // 复制像素数据
    size_t dataSize = width * height * 4;  // RGBA = 4 bytes per pixel
    textureData->pixels.resize(dataSize);
    std::memcpy(textureData->pixels.data(), imageData, dataSize);
    
    // 释放 stb_image 数据
    stbi_image_free(imageData);
    
    return textureData;
}

void TextureManager::SetResourceBasePath(const std::string& path)
{
    s_resourceBasePath = path;
    MOON_LOG_INFO("TextureManager", "Resource base path set to: %s", path.c_str());
}

const std::string& TextureManager::GetResourceBasePath()
{
    return s_resourceBasePath;
}

} // namespace Moon
