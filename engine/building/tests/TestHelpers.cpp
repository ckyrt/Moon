#include "TestHelpers.h"
#include "../../core/Assets/AssetPaths.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace Moon {
namespace Building {
namespace Test {

// ========================================
// Helper function to load JSON from file
// ========================================
std::string TestHelpers::LoadFromFile(const std::string& relativePath) {
  const std::string path = Moon::Assets::BuildBuildingPath(relativePath);
  std::ifstream file(path);
  if (file.good()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    std::string content = buffer.str();
    if (content.size() >= 3 &&
      (unsigned char)content[0] == 0xEF &&
      (unsigned char)content[1] == 0xBB &&
      (unsigned char)content[2] == 0xBF) {
      content = content.substr(3);
    }
    return content;
    }

  throw std::runtime_error("Failed to load test data: " + path);
}

// ========================================
// Basic Building Test Data
// ========================================

std::string TestHelpers::CreateMinimalValidBuilding() {
    return LoadFromFile("basic/minimal_building.json");
}

std::string TestHelpers::CreateSimpleRoom() {
    return LoadFromFile("basic/simple_room.json");
}

std::string TestHelpers::CreateMultiFloorBuilding() {
    return LoadFromFile("basic/multi_floor.json");
}

// ========================================
// Invalid JSON Test Data
// ========================================

std::string TestHelpers::CreateInvalidJSON_MissingGrid() {
    return R"({
  "schema": "moon_building_v8",
  "style": {
    "category": "modern",
    "facade": "glass",
    "roof": "flat",
    "window_style": "normal",
    "material": "concrete"
  },
  "masses": [],
  "floors": []
})";
}

std::string TestHelpers::CreateInvalidJSON_WrongGridSize() {
    return R"({
  "schema": "moon_building_v8",
  "grid": 0.3,
  "style": {
    "category": "modern",
    "facade": "glass",
    "roof": "flat",
    "window_style": "normal",
    "material": "concrete"
  },
  "masses": [],
  "floors": []
})";
}

std::string TestHelpers::CreateInvalidJSON_NonAlignedCoordinates() {
    return R"({
  "schema": "moon_building_v8",
  "grid": 0.5,
  "style": {
    "category": "modern",
    "facade": "glass",
    "roof": "flat",
    "window_style": "normal",
    "material": "concrete"
  },
  "masses": [
    {
      "mass_id": "main",
      "origin": [0.3, 0.7],
      "size": [10.0, 10.0],
      "floors": 1
    }
  ],
  "floors": []
})";
}

} // namespace Test
} // namespace Building
} // namespace Moon
