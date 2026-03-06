#include "TestHelpers.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <string>

namespace Moon {
namespace Building {
namespace Test {

// ========================================
// Helper function to load JSON from file
// ========================================
static std::string LoadFromFile(const std::string& relativePath) {
    // Try multiple possible locations
    std::vector<std::string> searchPaths = {
        "assets/building/" + relativePath,
        "../assets/building/" + relativePath,
        "../../assets/building/" + relativePath,
        "../../../assets/building/" + relativePath
    };
    
    for (const auto& path : searchPaths) {
        std::ifstream file(path);
        if (file.good()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            return buffer.str();
        }
    }
    
    throw std::runtime_error("Failed to load test data: " + relativePath);
}

// ========================================
// Complex Building Scenarios
// ========================================
// All test data now loaded from assets/building/ directory
// This makes the data accessible for AI reference and easier to maintain

std::string TestHelpers::CreateVilla() {
    return LoadFromFile("residential/villa.json");
}

std::string TestHelpers::CreateApartmentBuilding() {
    return LoadFromFile("residential/apartment.json");
}

std::string TestHelpers::CreateLShapedBuilding() {
    return LoadFromFile("complex/l_shaped.json");
}

std::string TestHelpers::CreateLuxuryVilla() {
    return LoadFromFile("residential/luxury_villa.json");
}

} // namespace Test
} // namespace Building
} // namespace Moon
