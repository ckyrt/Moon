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

void BlueprintDatabase::SetComponentsDirectory(const std::string& baseDir) {
    m_baseDir = baseDir;
    MOON_LOG_INFO("BlueprintDB", "Set components directory: %s", baseDir.c_str());
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

    // 读取所有组件索引信息，建立 id -> path 映射（用于懒加载）
    m_idToPath.clear();
    
    for (const auto& item : j["items"]) {
        if (!item.contains("id") || !item.contains("path")) continue;
        
        std::string id = item["id"].get<std::string>();
        std::string relPath = item["path"].get<std::string>();
        std::string fullPath = baseDir + relPath;
        
        // 注册路径映射（所有组件都注册，不区分 dependency）
        m_idToPath[id] = fullPath;
        
        std::string desc = item.contains("description") ? item["description"].get<std::string>() : "";
        MOON_LOG_INFO("BlueprintDB", "Registered: %s -> %s (%s)", 
            id.c_str(), fullPath.c_str(), desc.c_str());
    }

    MOON_LOG_INFO("BlueprintDB", "Index loaded: %d components registered", (int)m_idToPath.size());
    return m_idToPath.size() > 0;
}

const Blueprint* BlueprintDatabase::GetBlueprint(const std::string& id) {
    // 先查缓存
    auto it = m_blueprints.find(id);
    if (it != m_blueprints.end()) {
        return it->second.get();
    }
    
    // 缓存未命中，尝试懒加载
    return LoadBlueprintLazy(id);
}

const Blueprint* BlueprintDatabase::LoadBlueprintLazy(const std::string& id) {
    // 只从 index.json 注册的路径加载
    auto pathIt = m_idToPath.find(id);
    if (pathIt == m_idToPath.end()) {
        MOON_LOG_WARN("BlueprintDB", "Blueprint not found in index: %s", id.c_str());
        return nullptr;
    }
    
    std::string filepath = pathIt->second;
    std::string error;
    auto blueprint = BlueprintLoader::LoadFromFile(filepath, error);
    
    if (!blueprint) {
        MOON_LOG_ERROR("BlueprintDB", "Failed to lazy-load %s: %s", id.c_str(), error.c_str());
        return nullptr;
    }
    
    MOON_LOG_INFO("BlueprintDB", "Lazy-loaded blueprint: %s from %s", id.c_str(), filepath.c_str());
    
    const Blueprint* ptr = blueprint.get();
    m_blueprints[id] = std::move(blueprint);
    return ptr;
}

bool BlueprintDatabase::HasBlueprint(const std::string& id) const {
    return m_blueprints.find(id) != m_blueprints.end();
}

void BlueprintDatabase::Clear() {
    m_blueprints.clear();
    m_idToPath.clear();
    m_baseDir.clear();
}

} // namespace CSG
} // namespace Moon
