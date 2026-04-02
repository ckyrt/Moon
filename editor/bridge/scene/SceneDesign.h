#pragma once

#include "../../../external/nlohmann/json.hpp"

#include <string>

namespace Moon {
namespace Editor {
namespace SceneDesign {

bool ParseSceneDesign(const std::string& sceneJson,
                      nlohmann::json& outScene,
                      std::string& outError);

bool ParseSceneEditOps(const std::string& opsJson,
                       nlohmann::json& outOps,
                       std::string& outError);

bool NormalizeSceneDesign(const std::string& sceneJson,
                          std::string& outSceneJson,
                          std::string& outError);

bool ApplySceneOperations(const std::string& sceneJson,
                          const std::string& opsJson,
                          std::string& outSceneJson,
                          std::string& outError);

} // namespace SceneDesign
} // namespace Editor
} // namespace Moon
