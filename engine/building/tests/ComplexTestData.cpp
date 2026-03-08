#include "TestHelpers.h"

namespace Moon {
namespace Building {
namespace Test {

// ========================================
// Complex Building Scenarios
// ========================================

std::string TestHelpers::CreateVilla() {
        return R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "villa",
    "style": {
        "category": "modern",
        "facade": "glass_white",
        "roof": "flat",
        "window_style": "full_height",
        "material": "concrete_white"
    },
    "mass": {
        "footprint_area": 216,
        "floors": 2,
        "total_height": 6.0
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "ground_floor",
                "spaces": [
                    {
                        "space_id": "living_room",
                        "type": "living",
                        "zone": "public",
                        "area_preferred": 38,
                        "adjacency": [
                            {"to": "kitchen", "relationship": "connected", "importance": "required"},
                            {"to": "hall", "relationship": "nearby", "importance": "preferred"}
                        ],
                        "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 5.0}
                    },
                    {
                        "space_id": "kitchen",
                        "type": "kitchen",
                        "zone": "public",
                        "area_preferred": 24,
                        "adjacency": [
                            {"to": "living_room", "relationship": "connected", "importance": "required"},
                            {"to": "bathroom_gf", "relationship": "nearby", "importance": "preferred"}
                        ],
                        "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}
                    },
                    {
                        "space_id": "hall",
                        "type": "corridor",
                        "zone": "circulation",
                        "area_preferred": 32,
                        "adjacency": [
                            {"to": "core", "relationship": "connected", "importance": "required"},
                            {"to": "living_room", "relationship": "connected", "importance": "preferred"}
                        ],
                        "constraints": {"ceiling_height": 3.0, "min_width": 3.0}
                    },
                    {
                        "space_id": "core",
                        "type": "core",
                        "zone": "service",
                        "area_preferred": 26,
                        "constraints": {"connects_to_floor": 1, "ceiling_height": 3.0, "min_width": 3.0}
                    },
                    {
                        "space_id": "bathroom_gf",
                        "type": "bathroom",
                        "zone": "service",
                        "area_preferred": 8,
                        "adjacency": [
                            {"to": "hall", "relationship": "connected", "importance": "required"}
                        ],
                        "constraints": {"ceiling_height": 3.0, "min_width": 2.0}
                    },
                    {
                        "space_id": "garage",
                        "type": "garage",
                        "zone": "service",
                        "area_preferred": 36,
                        "constraints": {"exterior_access": true, "ceiling_height": 3.0, "min_width": 5.5}
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
                        "area_preferred": 26,
                        "adjacency": [
                            {"to": "bedroom_2", "relationship": "nearby", "importance": "preferred"}
                        ],
                        "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}
                    },
                    {
                        "space_id": "bedroom_2",
                        "type": "bedroom",
                        "zone": "private",
                        "area_preferred": 24,
                        "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}
                    },
                    {
                        "space_id": "bathroom_up",
                        "type": "bathroom",
                        "zone": "service",
                        "area_preferred": 10,
                        "constraints": {"ceiling_height": 3.0, "min_width": 2.0}
                    },
                    {
                        "space_id": "landing",
                        "type": "corridor",
                        "zone": "circulation",
                        "area_preferred": 20,
                        "constraints": {"ceiling_height": 3.0, "min_width": 2.5}
                    }
                ]
            }
        ]
    }
})";
}

