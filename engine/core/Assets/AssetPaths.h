#pragma once

#include <string>

namespace Moon {
namespace Assets {

static const char* kProjectRootPath = "E:/game_engine/Moon";
static const char* kAssetRootPath = "E:/game_engine/Moon/assets";

inline bool IsAbsolutePath(const std::string& path)
{
    return path.size() > 1 && path[1] == ':';
}

inline std::string NormalizeSlashes(std::string path)
{
    for (char& ch : path) {
        if (ch == '\\') {
            ch = '/';
        }
    }
    return path;
}

inline std::string BuildAssetPath(const std::string& path)
{
    if (path.empty()) {
        return std::string(kAssetRootPath);
    }

    std::string normalized = NormalizeSlashes(path);
    if (IsAbsolutePath(normalized)) {
        return normalized;
    }

    if (normalized.rfind("assets/", 0) == 0) {
        return std::string(kProjectRootPath) + "/" + normalized;
    }

    return std::string(kAssetRootPath) + "/" + normalized;
}

inline std::string BuildBuildingPath(const std::string& path)
{
    return BuildAssetPath("building/" + path);
}

inline std::string BuildCsgPath(const std::string& path)
{
    return BuildAssetPath("csg/" + path);
}

inline std::string BuildShaderPath(const std::string& path)
{
    return BuildAssetPath("shaders/" + path);
}

inline std::string BuildTexturePath(const std::string& path)
{
    std::string normalized = NormalizeSlashes(path);
    if (IsAbsolutePath(normalized) || normalized.rfind("assets/", 0) == 0 || normalized.rfind("textures/", 0) == 0) {
        return BuildAssetPath(normalized);
    }
    return BuildAssetPath("textures/" + normalized);
}

} // namespace Assets
} // namespace Moon