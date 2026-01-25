/*

my_scene (2).zip
└─ my_scene/
   ├─ scene.json
   │
   ├─ terrain/
   │  ├─ heightmap.txt
   │  ├─ heightmap.meta.json
   │  ├─ basemap.txt
   │  └─ basemap.meta.json
   │
   └─ grass/
      └─ grassmap.png

*/

#include "SceneZipLoader.h"

// ---- Suppress warnings for third-party C library (kuba--/zip) ----
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996) // strncpy, etc.
#pragma warning(disable: 6011) // possible null dereference
#pragma warning(disable: 6386) // buffer overrun (static analysis)
#pragma warning(disable: 6308) // realloc may return null
#pragma warning(disable: 6262) // large stack usage
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

using json = nlohmann::json;

// ======================================================
// zip 工具函数（kuba--/zip）
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
// txt heightmap 解析
// ======================================================

static bool ParseHeightmapTxt(
    const std::string& text,
    int expectedCount,
    std::vector<float>& outData)
{
    std::stringstream ss(text);
    std::string token;

    outData.clear();
    outData.reserve(expectedCount);

    while (std::getline(ss, token, ','))
    {
        if (token.empty())
            continue;

        outData.push_back(std::stof(token));
    }

    return (int)outData.size() == expectedCount;
}

// ======================================================
// 主入口
// ======================================================

bool SceneZipLoader::LoadSceneFromZip(
    const std::string& zipPath,
    SceneData& outScene)
{
    // kuba--/zip：open(mode='r')，没有 ZIP_RDONLY
    zip_t* zip = zip_open(zipPath.c_str(), 0, 'r');
    if (!zip)
        return false;

    // ---------------- scene.json ----------------
    std::string sceneJsonText;
    if (!ReadZipTextFile(zip, "my_scene/scene.json", sceneJsonText))
    {
        zip_close(zip);
        return false;
    }

    json jscene = json::parse(sceneJsonText);

    // ---------------- terrain ----------------
    if (jscene.contains("terrainData"))
    {
        const auto& t = jscene["terrainData"];

        int resolution = t["resolution"].get<int>();
        std::string heightFile = t["heightMapFile"].get<std::string>();

        std::string heightTxt;
        std::string heightPath = "my_scene/" + heightFile;

        if (!ReadZipTextFile(zip, heightPath.c_str(), heightTxt))
        {
            zip_close(zip);
            return false;
        }

        if (!ParseHeightmapTxt(
            heightTxt,
            resolution * resolution,
            outScene.terrain.heightMap))
        {
            zip_close(zip);
            return false;
        }

        outScene.terrain.resolution = resolution;
    }

    // ---------------- rivers (in scene.json) ----------------
    if (jscene.contains("riverSplines"))
    {
        for (const auto& r : jscene["riverSplines"])
        {
            RiverSpline river;
            river.id = r["id"].get<std::string>();

            for (const auto& p : r["points"])
            {
                river.points.push_back(p["position"][0].get<float>());
                river.points.push_back(p["position"][1].get<float>());
                river.points.push_back(p["position"][2].get<float>());
            }

            river.width = r["points"][0]["width"].get<float>();
            outScene.rivers.push_back(std::move(river));
        }
    }

    // ---------------- grass ----------------
    {
        // grassmap.png 是可选的
        ReadZipFileToBuffer(
            zip,
            "my_scene/grass/grassmap.png",
            outScene.grass.grassMapPNG
        );
    }

    zip_close(zip);
    return true;
}