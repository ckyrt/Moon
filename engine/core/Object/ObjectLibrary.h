#pragma once

#include "ObjectCatalog.h"
#include "ObjectFactory.h"

namespace Moon {
namespace Object {

class ObjectLibrary {
public:
    bool InitializeDefaults(std::string& outError);

    bool Initialize(const std::string& csgIndexPath,
                    const std::string& objectCatalogPath,
                    std::string& outError);

    void Clear();

    const ObjectCatalog& GetCatalog() const { return m_catalog; }
    const CSG::BlueprintDatabase& GetBlueprintDatabase() const { return m_blueprintDatabase; }

    GeneratedObject BuildObject(const ObjectBuildRequest& request, std::string& outError) const;

private:
    CSG::BlueprintDatabase m_blueprintDatabase;
    ObjectCatalog m_catalog;
    ObjectFactory m_factory;
};

} // namespace Object
} // namespace Moon
