#include "Blueprint.h"
#include "BlueprintLoader.h"
#include "../Logging/Logger.h"
#include "../../../external/nlohmann/json.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace Moon {
namespace Object {

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
    if (m_metadata.id.empty()) {
        outError = "Blueprint ID is empty";
        return false;
    }

    if (!m_rootNode) {
        outError = "Blueprint has no root node";
        return false;
    }

    return true;
}

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

    const std::string id = blueprint->GetId();
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

    std::string baseDir = indexPath;
    const size_t lastSlash = baseDir.find_last_of("/\\");
    baseDir = (lastSlash != std::string::npos) ? baseDir.substr(0, lastSlash + 1) : "";

    json root;
    try {
        root = json::parse(file);
    } catch (const json::exception& e) {
        outError = std::string("JSON parse error in index: ") + e.what();
        MOON_LOG_ERROR("BlueprintDB", "%s", outError.c_str());
        return false;
    }

    if (!root.contains("items") || !root["items"].is_array()) {
        outError = "index.json missing 'items' array";
        MOON_LOG_ERROR("BlueprintDB", "%s", outError.c_str());
        return false;
    }

    m_idToPath.clear();
    m_entries.clear();

    for (const auto& item : root["items"]) {
        if (!item.contains("id") || !item.contains("path") ||
            !item["id"].is_string() || !item["path"].is_string()) {
            continue;
        }

        EntryInfo entry;
        entry.id = item["id"].get<std::string>();
        entry.path = baseDir + item["path"].get<std::string>();
        if (item.contains("description") && item["description"].is_string()) {
            entry.description = item["description"].get<std::string>();
        }
        if (item.contains("category") && item["category"].is_string()) {
            entry.category = item["category"].get<std::string>();
        }
        if (item.contains("tags") && item["tags"].is_array()) {
            for (const auto& tag : item["tags"]) {
                if (tag.is_string()) {
                    entry.tags.push_back(tag.get<std::string>());
                }
            }
        }

        m_idToPath[entry.id] = entry.path;
        m_entries[entry.id] = entry;

        MOON_LOG_INFO("BlueprintDB", "Registered: %s -> %s (%s)",
            entry.id.c_str(), entry.path.c_str(), entry.description.c_str());
    }

    MOON_LOG_INFO("BlueprintDB", "Index loaded: %d assets registered", static_cast<int>(m_idToPath.size()));
    return !m_idToPath.empty();
}

const Blueprint* BlueprintDatabase::GetBlueprint(const std::string& id) {
    auto it = m_blueprints.find(id);
    if (it != m_blueprints.end()) {
        return it->second.get();
    }

    return LoadBlueprintLazy(id);
}

const BlueprintDatabase::EntryInfo* BlueprintDatabase::GetEntryInfo(const std::string& id) const {
    auto it = m_entries.find(id);
    return (it != m_entries.end()) ? &it->second : nullptr;
}

std::vector<BlueprintDatabase::EntryInfo> BlueprintDatabase::FindEntriesByCategory(const std::string& category) const {
    std::vector<EntryInfo> result;
    for (const auto& entry : m_entries) {
        if (entry.second.category == category) {
            result.push_back(entry.second);
        }
    }
    return result;
}

std::vector<BlueprintDatabase::EntryInfo> BlueprintDatabase::FindEntriesByTag(const std::string& tag) const {
    std::vector<EntryInfo> result;
    for (const auto& entry : m_entries) {
        if (std::find(entry.second.tags.begin(), entry.second.tags.end(), tag) != entry.second.tags.end()) {
            result.push_back(entry.second);
        }
    }
    return result;
}

const Blueprint* BlueprintDatabase::LoadBlueprintLazy(const std::string& id) {
    auto pathIt = m_idToPath.find(id);
    if (pathIt == m_idToPath.end()) {
        MOON_LOG_WARN("BlueprintDB", "Blueprint not found in index: %s", id.c_str());
        return nullptr;
    }

    std::string error;
    auto blueprint = BlueprintLoader::LoadFromFile(pathIt->second, error);
    if (!blueprint) {
        MOON_LOG_ERROR("BlueprintDB", "Failed to lazy-load %s: %s", id.c_str(), error.c_str());
        return nullptr;
    }

    const Blueprint* ptr = blueprint.get();
    m_blueprints[id] = std::move(blueprint);
    MOON_LOG_INFO("BlueprintDB", "Lazy-loaded blueprint: %s from %s", id.c_str(), pathIt->second.c_str());
    return ptr;
}

bool BlueprintDatabase::HasBlueprint(const std::string& id) const {
    return m_blueprints.find(id) != m_blueprints.end();
}

void BlueprintDatabase::Clear() {
    m_blueprints.clear();
    m_idToPath.clear();
    m_entries.clear();
    m_baseDir.clear();
}

} // namespace Object
} // namespace Moon
