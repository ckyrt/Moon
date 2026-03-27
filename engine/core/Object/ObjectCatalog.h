#pragma once

#include "ObjectTypes.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Moon {
namespace Object {

class ObjectCatalog {
public:
    bool LoadFromFile(const std::string& filepath, std::string& outError);

    void Clear();

    bool HasDefinition(const std::string& objectId) const;
    const ObjectDefinition* GetDefinition(const std::string& objectId) const;

    std::vector<const ObjectDefinition*> FindByCategory(const std::string& category) const;
    std::vector<const ObjectDefinition*> FindByTag(const std::string& tag) const;

    const std::unordered_map<std::string, ObjectDefinition>& GetDefinitions() const {
        return m_definitions;
    }

private:
    bool ParseDefinition(const void* jsonValue, ObjectDefinition& outDefinition, std::string& outError);
    bool ValidateDefinition(const ObjectDefinition& definition, std::string& outError) const;
    bool ResolveInheritance(std::string& outError);
    bool ResolveDefinitionInheritance(const std::string& objectId,
                                      std::unordered_map<std::string, int>& visitState,
                                      std::string& outError);

    std::unordered_map<std::string, ObjectDefinition> m_definitions;
};

} // namespace Object
} // namespace Moon
