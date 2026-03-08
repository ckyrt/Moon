#pragma once

/**
 * @file BuildingToCSGConverter.h
 * @brief Converts a GeneratedBuilding into a CSG Blueprint JSON string
 *
 * This is the bridge between the Building pipeline output and the CSG renderer.
 * It handles:
 *   - Wall cube generation
 *   - Window hole cutting (CSG subtract chain per wall)
 *   - Window frame reference placement (window_v1)
 *   - Door reference placement (door_v1)
 *   - Stair step and landing primitive generation
 *   - Floor slab generation (material by room usage)
 *
 * Produced JSON is suitable for Moon::CSG::BlueprintLoader::ParseFromString().
 */

#include "BuildingTypes.h"
#include <string>

namespace Moon {
namespace Building {

class BuildingToCSGConverter
{
public:
    /**
     * @brief Convert a GeneratedBuilding to a CSG Blueprint JSON string.
     *
    * @param building   Output of BuildingPipeline::ProcessBuilding()
     * @return           JSON string ready for BlueprintLoader::ParseFromString()
     */
    static std::string Convert(const GeneratedBuilding& building);

private:
    // Returns true if the window center lies on this wall segment
    // (same spaceId, same floor level, within 50 cm perpendicular distance, not near endpoints)
    static bool IsWindowOnWall(const Window& window, const WallSegment& wall);

    // Returns a CSG subtract chain node: base - holes[0] - holes[1] - ...
    // Uses the nlohmann::json type internally; declared separately to keep
    // this header free of nlohmann includes (impl detail in .cpp)
};

} // namespace Building
} // namespace Moon
