#include "Blueprint.h"
#include "BlueprintLoader.h"
#include "../Logging/Logger.h"
#include "../../../external/nlohmann/json.hpp"
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace Moon {
namespace CSG {

// ============================================================================
// Blueprint 实现
// ============================================================================

Blueprint::Blueprint() {
}

Blueprint::~Blueprint() {
}

void Blueprint::AddParameter(const std::string& name, const ParameterDef& def) {
    m_parameters[name] = def;
}

bool Blueprint::HasParameter(const std::string& name) const {
    return m_parameters.find(name) != m_parameters.end();
}

const ParameterDef* Blueprint::GetParameter(const std::string& name) const {
    auto it = m_parameters.find(name);
    return (it != m_parameters.end()) ? &it->second : nullptr;
}

void Blueprint::SetRootNode(std::unique_ptr<Node> root) {
    m_rootNode = std::move(root);
}

bool Blueprint::Validate(std::string& outError) const {
    // 基础验证
    if (m_metadata.id.empty()) {
        outError = "Blueprint ID is empty";
        return false;
    }

    if (!m_rootNode) {
        outError = "Blueprint has no root node";
        return false;
    }

    // TODO: 递归验证 Node 树（参数引用是否存在、CSG 节点是否合法等）

    return true;
}

// ============================================================================
// BlueprintDatabase 实现
// ============================================================================

BlueprintDatabase::BlueprintDatabase() {
}

BlueprintDatabase::~BlueprintDatabase() {
}

bool BlueprintDatabase::LoadBlueprint(const std::string& filepath, std::string& outError) {
    auto blueprint = BlueprintLoader::LoadFromFile(filepath, outError);
    if (!blueprint) {
        return false;
    }

    std::string id = blueprint->GetId();
    
    // 检查重复 ID
    if (HasBlueprint(id)) {
        outError = "Blueprint ID already exists: " + id;
        MOON_LOG_ERROR("BlueprintDB", "%s", outError.c_str());
        return false;
    }

    m_idToPath[id] = filepath;
    m_blueprints[id] = std::move(blueprint);
    
    MOON_LOG_INFO("BlueprintDB", "Loaded blueprint: %s from %s", id.c_str(), filepath.c_str());
    m_orderedIds.push_back(id);
    return true;
}

bool BlueprintDatabase::LoadIndex(const std::string& indexPath, std::string& outError) {
    std::ifstream file(indexPath);
    if (!file.is_open()) {
        outError = "Failed to open index: " + indexPath;
        MOON_LOG_ERROR("BlueprintDB", "%s", outError.c_str());
        return false;
    }

    // 提取 index 所在目录，用于拼接相对路径
    std::string baseDir = indexPath;
    size_t lastSlash = baseDir.find_last_of("/\\");
    if (lastSlash != std::string::npos)
        baseDir = baseDir.substr(0, lastSlash + 1);
    else
        baseDir = "";

    json j;
    try {
        j = json::parse(file);
    } catch (const json::exception& e) {
        outError = std::string("JSON parse error in index: ") + e.what();
        MOON_LOG_ERROR("BlueprintDB", "%s", outError.c_str());
        return false;
    }

    if (!j.contains("items") || !j["items"].is_array()) {
        outError = "index.json missing 'items' array";
        MOON_LOG_ERROR("BlueprintDB", "%s", outError.c_str());
        return false;
    }

    int loaded = 0, failed = 0;
    for (const auto& item : j["items"]) {
        if (!item.contains("path")) continue;
        std::string relPath = item["path"].get<std::string>();
        std::string fullPath = baseDir + relPath;
        bool isDependency = item.contains("dependency") && item["dependency"].get<bool>();
        std::string itemError;
        if (LoadBlueprint(fullPath, itemError)) {
            // dependency 只加载进数据库，不加入 orderedIds（不出现在场景里）
            if (isDependency && !m_orderedIds.empty()) {
                m_orderedIds.pop_back();
            }
            loaded++;
        } else {
            MOON_LOG_WARN("BlueprintDB", "Skipping %s: %s", fullPath.c_str(), itemError.c_str());
            failed++;
        }
    }

    MOON_LOG_INFO("BlueprintDB", "LoadIndex done: %d loaded, %d failed", loaded, failed);
    return loaded > 0;
}

const Blueprint* BlueprintDatabase::GetBlueprint(const std::string& id) const {
    auto it = m_blueprints.find(id);
    return (it != m_blueprints.end()) ? it->second.get() : nullptr;
}

bool BlueprintDatabase::HasBlueprint(const std::string& id) const {
    return m_blueprints.find(id) != m_blueprints.end();
}

std::vector<std::string> BlueprintDatabase::GetAllIds() const {
    return m_orderedIds;  // 返回按加载顺序排列的 IDs
}

void BlueprintDatabase::Clear() {
    m_blueprints.clear();
    m_idToPath.clear();
    m_orderedIds.clear();
}

} // namespace CSG
} // namespace Moon