std::string TestHelpers::CreateApartmentBuilding() {
        return R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "apartment",
    "style": {
        "category": "modern",
        "facade": "concrete",
        "roof": "flat",
        "window_style": "standard",
        "material": "concrete_white"
    },
    "mass": {
        "footprint_area": 300,
        "floors": 3,
        "total_height": 10.0
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "lobby_floor",
                "spaces": [
                    {"space_id": "core", "type": "core", "zone": "service", "area_preferred": 90, "constraints": {"connects_to_floor": 1, "ceiling_height": 3.5, "min_width": 4.5}},
                    {"space_id": "lobby", "type": "lobby", "zone": "circulation", "area_preferred": 48, "constraints": {"natural_light": "preferred", "ceiling_height": 4.0, "min_width": 5.0}},
                    {"space_id": "retail_a", "unit_id": "retail_a", "type": "shop", "zone": "public", "area_preferred": 42, "constraints": {"natural_light": "required", "ceiling_height": 3.5, "min_width": 5.0}},
                    {"space_id": "retail_b", "unit_id": "retail_b", "type": "shop", "zone": "public", "area_preferred": 42, "constraints": {"natural_light": "required", "ceiling_height": 3.5, "min_width": 5.0}}
                ]
            },
            {
                "level": 1,
                "name": "typical_floor_1",
                "spaces": [
                    {"space_id": "core_1", "type": "core", "zone": "service", "area_preferred": 70, "constraints": {"connects_from_floor": 0, "connects_to_floor": 2, "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "corridor_1", "type": "corridor", "zone": "circulation", "area_preferred": 42, "constraints": {"ceiling_height": 3.0, "min_width": 2.5}},
                    {"space_id": "living_a_1", "unit_id": "apt_a_1", "type": "living", "zone": "public", "area_preferred": 30, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "bedroom_a_1", "unit_id": "apt_a_1", "type": "bedroom", "zone": "private", "area_preferred": 16, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}},
                    {"space_id": "bathroom_a_1", "unit_id": "apt_a_1", "type": "bathroom", "zone": "service", "area_preferred": 6, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}},
                    {"space_id": "living_b_1", "unit_id": "apt_b_1", "type": "living", "zone": "public", "area_preferred": 30, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "bedroom_b_1", "unit_id": "apt_b_1", "type": "bedroom", "zone": "private", "area_preferred": 16, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}},
                    {"space_id": "bathroom_b_1", "unit_id": "apt_b_1", "type": "bathroom", "zone": "service", "area_preferred": 6, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}}
                ]
            },
            {
                "level": 2,
                "name": "typical_floor_2",
                "spaces": [
                      {"space_id": "core_2", "type": "core", "zone": "service", "area_preferred": 70, "constraints": {"connects_from_floor": 1, "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "corridor_2", "type": "corridor", "zone": "circulation", "area_preferred": 42, "constraints": {"ceiling_height": 3.0, "min_width": 2.5}},
                    {"space_id": "living_a_2", "unit_id": "apt_a_2", "type": "living", "zone": "public", "area_preferred": 30, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "bedroom_a_2", "unit_id": "apt_a_2", "type": "bedroom", "zone": "private", "area_preferred": 16, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}},
                    {"space_id": "bathroom_a_2", "unit_id": "apt_a_2", "type": "bathroom", "zone": "service", "area_preferred": 6, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}},
                    {"space_id": "living_b_2", "unit_id": "apt_b_2", "type": "living", "zone": "public", "area_preferred": 30, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "bedroom_b_2", "unit_id": "apt_b_2", "type": "bedroom", "zone": "private", "area_preferred": 16, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}},
                    {"space_id": "bathroom_b_2", "unit_id": "apt_b_2", "type": "bathroom", "zone": "service", "area_preferred": 6, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}}
                ]
            }
        ]
    }
})";
}

std::string TestHelpers::CreateLShapedBuilding() {
        return R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "villa",
    "style": {
        "category": "modern",
        "facade": "glass_white",
        "roof": "flat",
        "window_style": "full_height",
        "material": "concrete_white"
    },
    "mass": {
        "footprint_area": 176,
        "floors": 1,
        "total_height": 3.0
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "l_shape_floor",
                "spaces": [
                    {"space_id": "living_wing", "type": "living", "zone": "public", "area_preferred": 77, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 6.0}},
                    {"space_id": "bedroom_wing", "type": "bedroom", "zone": "private", "area_preferred": 63, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 5.0}},
                    {"space_id": "joint_corridor", "type": "corridor", "zone": "circulation", "area_preferred": 18, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}}
                ]
            }
        ]
    }
})";
}

