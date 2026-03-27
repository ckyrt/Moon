#include "TestHelpers.h"

namespace Moon {
namespace Building {
namespace Test {

// ========================================
// Complex Building Scenarios
// ========================================

std::string TestHelpers::CreateVilla() {
    return LoadFromFile("fixtures/villa.json");
}

std::string TestHelpers::CreateApartmentBuilding() {
    return LoadFromFile("fixtures/apartment_building.json");
}

std::string TestHelpers::CreateLShapedBuilding() {
    return LoadFromFile("fixtures/l_shaped_villa.json");
}

std::string TestHelpers::CreateLuxuryVilla() {
    return LoadFromFile("fixtures/luxury_villa.json");
}

std::string TestHelpers::CreateOfficeTower() {
    return LoadFromFile("fixtures/office_tower.json");
}

std::string TestHelpers::CreateShoppingMall() {
    return LoadFromFile("fixtures/shopping_mall.json");
}

std::string TestHelpers::CreateComplexShoppingMall() {
    return LoadFromFile("complex_shopping_mall_demo.json");
}

std::string TestHelpers::CreateCBDResidential() {
    return LoadFromFile("fixtures/cbd_residential.json");
}

std::string TestHelpers::CreateTownhouseVilla() {
    return LoadFromFile("reference/townhouse_villa.json");
}

std::string TestHelpers::CreateMidriseApartment() {
    return LoadFromFile("reference/midrise_apartment.json");
}

std::string TestHelpers::CreateNeighborhoodOffice() {
    return LoadFromFile("reference/neighborhood_office.json");
}

std::string TestHelpers::CreateRetailCenter() {
    return LoadFromFile("reference/retail_center.json");
}

std::string TestHelpers::CreateCourtyardVilla() {
    return LoadFromFile("reference/courtyard_villa.json");
}

std::string TestHelpers::CreateSlenderCBDResidential() {
    return LoadFromFile("reference/slender_cbd_residential.json");
}

std::string TestHelpers::CreateCorporateOfficeTower() {
    return LoadFromFile("reference/corporate_office_tower.json");
}

std::string TestHelpers::CreateShoppingCenter() {
    return LoadFromFile("reference/shopping_center.json");
}

} // namespace Test
} // namespace Building
} // namespace Moon
