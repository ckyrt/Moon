#pragma once

#include "ObjectFactory.h"
#include "ObjectTypes.h"

namespace Moon {
namespace Object {

class ObjectLibrary {
public:
    bool InitializeDefaults(std::string& outError);
    bool Initialize(const std::string& backendIndexPath, std::string& outError);

    void Clear();

    const BlueprintDatabase& GetBlueprintDatabase() const { return m_blueprintDatabase; }
    GeneratedObject BuildObject(const ObjectBuildRequest& request, std::string& outError) const;

private:
    BlueprintDatabase m_blueprintDatabase;
    ObjectFactory m_factory;
};

} // namespace Object
} // namespace Moon