std::string TestHelpers::CreateLuxuryVilla() {
        return R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "villa",
    "style": {
        "category": "luxury",
        "facade": "stone_white",
        "roof": "pitched",
        "window_style": "french",
        "material": "brick_white"
    },
    "mass": {
        "footprint_area": 216,
        "floors": 3,
        "total_height": 9.5
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "ground_floor",
                "spaces": [
                    {"space_id": "entrance_hall", "type": "entrance", "zone": "circulation", "area_preferred": 18, "constraints": {"natural_light": "preferred", "ceiling_height": 4.0, "min_width": 3.0}},
                    {"space_id": "core_gf", "type": "core", "zone": "service", "area_preferred": 20, "constraints": {"connects_to_floor": 1, "ceiling_height": 3.5, "min_width": 3.0}},
                    {"space_id": "living_room", "type": "living", "zone": "public", "area_preferred": 42, "constraints": {"natural_light": "required", "ceiling_height": 4.0, "min_width": 5.0}},
                    {"space_id": "dining_room", "type": "dining", "zone": "public", "area_preferred": 20, "constraints": {"natural_light": "required", "ceiling_height": 3.5, "min_width": 4.0}},
                    {"space_id": "kitchen", "type": "kitchen", "zone": "service", "area_preferred": 18, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}},
                    {"space_id": "guest_bedroom", "type": "bedroom", "zone": "private", "area_preferred": 22, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "guest_bathroom", "type": "bathroom", "zone": "service", "area_preferred": 8, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}},
                    {"space_id": "storage", "type": "storage", "zone": "service", "area_preferred": 9, "constraints": {"ceiling_height": 2.5, "min_width": 2.0}},
                      {"space_id": "powder_room", "type": "bathroom", "zone": "service", "area_preferred": 8, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}},
                    {"space_id": "laundry", "type": "laundry", "zone": "service", "area_preferred": 12, "constraints": {"ceiling_height": 2.5, "min_width": 2.5}}
                ]
            },
            {
                "level": 1,
                "name": "bedroom_floor",
                "spaces": [
                    {"space_id": "core_l1", "type": "core", "zone": "service", "area_preferred": 18, "constraints": {"connects_from_floor": 0, "connects_to_floor": 2, "ceiling_height": 3.0, "min_width": 3.0}},
                    {"space_id": "master_bedroom", "type": "bedroom", "zone": "private", "area_preferred": 33, "constraints": {"natural_light": "required", "ceiling_height": 3.2, "min_width": 4.5}},
                    {"space_id": "master_bathroom", "type": "bathroom", "zone": "service", "area_preferred": 12, "constraints": {"ceiling_height": 3.0, "min_width": 2.5}},
                    {"space_id": "walk_in_closet", "type": "closet", "zone": "service", "area_preferred": 8, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}},
                    {"space_id": "bedroom_2", "type": "bedroom", "zone": "private", "area_preferred": 18, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}},
                    {"space_id": "bedroom_3", "type": "bedroom", "zone": "private", "area_preferred": 18, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}},
                    {"space_id": "hallway", "type": "corridor", "zone": "circulation", "area_preferred": 24, "constraints": {"ceiling_height": 3.0, "min_width": 2.5}},
                    {"space_id": "shared_bathroom", "type": "bathroom", "zone": "service", "area_preferred": 10, "constraints": {"ceiling_height": 3.0, "min_width": 2.5}},
                    {"space_id": "study", "type": "office", "zone": "private", "area_preferred": 18, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}}
                ]
            },
            {
                "level": 2,
                "name": "amenity_floor",
                "spaces": [
                    {"space_id": "core_l2", "type": "core", "zone": "service", "area_preferred": 18, "constraints": {"connects_from_floor": 1, "ceiling_height": 3.0, "min_width": 3.0}},
                    {"space_id": "entertainment_room", "type": "living", "zone": "public", "area_preferred": 52, "constraints": {"natural_light": "required", "ceiling_height": 3.5, "min_width": 5.0}},
                    {"space_id": "gym", "type": "office", "zone": "private", "area_preferred": 24, "constraints": {"natural_light": "required", "ceiling_height": 3.5, "min_width": 4.0}},
                    {"space_id": "terrace", "type": "terrace", "zone": "public", "area_preferred": 28, "constraints": {"exterior_access": true, "ceiling_height": 0.0, "min_width": 4.0}},
                    {"space_id": "utility_room", "type": "storage", "zone": "service", "area_preferred": 12, "constraints": {"ceiling_height": 2.5, "min_width": 2.5}}
                ]
            }
        ]
    }
})";
}

