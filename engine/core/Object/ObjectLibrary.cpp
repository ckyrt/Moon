#include "../Assets/AssetPaths.h"
#include "ObjectLibrary.h"

namespace Moon {
namespace Object {

bool ObjectLibrary::InitializeDefaults(std::string& outError) {
    return Initialize(
        Assets::BuildCsgPath("index.json"),
        Assets::BuildObjectPath("catalog.json"),
        outError);
}

bool ObjectLibrary::Initialize(const std::string& csgIndexPath,
                               const std::string& objectCatalogPath,
                               std::string& outError) {
    Clear();

    if (!m_blueprintDatabase.LoadIndex(csgIndexPath, outError)) {
        return false;
    }

    if (!m_catalog.LoadFromFile(objectCatalogPath, outError)) {
        Clear();
        return false;
    }

    m_factory.SetBlueprintDatabase(&m_blueprintDatabase);
    m_factory.SetCatalog(&m_catalog);
    return true;
}

void ObjectLibrary::Clear() {
    m_blueprintDatabase.Clear();
    m_catalog.Clear();
    m_factory.SetBlueprintDatabase(nullptr);
    m_factory.SetCatalog(nullptr);
}

GeneratedObject ObjectLibrary::BuildObject(const ObjectBuildRequest& request, std::string& outError) const {
    return m_factory.BuildObject(request, outError);
}

} // namespace Object
} // namespace Moon
