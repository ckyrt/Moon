#include "TestHelpers.h"

namespace Moon {
namespace Building {
namespace Test {

std::string TestHelpers::LoadFromFile(const std::string&) {
  return std::string();
}

// ========================================
// Basic Building Test Data
// ========================================

std::string TestHelpers::CreateMinimalValidBuilding() {
    return R"({
  "schema": "moon_building",
  "grid": 0.5,
  "building_type": "villa",
  "style": {
    "category": "modern",
    "facade": "glass",
    "roof": "flat",
    "window_style": "large",
    "material": "concrete"
  },
  "mass": {
    "footprint_area": 80,
    "floors": 1,
    "total_height": 3
  },
  "program": {
    "floors": [
      {
        "level": 0,
        "name": "ground_floor",
        "spaces": [
          {
            "space_id": "living_1",
            "type": "living",
            "zone": "public",
            "area_preferred": 63,
            "constraints": {
              "natural_light": "required",
              "ceiling_height": 3.0,
              "min_width": 4.0
            }
          }
        ]
      }
    ]
  }
})";
}

std::string TestHelpers::CreateSimpleRoom() {
    return R"({
  "schema": "moon_building",
  "grid": 0.5,
  "building_type": "villa",
  "style": {
    "category": "minimal",
    "facade": "white",
    "roof": "flat",
    "window_style": "standard",
    "material": "plaster"
  },
  "mass": {
    "footprint_area": 20,
    "floors": 1,
    "total_height": 3
  },
  "program": {
    "floors": [
      {
        "level": 0,
        "name": "room_floor",
        "spaces": [
          {
            "space_id": "living_1",
            "type": "living",
            "zone": "public",
            "area_preferred": 12,
            "constraints": {
              "natural_light": "required",
              "ceiling_height": 3.0,
              "min_width": 3.0
            }
          }
        ]
      }
    ]
  }
})";
}

std::string TestHelpers::CreateMultiFloorBuilding() {
    return R"({
  "schema": "moon_building",
  "grid": 0.5,
  "building_type": "villa",
  "style": {
    "category": "modern",
    "facade": "brick",
    "roof": "flat",
    "window_style": "medium",
    "material": "brick_red"
  },
  "mass": {
    "footprint_area": 120,
    "floors": 2,
    "total_height": 6.5
  },
  "program": {
    "floors": [
      {
        "level": 0,
        "name": "ground_floor",
        "spaces": [
          {
            "space_id": "core",
            "type": "core",
            "zone": "service",
            "area_preferred": 80,
            "constraints": {
              "connects_to_floor": 1,
              "ceiling_height": 3.5,
              "min_width": 4.0
            }
          }
        ]
      },
      {
        "level": 1,
        "name": "upper_floor",
        "spaces": [
          {
            "space_id": "bedroom_1",
            "type": "bedroom",
            "zone": "private",
            "area_preferred": 80,
            "constraints": {
              "natural_light": "required",
              "ceiling_height": 3.0,
              "min_width": 4.0
            }
          }
        ]
      }
    ]
  }
})";
}

// ========================================
// Invalid JSON Test Data
// ========================================

std::string TestHelpers::CreateInvalidJSON_MissingGrid() {
    return R"({
  "schema": "moon_building",
  "style": {
    "category": "modern",
    "facade": "glass",
    "roof": "flat",
    "window_style": "normal",
    "material": "concrete"
  },
  "mass": {
    "footprint_area": 80,
    "floors": 1
  },
  "program": {
    "floors": []
  }
})";
}

std::string TestHelpers::CreateInvalidJSON_WrongGridSize() {
    return R"({
  "schema": "moon_building",
  "grid": 0.3,
  "style": {
    "category": "modern",
    "facade": "glass",
    "roof": "flat",
    "window_style": "normal",
    "material": "concrete"
  },
  "mass": {
    "footprint_area": 80,
    "floors": 1
  },
  "program": {
    "floors": []
  }
})";
}

std::string TestHelpers::CreateInvalidJSON_NonAlignedCoordinates() {
    return R"({
  "schema": "moon_building",
  "grid": 0.5,
  "style": {
    "category": "modern",
    "facade": "glass",
    "roof": "flat",
    "window_style": "normal",
    "material": "concrete"
  },
  "mass": {
    "footprint_area": -10,
    "floors": 1
  },
  "program": {
    "floors": [
      {
        "level": 0,
        "spaces": [
          {
            "space_id": "bad_room",
            "type": "living",
            "constraints": {
              "min_width": -1.0
            }
          }
        ]
      }
    ]
  }
})";
}

} // namespace Test
} // namespace Building
} // namespace Moon
