#pragma once

#include <string>
#include <vector>

struct TerrainData
{
    // 地形分辨率（resolution x resolution）
    int resolution = 0;

    // heightmap.txt
    // size = resolution * resolution
    std::vector<float> heightMap;

    // basemap.txt
    // size = resolution * resolution
    std::vector<float> baseMap;
};


// ======================================================
// River
// ======================================================

struct RiverSpline
{
    std::string id;

    // 点序列：x0,y0,z0,x1,y1,z1,...
    // 与 scene.json 中 points[].position 对齐
    std::vector<float> points;

    // 河道宽度（目前取第一个点的 width）
    float width = 0.0f;

    // ✅ 新增：水深（0 = 贴地）
    float waterDepth = 0.0f;
};


// ======================================================
// Grass
// ======================================================

struct GrassData
{
    // grassmap.png 的原始二进制数据
    // 后续你可以直接丢给 stb_image / GPU 上传
    std::vector<unsigned char> grassMapPNG;
};


// ======================================================
// SceneData（加载结果）
// ======================================================

struct SceneData
{
    TerrainData terrain;
    std::vector<RiverSpline> rivers;
    GrassData grass;
};


// ======================================================
// SceneZipLoader
// ======================================================

class SceneZipLoader
{
public:
    // 从 zip 文件中加载整个场景
    // - 不解压到磁盘
    // - 直接从 zip 内存读取
    //
    // 返回：
    //   true  = 成功
    //   false = zip 结构错误 / 数据不完整
    static bool LoadSceneFromZip(
        const std::string& zipPath,
        SceneData& outScene);
};