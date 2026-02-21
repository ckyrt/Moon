#include "Blueprint.h"
#include "BlueprintLoader.h"
#include "../Logging/Logger.h"

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
    return true;
}

bool BlueprintDatabase::LoadIndex(const std::string& indexPath, std::string& outError) {
    // TODO: 实现 index.json 解析
    // 这将在阶段2实现
    outError = "Index loading not implemented yet";
    MOON_LOG_WARN("BlueprintDB", "LoadIndex called but not implemented: %s", indexPath.c_str());
    return false;
}

const Blueprint* BlueprintDatabase::GetBlueprint(const std::string& id) const {
    auto it = m_blueprints.find(id);
    return (it != m_blueprints.end()) ? it->second.get() : nullptr;
}

bool BlueprintDatabase::HasBlueprint(const std::string& id) const {
    return m_blueprints.find(id) != m_blueprints.end();
}

std::vector<std::string> BlueprintDatabase::GetAllIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_blueprints.size());
    for (const auto& pair : m_blueprints) {
        ids.push_back(pair.first);
    }
    return ids;
}

void BlueprintDatabase::Clear() {
    m_blueprints.clear();
    m_idToPath.clear();
}

} // namespace CSG
} // namespace Moon