std::string TestHelpers::CreateOfficeTower() {
        return R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "office_tower",
    "style": {
        "category": "contemporary",
        "facade": "glass_curtain",
        "roof": "flat",
        "window_style": "full_height",
        "material": "steel_glass"
    },
    "mass": {
        "footprint_area": 420,
        "floors": 4,
        "total_height": 16.0
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "ground_lobby",
                "spaces": [
                    {"space_id": "tower_core_gf", "type": "core", "zone": "service", "area_preferred": 78, "constraints": {"connects_to_floor": 1, "ceiling_height": 4.5, "min_width": 4.5}},
                    {"space_id": "main_lobby", "type": "lobby", "zone": "circulation", "area_preferred": 96, "constraints": {"natural_light": "preferred", "ceiling_height": 5.0, "min_width": 6.0}},
                    {"space_id": "podium_cafe", "unit_id": "podium_cafe", "type": "shop", "zone": "public", "area_preferred": 34, "constraints": {"natural_light": "required", "ceiling_height": 4.5, "min_width": 4.0}},
                    {"space_id": "reception", "type": "office", "zone": "public", "area_preferred": 42, "constraints": {"natural_light": "preferred", "ceiling_height": 4.0, "min_width": 4.0}},
                    {"space_id": "meeting_hub", "type": "meeting_room", "zone": "public", "area_preferred": 30, "constraints": {"natural_light": "preferred", "ceiling_height": 4.0, "min_width": 4.0}},
                    {"space_id": "gf_bathroom", "type": "bathroom", "zone": "service", "area_preferred": 12, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}}
                ]
            },
            {
                "level": 1,
                "name": "podium_shared_floor",
                "spaces": [
                    {"space_id": "tower_core_l1", "type": "core", "zone": "service", "area_preferred": 72, "constraints": {"connects_from_floor": 0, "connects_to_floor": 2, "ceiling_height": 3.8, "min_width": 4.0}},
                    {"space_id": "co_work_l1", "type": "office", "zone": "public", "area_preferred": 86, "constraints": {"natural_light": "required", "ceiling_height": 3.8, "min_width": 5.5}},
                    {"space_id": "training_l1", "type": "meeting_room", "zone": "public", "area_preferred": 40, "constraints": {"natural_light": "preferred", "ceiling_height": 3.8, "min_width": 4.5}},
                    {"space_id": "support_l1", "type": "mechanical", "zone": "service", "area_preferred": 18, "constraints": {"ceiling_height": 3.2, "min_width": 2.5}},
                    {"space_id": "bathroom_l1", "type": "bathroom", "zone": "service", "area_preferred": 14, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}}
                ]
            },
            {
                "level": 2,
                "name": "office_floor_2",
                "spaces": [
                    {"space_id": "tower_core_l2", "type": "core", "zone": "service", "area_preferred": 72, "constraints": {"connects_from_floor": 1, "connects_to_floor": 3, "ceiling_height": 3.8, "min_width": 4.0}},
                    {"space_id": "open_office_l2", "type": "office", "zone": "public", "area_preferred": 126, "constraints": {"natural_light": "required", "ceiling_height": 3.8, "min_width": 6.0}},
                    {"space_id": "meeting_l2", "type": "meeting_room", "zone": "public", "area_preferred": 32, "constraints": {"natural_light": "preferred", "ceiling_height": 3.8, "min_width": 4.0}},
                    {"space_id": "bathroom_l2", "type": "bathroom", "zone": "service", "area_preferred": 14, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}}
                ]
            },
            {
                "level": 3,
                "name": "executive_floor",
                "spaces": [
                    {"space_id": "tower_core_l3", "type": "core", "zone": "service", "area_preferred": 72, "constraints": {"connects_from_floor": 2, "ceiling_height": 3.8, "min_width": 4.0}},
                    {"space_id": "executive_office", "type": "office", "zone": "public", "area_preferred": 88, "constraints": {"natural_light": "required", "ceiling_height": 4.0, "min_width": 5.0}},
                    {"space_id": "board_room", "type": "meeting_room", "zone": "public", "area_preferred": 42, "constraints": {"natural_light": "preferred", "ceiling_height": 4.0, "min_width": 5.0}},
                    {"space_id": "mechanical_l3", "type": "mechanical", "zone": "service", "area_preferred": 22, "constraints": {"ceiling_height": 3.2, "min_width": 3.0}},
                    {"space_id": "bathroom_l3", "type": "bathroom", "zone": "service", "area_preferred": 14, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}}
                ]
            }
        ]
    }
})";
}

