#include "SceneZipLoader.h"

// ---- Suppress warnings for third-party C library (kuba--/zip) ----
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#pragma warning(disable: 6011)
#pragma warning(disable: 6386)
#pragma warning(disable: 6308)
#pragma warning(disable: 6262)
#endif

#include "../../external/zip/src/zip.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
// -----------------------------------------------------------------

#include "../../external/nlohmann/json.hpp"

#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <Logging/Logger.h>

using json = nlohmann::json;

// ======================================================
// zip helpers
// ======================================================

static bool ReadZipFileToBuffer(
    zip_t* zip,
    const char* pathInZip,
    std::vector<unsigned char>& outBuffer)
{
    if (zip_entry_open(zip, pathInZip) != 0)
        return false;

    void* buf = nullptr;
    size_t bufSize = 0;

    if (zip_entry_read(zip, &buf, &bufSize) < 0)
    {
        zip_entry_close(zip);
        return false;
    }

    outBuffer.resize(bufSize);
    std::memcpy(outBuffer.data(), buf, bufSize);

    free(buf);
    zip_entry_close(zip);
    return true;
}

static bool ReadZipTextFile(
    zip_t* zip,
    const char* pathInZip,
    std::string& outText)
{
    std::vector<unsigned char> buffer;
    if (!ReadZipFileToBuffer(zip, pathInZip, buffer))
        return false;

    outText.assign(reinterpret_cast<char*>(buffer.data()), buffer.size());
    return true;
}

// ======================================================
// Robust txt float parser (matches JS behavior)
// ======================================================

static bool ParseFloatTxt(
    const std::string& text,
    int expectedCount,
    std::vector<float>& outData)
{
    outData.clear();
    outData.reserve(expectedCount);

    std::string token;
    token.reserve(32);

    for (char c : text)
    {
        if (std::isdigit(c) || c == '-' || c == '.' || c == 'e' || c == 'E')
        {
            token.push_back(c);
        }
        else
        {
            if (!token.empty())
            {
                outData.push_back(std::stof(token));
                token.clear();
            }
        }
    }

    if (!token.empty())
        outData.push_back(std::stof(token));

    return (int)outData.size() == expectedCount;
}

// ======================================================
// Main entry
// ======================================================

bool SceneZipLoader::LoadSceneFromZip(
    const std::string& zipPath,
    SceneData& outScene)
{

    zip_t* zip = zip_open(zipPath.c_str(), 0, 'r');
    if (!zip)
        return false;

    // ===============================
    // 动态获取场景文件夹名（去除扩展名）
    // ===============================
    std::string::size_type slash = zipPath.find_last_of("/\\");
    std::string::size_type dot = zipPath.find_last_of('.');
    std::string sceneFolder;
    if (slash == std::string::npos) slash = 0; else slash += 1;
    if (dot == std::string::npos || dot < slash) dot = zipPath.size();
    sceneFolder = zipPath.substr(slash, dot - slash);

    // ==================================================
    // terrain meta
    // ==================================================

    std::string heightMetaText;
    std::string metaPath = sceneFolder + "/terrain/heightmap.meta.json";
    if (!ReadZipTextFile(
        zip,
        metaPath.c_str(),
        heightMetaText))
    {
        zip_close(zip);
        return false;
    }

    json heightMeta = json::parse(heightMetaText);

    // heightMeta 中的 width/height 实际是地形采样分辨率（如 129×129）
    // 不是实际的地形尺寸（固定为 100×100）
    if (!heightMeta.contains("width") || !heightMeta.contains("height")) {
        MOON_LOG_ERROR("SceneZipLoader", "heightmap.meta.json missing width/height (resolution)");
        return false;
    }

    int resolutionW = heightMeta["width"].get<int>();
    int resolutionH = heightMeta["height"].get<int>();

    if (resolutionW != resolutionH) {
        MOON_LOG_WARN("SceneZipLoader", "heightmap resolution not square: %d x %d", resolutionW, resolutionH);
    }

    // 地形采样分辨率（决定细分网格数量）
    int resolution = resolutionW;
    outScene.terrain.resolution = resolution;

    // 地形实际尺寸固定为 100×100（世界坐标单位）
    // TODO: 如需可配置，可从 heightMeta 中读取 "terrainWidth" 和 "terrainHeight"

    // ==================================================
    // heightmap.txt
    // ==================================================


    std::string heightTxt;
    std::string heightPath = sceneFolder + "/terrain/heightmap.txt";
    if (!ReadZipTextFile(
        zip,
        heightPath.c_str(),
        heightTxt))
    {
        zip_close(zip);
        return false;
    }

    if (!ParseFloatTxt(
        heightTxt,
        resolution * resolution,
        outScene.terrain.heightMap))
    {
        zip_close(zip);
        return false;
    }

    // ==================================================
    // basemap.txt
    // ==================================================


    std::string baseTxt;
    std::string basePath = sceneFolder + "/terrain/basemap.txt";
    if (ReadZipTextFile(
        zip,
        basePath.c_str(),
        baseTxt))
    {
        ParseFloatTxt(
            baseTxt,
            resolution * resolution,
            outScene.terrain.baseMap);
    }

    // ==================================================
    // scene.json (rivers etc.)
    // ==================================================


    std::string sceneJsonText;
    std::string sceneJsonPath = sceneFolder + "/scene.json";
    if (ReadZipTextFile(zip, sceneJsonPath.c_str(), sceneJsonText))
    {
        json jscene = json::parse(sceneJsonText);

        if (jscene.contains("riverSplines"))
        {
            for (const auto& r : jscene["riverSplines"])
            {
                RiverSpline river;
                river.id = r["id"].get<std::string>();
                river.width = r["points"][0]["width"].get<float>();
                
                // 读取水深参数（默认0.5米）
                if (r.contains("waterDepth")) {
                    river.waterDepth = r["waterDepth"].get<float>();
                } else {
                    river.waterDepth = 0.5f;  // 默认值
                }

                for (const auto& p : r["points"])
                {
                    river.points.push_back(p["position"][0].get<float>());
                    river.points.push_back(p["position"][1].get<float>());
                    river.points.push_back(p["position"][2].get<float>());
                }

                outScene.rivers.push_back(std::move(river));
            }
        }
    }

    // ==================================================
    // grass
    // ==================================================


    std::string grassPath = sceneFolder + "/grass/grassmap.png";
    ReadZipFileToBuffer(
        zip,
        grassPath.c_str(),
        outScene.grass.grassMapPNG
    );

    zip_close(zip);
    return true;
}
