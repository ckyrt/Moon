#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace Moon {

/**
 * @brief 纹理格式枚举
 */
enum class TextureFormat {
    Unknown = 0,
    R8,          // 单通道 8-bit
    RG8,         // 双通道 8-bit
    RGB8,        // RGB 8-bit (不常用，通常用 RGBA8)
    RGBA8,       // RGBA 8-bit
    R16F,        // 单通道 16-bit float
    RG16F,       // 双通道 16-bit float
    RGBA16F,     // RGBA 16-bit float
    R32F,        // 单通道 32-bit float
    RG32F,       // 双通道 32-bit float
    RGBA32F,     // RGBA 32-bit float (HDR)
};

/**
 * @brief 纹理过滤模式
 */
enum class TextureFilter {
    Point,       // 点采样（最近邻）
    Bilinear,    // 双线性过滤
    Trilinear,   // 三线性过滤（使用 mipmap）
};

/**
 * @brief 纹理包裹模式
 */
enum class TextureWrapMode {
    Repeat,      // 重复
    Clamp,       // 边缘拉伸
    Mirror,      // 镜像重复
};

/**
 * @brief 纹理数据（CPU 端）
 * 
 * 保存从文件加载的原始像素数据
 * 类似 Unity 的 Texture2D 或 Unreal 的 FTexture2DMipMap
 */
struct TextureData {
    // 像素数据（Mipmap Level 0）
    std::vector<unsigned char> pixels;
    
    // 纹理属性
    int width = 0;
    int height = 0;
    TextureFormat format = TextureFormat::RGBA8;
    
    // 颜色空间
    bool sRGB = true;  // Albedo 贴图用 sRGB，Normal/Metallic/Roughness 用 Linear
    
    // 采样设置（可选，默认值）
    TextureFilter filterMode = TextureFilter::Trilinear;
    TextureWrapMode wrapMode = TextureWrapMode::Repeat;
    
    // Mipmap（可选，通常由 GPU 自动生成）
    bool generateMipmaps = true;
    
    /**
     * @brief 检查纹理数据是否有效
     */
    bool IsValid() const { 
        return !pixels.empty() && width > 0 && height > 0 && format != TextureFormat::Unknown; 
    }
    
    /**
     * @brief 获取单个像素的字节数
     */
    int GetBytesPerPixel() const {
        switch (format) {
            case TextureFormat::R8: return 1;
            case TextureFormat::RG8: return 2;
            case TextureFormat::RGB8: return 3;
            case TextureFormat::RGBA8: return 4;
            case TextureFormat::R16F: return 2;
            case TextureFormat::RG16F: return 4;
            case TextureFormat::RGBA16F: return 8;
            case TextureFormat::R32F: return 4;
            case TextureFormat::RG32F: return 8;
            case TextureFormat::RGBA32F: return 16;
            default: return 0;
        }
    }
    
    /**
     * @brief 获取总数据大小（字节）
     */
    size_t GetDataSize() const {
        return width * height * GetBytesPerPixel();
    }
};

/**
 * @brief 纹理句柄
 * 
 * 用于标识一个纹理资源
 * 实际的 GPU 资源由 Renderer 管理
 */
class Texture {
public:
    Texture(const std::string& path, std::shared_ptr<TextureData> data)
        : m_path(path), m_data(data) {}
    
    const std::string& GetPath() const { return m_path; }
    std::shared_ptr<TextureData> GetData() const { return m_data; }
    
    // GPU 资源 ID（由 Renderer 设置）
    void SetGPUResourceID(uint64_t id) { m_gpuResourceID = id; }
    uint64_t GetGPUResourceID() const { return m_gpuResourceID; }
    bool HasGPUResource() const { return m_gpuResourceID != 0; }
    
private:
    std::string m_path;
    std::shared_ptr<TextureData> m_data;
    uint64_t m_gpuResourceID = 0;  // GPU 资源 ID（renderer 管理）
};

/**
 * @brief 纹理管理器（CPU 端）
 * 
 * 职责：
 * 1. 从文件加载纹理数据到内存
 * 2. 管理纹理的缓存和生命周期
 * 3. 提供纹理查询接口
 * 
 * 不涉及任何 GPU 资源管理（由 Renderer 负责）
 */
class TextureManager {
public:
    TextureManager() = default;
    ~TextureManager() = default;
    
    /**
     * @brief 从文件加载纹理
     * @param filepath 纹理文件路径（相对于 assets 目录）
     * @param sRGB 是否使用 sRGB 颜色空间（Albedo 贴图应该使用 true）
     * @return 纹理句柄，加载失败返回 nullptr
     */
    std::shared_ptr<Texture> LoadTexture(const std::string& filepath, bool sRGB = true);
    
    /**
     * @brief 获取已加载的纹理
     * @param filepath 纹理文件路径
     * @return 纹理句柄，未找到返回 nullptr
     */
    std::shared_ptr<Texture> GetTexture(const std::string& filepath);
    
    /**
     * @brief 清空所有缓存
     */
    void ClearCache();
    
    /**
     * @brief 获取缓存的纹理数量
     */
    size_t GetCachedTextureCount() const { return m_textureCache.size(); }
    
    /**
     * @brief 设置资源根路径（相对路径的基准目录）
     * @param path 资源根路径（通常是exe所在目录）
     */
    static void SetResourceBasePath(const std::string& path);
    
    /**
     * @brief 获取资源根路径
     */
    static const std::string& GetResourceBasePath();
    
private:
    /**
     * @brief 从文件加载纹理数据（使用 stb_image）
     */
    std::shared_ptr<TextureData> LoadTextureData(const std::string& filepath, bool sRGB);
    
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textureCache;
    
    static std::string s_resourceBasePath;  // 资源根路径
};

} // namespace Moon