std::string TestHelpers::CreateShoppingMall() {
        return R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "mall",
    "style": {
        "category": "commercial",
        "facade": "glass_stone",
        "roof": "flat",
        "window_style": "full_height",
        "material": "stone_glass"
    },
    "mass": {
        "footprint_area": 900,
        "floors": 2,
        "total_height": 10.0
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "main_mall_floor",
                "spaces": [
                    {"space_id": "mall_core_gf", "type": "core", "zone": "service", "area_preferred": 70, "constraints": {"connects_to_floor": 1, "ceiling_height": 4.8, "min_width": 4.5}},
                    {"space_id": "main_atrium", "type": "void", "zone": "public", "area_preferred": 180, "constraints": {"ceiling_height": 10.0, "min_width": 10.0}},
                    {"space_id": "main_concourse", "type": "corridor", "zone": "circulation", "area_preferred": 150, "adjacency": [{"to": "main_atrium", "relationship": "around", "importance": "required"}], "constraints": {"ceiling_height": 4.8, "min_width": 5.0}},
                    {"space_id": "mall_lobby", "type": "lobby", "zone": "circulation", "area_preferred": 84, "constraints": {"natural_light": "preferred", "ceiling_height": 5.0, "min_width": 6.0}},
                    {"space_id": "shop_a_gf", "unit_id": "shop_a", "type": "shop", "zone": "public", "area_preferred": 68, "adjacency": [{"to": "main_concourse", "relationship": "connected", "importance": "required"}], "constraints": {"ceiling_height": 4.8, "min_width": 5.0}},
                    {"space_id": "shop_b_gf", "unit_id": "shop_b", "type": "shop", "zone": "public", "area_preferred": 64, "adjacency": [{"to": "main_concourse", "relationship": "connected", "importance": "required"}], "constraints": {"ceiling_height": 4.8, "min_width": 5.0}},
                    {"space_id": "shop_c_gf", "unit_id": "shop_c", "type": "shop", "zone": "public", "area_preferred": 72, "adjacency": [{"to": "main_concourse", "relationship": "connected", "importance": "required"}], "constraints": {"ceiling_height": 4.8, "min_width": 5.0}},
                    {"space_id": "mall_bathroom_gf", "type": "bathroom", "zone": "service", "area_preferred": 18, "constraints": {"ceiling_height": 3.5, "min_width": 2.5}}
                ]
            },
            {
                "level": 1,
                "name": "upper_mall_floor",
                "spaces": [
                    {"space_id": "mall_core_l1", "type": "core", "zone": "service", "area_preferred": 68, "constraints": {"connects_from_floor": 0, "ceiling_height": 4.5, "min_width": 4.5}},
                    {"space_id": "upper_atrium", "type": "void", "zone": "public", "area_preferred": 160, "constraints": {"ceiling_height": 5.0, "min_width": 9.0}},
                    {"space_id": "upper_concourse", "type": "corridor", "zone": "circulation", "area_preferred": 140, "adjacency": [{"to": "upper_atrium", "relationship": "around", "importance": "required"}], "constraints": {"ceiling_height": 4.5, "min_width": 5.0}},
                    {"space_id": "food_hall", "unit_id": "food_hall", "type": "shop", "zone": "public", "area_preferred": 96, "adjacency": [{"to": "upper_concourse", "relationship": "connected", "importance": "required"}], "constraints": {"ceiling_height": 4.5, "min_width": 6.0}},
                    {"space_id": "shop_d_l1", "unit_id": "shop_d", "type": "shop", "zone": "public", "area_preferred": 62, "adjacency": [{"to": "upper_concourse", "relationship": "connected", "importance": "required"}], "constraints": {"ceiling_height": 4.5, "min_width": 5.0}},
                    {"space_id": "shop_e_l1", "unit_id": "shop_e", "type": "shop", "zone": "public", "area_preferred": 60, "adjacency": [{"to": "upper_concourse", "relationship": "connected", "importance": "required"}], "constraints": {"ceiling_height": 4.5, "min_width": 5.0}},
                    {"space_id": "mall_bathroom_l1", "type": "bathroom", "zone": "service", "area_preferred": 18, "constraints": {"ceiling_height": 3.5, "min_width": 2.5}}
                ]
            }
        ]
    }
})";
}

