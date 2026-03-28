#pragma once

#include "ObjectCatalog.h"
#include "Blueprint.h"
#include "../CSG/CSGBuilder.h"

namespace Moon {
namespace Object {

class ObjectFactory {
public:
    void SetBlueprintDatabase(BlueprintDatabase* database);
    void SetCatalog(const ObjectCatalog* catalog);

    GeneratedObject BuildObject(const ObjectBuildRequest& request, std::string& outError) const;

private:
    const ObjectVariant* FindVariant(const ObjectDefinition& definition, const std::string& variantId) const;
    uint32_t MakeSeed(const ObjectBuildRequest& request) const;
    void ApplyRandomizedDefaults(const ObjectDefinition& definition,
                                 const ObjectBuildRequest& request,
                                 std::unordered_map<std::string, float>& inOutParameters) const;

    BlueprintDatabase* m_blueprintDatabase = nullptr;
    const ObjectCatalog* m_catalog = nullptr;
};

} // namespace Object
} // namespace Moon
