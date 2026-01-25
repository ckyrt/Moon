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

    // ==================================================
    // terrain meta
    // ==================================================

    std::string heightMetaText;
    if (!ReadZipTextFile(
        zip,
        "my_scene/terrain/heightmap.meta.json",
        heightMetaText))
    {
        zip_close(zip);
        return false;
    }

    json heightMeta = json::parse(heightMetaText);

    if (!heightMeta.contains("width") || !heightMeta.contains("height")) {
        MOON_LOG_ERROR("SceneZipLoader", "heightmap.meta.json missing width/height");
        return false;
    }

    int width = heightMeta["width"].get<int>();
    int height = heightMeta["height"].get<int>();

    if (width != height) {
        MOON_LOG_WARN("SceneZipLoader", "heightmap not square: %d x %d", width, height);
    }

    int resolution = width;

    outScene.terrain.resolution = resolution;

    // ==================================================
    // heightmap.txt
    // ==================================================

    std::string heightTxt;
    if (!ReadZipTextFile(
        zip,
        "my_scene/terrain/heightmap.txt",
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
    if (ReadZipTextFile(
        zip,
        "my_scene/terrain/basemap.txt",
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
    if (ReadZipTextFile(zip, "my_scene/scene.json", sceneJsonText))
    {
        json jscene = json::parse(sceneJsonText);

        if (jscene.contains("riverSplines"))
        {
            for (const auto& r : jscene["riverSplines"])
            {
                RiverSpline river;
                river.id = r["id"].get<std::string>();
                river.width = r["points"][0]["width"].get<float>();

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

    ReadZipFileToBuffer(
        zip,
        "my_scene/grass/grassmap.png",
        outScene.grass.grassMapPNG
    );

    zip_close(zip);
    return true;
}
