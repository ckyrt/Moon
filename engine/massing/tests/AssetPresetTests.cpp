#include <gtest/gtest.h>

#include "../MassMeshBuilder.h"
#include "../MassRuleParser.h"
#include "../../core/Assets/AssetPaths.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

namespace {

std::string ReadUtf8TextFile(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }
    return content;
}

std::vector<std::string> CollectJsonFiles(const std::string& root) {
    std::vector<std::string> files;
    WIN32_FIND_DATAA findData;
    HANDLE handle = FindFirstFileA((root + "\\*.json").c_str(), &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        return files;
    }

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            files.push_back(root + "\\" + findData.cFileName);
        }
    } while (FindNextFileA(handle, &findData));

    FindClose(handle);
    std::sort(files.begin(), files.end());
    return files;
}

} // namespace

TEST(MassingAssetPresetTests, AllMassingJsonFilesParseAndBuild) {
    const std::string root = Moon::Assets::BuildAssetPath("massing");
    const auto files = CollectJsonFiles(root);
    ASSERT_FALSE(files.empty()) << "No massing json files found under " << root;

    for (const auto& path : files) {
        SCOPED_TRACE(path);
        const std::string content = ReadUtf8TextFile(path);
        ASSERT_FALSE(content.empty()) << "Failed to read massing asset";

        Moon::Massing::RuleSet ruleSet;
        std::string parseError;
        ASSERT_TRUE(Moon::Massing::MassRuleParser::ParseFromString(content, ruleSet, parseError))
            << "Massing parse failed: " << parseError;

        Moon::Massing::MassBuildResult result;
        std::string buildError;
        EXPECT_TRUE(Moon::Massing::MassMeshBuilder::Build(ruleSet, result, buildError))
            << "Massing build failed: " << buildError;
        EXPECT_FALSE(result.items.empty()) << "Massing build produced no meshes";
    }
}
