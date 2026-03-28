#include "../Assets/AssetPaths.h"
#include "ObjectLibrary.h"

namespace Moon {
namespace Object {

bool ObjectLibrary::InitializeDefaults(std::string& outError) {
    return Initialize(Assets::BuildObjectPath("index.json"), outError);
}

bool ObjectLibrary::Initialize(const std::string& backendIndexPath, std::string& outError) {
    Clear();

    if (!m_blueprintDatabase.LoadIndex(backendIndexPath, outError)) {
        return false;
    }

    m_factory.SetBlueprintDatabase(&m_blueprintDatabase);
    return true;
}

void ObjectLibrary::Clear() {
    m_blueprintDatabase.Clear();
    m_factory.SetBlueprintDatabase(nullptr);
}

GeneratedObject ObjectLibrary::BuildObject(const ObjectBuildRequest& request, std::string& outError) const {
    return m_factory.BuildObject(request, outError);
}

} // namespace Object
} // namespace Moon
