#include "building/BuildingPipeline.h"
#include "building/BuildingToCSGConverter.h"
#include "building/tests/TestHelpers.h"
#include <iostream>

int main() {
    using namespace Moon::Building;
    using namespace Moon::Building::Test;
    
    BuildingPipeline pipeline;
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    if (pipeline.ProcessBuilding(inputJson, building, errorMsg)) {
        std::string csgJson = BuildingToCSGConverter::Convert(building);
        std::cout << csgJson << std::endl;
    } else {
        std::cerr << "Error: " << errorMsg << std::endl;
    }
    
    return 0;
}