std::string TestHelpers::CreateCBDResidential() {
        return R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "cbd_residential",
    "style": {
        "category": "urban_modern",
        "facade": "glass_metal",
        "roof": "flat",
        "window_style": "standard",
        "material": "concrete_glass"
    },
    "mass": {
        "footprint_area": 360,
        "floors": 4,
        "total_height": 13.6
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "arrival_floor",
                "spaces": [
                    {"space_id": "res_core_gf", "type": "core", "zone": "service", "area_preferred": 80, "constraints": {"connects_to_floor": 1, "ceiling_height": 4.0, "min_width": 4.5}},
                    {"space_id": "res_lobby", "type": "lobby", "zone": "circulation", "area_preferred": 72, "constraints": {"natural_light": "preferred", "ceiling_height": 4.2, "min_width": 5.0}},
                    {"space_id": "amenity_lounge", "type": "living", "zone": "public", "area_preferred": 44, "constraints": {"natural_light": "required", "ceiling_height": 3.5, "min_width": 4.5}},
                    {"space_id": "mail_room", "type": "storage", "zone": "service", "area_preferred": 12, "constraints": {"ceiling_height": 3.0, "min_width": 2.5}}
                ]
            },
            {
                "level": 1,
                "name": "amenity_floor",
                "spaces": [
                    {"space_id": "res_core_l1", "type": "core", "zone": "service", "area_preferred": 72, "constraints": {"connects_from_floor": 0, "connects_to_floor": 2, "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "res_corridor_l1", "type": "corridor", "zone": "circulation", "area_preferred": 46, "constraints": {"ceiling_height": 3.0, "min_width": 2.5}},
                    {"space_id": "sky_lounge_l1", "type": "living", "zone": "public", "area_preferred": 42, "constraints": {"natural_light": "required", "ceiling_height": 3.2, "min_width": 5.0}},
                    {"space_id": "cowork_l1", "type": "office", "zone": "public", "area_preferred": 30, "constraints": {"natural_light": "required", "ceiling_height": 3.2, "min_width": 4.0}},
                    {"space_id": "gym_l1", "type": "office", "zone": "public", "area_preferred": 28, "constraints": {"natural_light": "preferred", "ceiling_height": 3.2, "min_width": 4.0}},
                    {"space_id": "amenity_bathroom_l1", "type": "bathroom", "zone": "service", "area_preferred": 10, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}}
                ]
            },
            {
                "level": 2,
                "name": "residential_floor_2",
                "spaces": [
                    {"space_id": "res_core_l2", "type": "core", "zone": "service", "area_preferred": 72, "constraints": {"connects_from_floor": 1, "connects_to_floor": 3, "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "res_corridor_l2", "type": "corridor", "zone": "circulation", "area_preferred": 46, "constraints": {"ceiling_height": 3.0, "min_width": 2.5}},
                    {"space_id": "living_a_l2", "unit_id": "cbd_a_l2", "type": "living", "zone": "public", "area_preferred": 28, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "bedroom_a_l2", "unit_id": "cbd_a_l2", "type": "bedroom", "zone": "private", "area_preferred": 16, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}},
                    {"space_id": "bathroom_a_l2", "unit_id": "cbd_a_l2", "type": "bathroom", "zone": "service", "area_preferred": 7, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}},
                    {"space_id": "living_b_l2", "unit_id": "cbd_b_l2", "type": "living", "zone": "public", "area_preferred": 28, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "bedroom_b_l2", "unit_id": "cbd_b_l2", "type": "bedroom", "zone": "private", "area_preferred": 16, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}},
                    {"space_id": "bathroom_b_l2", "unit_id": "cbd_b_l2", "type": "bathroom", "zone": "service", "area_preferred": 7, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}}
                ]
            },
            {
                "level": 3,
                "name": "residential_floor_3",
                "spaces": [
                    {"space_id": "res_core_l3", "type": "core", "zone": "service", "area_preferred": 72, "constraints": {"connects_from_floor": 2, "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "res_corridor_l3", "type": "corridor", "zone": "circulation", "area_preferred": 46, "constraints": {"ceiling_height": 3.0, "min_width": 2.5}},
                    {"space_id": "living_a_l3", "unit_id": "cbd_a_l3", "type": "living", "zone": "public", "area_preferred": 28, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "bedroom_a_l3", "unit_id": "cbd_a_l3", "type": "bedroom", "zone": "private", "area_preferred": 16, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}},
                    {"space_id": "bathroom_a_l3", "unit_id": "cbd_a_l3", "type": "bathroom", "zone": "service", "area_preferred": 7, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}},
                    {"space_id": "living_b_l3", "unit_id": "cbd_b_l3", "type": "living", "zone": "public", "area_preferred": 28, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 4.0}},
                    {"space_id": "bedroom_b_l3", "unit_id": "cbd_b_l3", "type": "bedroom", "zone": "private", "area_preferred": 16, "constraints": {"natural_light": "required", "ceiling_height": 3.0, "min_width": 3.5}},
                    {"space_id": "bathroom_b_l3", "unit_id": "cbd_b_l3", "type": "bathroom", "zone": "service", "area_preferred": 7, "constraints": {"ceiling_height": 3.0, "min_width": 2.0}}
                ]
            }
        ]
    }
})";
}

} // namespace Test
} // namespace Building
} // namespace Moon
