#include "TextureManager.h"
#include "../Assets/AssetPaths.h"
#include "../Logging/Logger.h"

// 婢圭増妲?stb_image 閸戣姤鏆熼敍鍫濈杽闂勫懎鐣炬稊澶婃躬 DiligentRenderer.cpp 娑擃叏绱?
extern "C" {
    unsigned char* stbi_load(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels);
    void stbi_image_free(void* retval_from_stbi_load);
    const char* stbi_failure_reason(void);
}

#include <cstring>
#include <fstream>

#ifdef _WIN32
#include <direct.h>  // for _getcwd
#include <io.h>
#else
#include <unistd.h>  // for getcwd
#endif

namespace Moon {

namespace {

std::string NormalizePathString(std::string path)
{
    for (char& ch : path) {
        if (ch == '\\') {
            ch = '/';
        }
    }
    return path;
}

bool FileExists(const std::string& path)
{
#ifdef _WIN32
    return _access(path.c_str(), 0) == 0;
#else
    return access(path.c_str(), F_OK) == 0;
#endif
}

std::string JoinPath(const std::string& left, const std::string& right)
{
    if (left.empty()) {
        return NormalizePathString(right);
    }
    if (right.empty()) {
        return NormalizePathString(left);
    }

    if (left.back() == '/' || left.back() == '\\') {
        return NormalizePathString(left + right);
    }
    return NormalizePathString(left + '/' + right);
}

std::string GetTextureRelativePath(const std::string& filepath)
{
    std::string normalized = NormalizePathString(filepath);
    if (normalized.rfind("assets/", 0) == 0) {
        return normalized.substr(7);
    }
    if (normalized.rfind("textures/", 0) == 0) {
        return normalized;
    }
    return "textures/" + normalized;
}

std::string ParentPath(std::string path)
{
    path = NormalizePathString(path);
    while (!path.empty() && path.back() == '/') {
        path.pop_back();
    }

    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return "";
    }
    if (slash == 2 && path.size() > 2 && path[1] == ':') {
        return path.substr(0, slash + 1);
    }
    return path.substr(0, slash);
}

std::string FindAssetsRootFromBase(const std::string& basePath)
{
    std::string current = NormalizePathString(basePath);
    if (current.empty()) {
        return "";
    }

    while (!current.empty()) {
        const std::string candidate = JoinPath(current, "assets");
        if (FileExists(JoinPath(candidate, "textures"))) {
            return candidate;
        }

        const std::string parent = ParentPath(current);
        if (parent.empty() || parent == current) {
            break;
        }
        current = parent;
    }

    return "";
}

std::string ResolveTexturePath(const std::string& filepath, const std::string& basePath)
{
    const std::string normalized = NormalizePathString(filepath);
    if (Moon::Assets::IsAbsolutePath(normalized) && FileExists(normalized)) {
        return normalized;
    }

    const std::string relativeTexturePath = GetTextureRelativePath(normalized);
    std::vector<std::string> candidates;

    if (!basePath.empty()) {
        candidates.push_back(JoinPath(basePath, relativeTexturePath));
        candidates.push_back(JoinPath(basePath, "assets/" + relativeTexturePath));
    }

    const std::string projectAssetsRoot = !basePath.empty() ? FindAssetsRootFromBase(basePath) : std::string();
    if (!projectAssetsRoot.empty()) {
        candidates.push_back(JoinPath(projectAssetsRoot, relativeTexturePath));
    }

    candidates.push_back(Moon::Assets::BuildTexturePath(normalized));
    candidates.push_back(Moon::Assets::BuildTexturePath(relativeTexturePath));

    for (const std::string& candidate : candidates) {
        if (FileExists(candidate)) {
            return candidate;
        }
    }

    return candidates.empty() ? normalized : candidates.front();
}

} // namespace

// 闂堟瑦鈧焦鍨氶崨妯哄綁闁插繐鐣炬稊?
std::string TextureManager::s_resourceBasePath = "";

std::shared_ptr<Texture> TextureManager::LoadTexture(const std::string& filepath, bool sRGB)
{
    // 濡偓閺屻儳绱︾€?
    auto it = m_textureCache.find(filepath);
    if (it != m_textureCache.end()) {
        MOON_LOG_INFO("TextureManager", "Texture loaded from cache: %s", filepath.c_str());
        return it->second;
    }
    
    // 閸旂姾娴囩痪鍦倞閺佺増宓?
    auto textureData = LoadTextureData(filepath, sRGB);
    if (!textureData || !textureData->IsValid()) {
        MOON_LOG_ERROR("TextureManager", "Failed to load texture: %s", filepath.c_str());
        return nullptr;
    }
    
    // 閸掓稑缂撶痪鍦倞閸欍儲鐒?
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
    const std::string fullPath = ResolveTexturePath(filepath, s_resourceBasePath);
    MOON_LOG_INFO("TextureManager", "Attempting to load texture: %s", fullPath.c_str());
    
    int width, height, channels;
    unsigned char* imageData = stbi_load(fullPath.c_str(), &width, &height, &channels, 4);
    
    if (!imageData) {
        MOON_LOG_ERROR("TextureManager", "Failed to load image: %s (reason: %s)", 
                       fullPath.c_str(), stbi_failure_reason());
        return nullptr;
    }
    
    MOON_LOG_INFO("TextureManager", "Successfully loaded texture: %s (%dx%d, %d channels)", 
                  fullPath.c_str(), width, height, channels);
    
    auto textureData = std::make_shared<TextureData>();
    textureData->width = width;
    textureData->height = height;
    textureData->format = TextureFormat::RGBA8;
    textureData->sRGB = sRGB;
    textureData->generateMipmaps = true;
    textureData->filterMode = TextureFilter::Trilinear;
    textureData->wrapMode = TextureWrapMode::Repeat;
    
    size_t dataSize = width * height * 4;
    textureData->pixels.resize(dataSize);
    std::memcpy(textureData->pixels.data(), imageData, dataSize);
    
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
