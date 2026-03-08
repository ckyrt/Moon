#pragma once

#include <string>

namespace Moon {
namespace Building {
namespace Test {

/**
 * @brief Test helper utilities
 */
class TestHelpers {
public:
    /**
     * @brief Load JSON test data from assets/building/ directory
     * @param filename Relative path from assets/building/ (e.g., "basic/minimal_building.json")
     * @return JSON string content
     */
    static std::string LoadFromFile(const std::string& filename);

    /**
     * @brief Create a minimal valid building JSON
     */
    static std::string CreateMinimalValidBuilding();

    /**
     * @brief Create a simple room JSON
     */
    static std::string CreateSimpleRoom();

    /**
     * @brief Create a multi-floor building JSON
     */
    static std::string CreateMultiFloorBuilding();

    /**
     * @brief Create a villa with multiple masses
     */
    static std::string CreateVilla();

    /**
     * @brief Create an apartment building (5+ floors)
     */
    static std::string CreateApartmentBuilding();

    /**
     * @brief Create L-shaped building (multiple masses)
     */
    static std::string CreateLShapedBuilding();

    /**
     * @brief Create a realistic 3-floor luxury villa (18+ rooms)
     */
    static std::string CreateLuxuryVilla();

    /**
     * @brief Create a mixed-use office tower with lobby/core office floors
     */
    static std::string CreateOfficeTower();

    /**
     * @brief Create a shopping mall with atrium, circulation ring, and shops
     */
    static std::string CreateShoppingMall();

    /**
     * @brief Create a CBD residential tower with shared core and multiple units
     */
    static std::string CreateCBDResidential();

    /**
     * @brief Create invalid JSON (missing required fields)
     */
    static std::string CreateInvalidJSON_MissingGrid();
    
    static std::string CreateInvalidJSON_WrongGridSize();
    
    static std::string CreateInvalidJSON_NonAlignedCoordinates();
};

} // namespace Test
} // namespace Building
} // namespace Moon
