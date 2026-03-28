#pragma once

#include "BlueprintTypes.h"
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Moon {
namespace Object {

class Blueprint {
public:
    Blueprint();
    ~Blueprint();

    const std::string& GetId() const { return m_metadata.id; }
    const std::string& GetCategory() const { return m_metadata.category; }
    const std::vector<std::string>& GetTags() const { return m_metadata.tags; }
    int GetSchemaVersion() const { return m_metadata.schemaVersion; }

    void SetId(const std::string& id) { m_metadata.id = id; }
    void SetCategory(const std::string& category) { m_metadata.category = category; }
    void SetTags(const std::vector<std::string>& tags) { m_metadata.tags = tags; }
    void SetSchemaVersion(int version) { m_metadata.schemaVersion = version; }

    void AddParameter(const std::string& name, const ParameterDef& def);
    bool HasParameter(const std::string& name) const;
    const ParameterDef* GetParameter(const std::string& name) const;
    const std::unordered_map<std::string, ParameterDef>& GetParameters() const { return m_parameters; }

    using AnchorExpr = std::array<std::string, 3>;
    void AddAnchor(const std::string& name, const AnchorExpr& expr) { m_anchors[name] = expr; }
    bool HasAnchor(const std::string& name) const { return m_anchors.count(name) > 0; }
    const std::unordered_map<std::string, AnchorExpr>& GetAnchors() const { return m_anchors; }

    void SetRootNode(std::unique_ptr<Node> root);
    const Node* GetRootNode() const { return m_rootNode.get(); }
    Node* GetRootNode() { return m_rootNode.get(); }

    bool Validate(std::string& outError) const;

private:
    BlueprintMetadata m_metadata;
    std::unordered_map<std::string, ParameterDef> m_parameters;
    std::unordered_map<std::string, AnchorExpr> m_anchors;
    std::unique_ptr<Node> m_rootNode;
};

class BlueprintDatabase {
public:
    struct EntryInfo {
        std::string id;
        std::string path;
        std::string description;
        std::string category;
        std::vector<std::string> tags;
    };

    BlueprintDatabase();
    ~BlueprintDatabase();

    bool LoadBlueprint(const std::string& filepath, std::string& outError);
    void SetComponentsDirectory(const std::string& baseDir);
    bool LoadIndex(const std::string& indexPath, std::string& outError);

    const Blueprint* GetBlueprint(const std::string& id);
    const EntryInfo* GetEntryInfo(const std::string& id) const;
    std::vector<EntryInfo> FindEntriesByCategory(const std::string& category) const;
    std::vector<EntryInfo> FindEntriesByTag(const std::string& tag) const;
    bool HasBlueprint(const std::string& id) const;
    void Clear();

private:
    const Blueprint* LoadBlueprintLazy(const std::string& id);

    std::unordered_map<std::string, std::unique_ptr<Blueprint>> m_blueprints;
    std::unordered_map<std::string, std::string> m_idToPath;
    std::unordered_map<std::string, EntryInfo> m_entries;
    std::string m_baseDir;
};

} // namespace Object
} // namespace Moon
