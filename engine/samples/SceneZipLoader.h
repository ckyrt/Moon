#pragma once

#include <string>
#include <vector>

// ================= 场景数据结构 =================

// 地形数据（当前版本）
struct TerrainData
{
    int resolution = 0;              // heightmap 分辨率（resolution x resolution）
    std::vector<float> heightMap;    // 行主序 float，高度数据
};

// 河流样条（来自 scene.json）
struct RiverSpline
{
    std::string id;
    std::vector<float> points;       // x,y,z 连续存储
    float width = 0.0f;
};

// 草数据（当前只有分布贴图路径）
struct GrassData
{
    std::vector<unsigned char> grassMapPNG; // png 原始字节
};

// 场景整体
struct SceneData
{
    TerrainData terrain;
    std::vector<RiverSpline> rivers;
    GrassData grass;
};

// ================= Loader =================

class SceneZipLoader
{
public:
    // 从 zip 文件加载 SceneData
    static bool LoadSceneFromZip(
        const std::string& zipPath,
        SceneData& outScene
    );
};
