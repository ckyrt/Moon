#pragma once

#include "Blueprint.h"
#include "ObjectTypes.h"
#include "../CSG/CSGBuilder.h"

namespace Moon {
namespace Object {

class ObjectFactory {
public:
    void SetBlueprintDatabase(BlueprintDatabase* database);
    GeneratedObject BuildObject(const ObjectBuildRequest& request, std::string& outError) const;

private:
    BlueprintDatabase* m_blueprintDatabase = nullptr;
};

} // namespace Object
} // namespace Moon
