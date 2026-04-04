#include "LayoutTypologyPolicy.h"

namespace Moon {
namespace Building {

float GetTypologyPlacementScoreBoost(BuildingTypology typology,
                                     const SemanticSpace& space,
                                     int floorLevel) {
    if (typology == BuildingTypology::Office) {
        float score = 0.0f;
        if (floorLevel == 0 && (space.type == "lobby" || space.type == "entrance")) {
            score += 700.0f;
        }
        if (space.type == "office" || space.type == "meeting_room") {
            score += 220.0f;
        }
        if (space.type == "mechanical") {
            score += 120.0f;
        }
        return score;
    }

    if (typology == BuildingTypology::Retail) {
        float score = 0.0f;
        if (space.type == "void") score += 11300.0f;
        if (space.type == "corridor") score += 8650.0f;
        if (space.type == "shop") score += 8450.0f;
        if (space.type == "stairs") score += 7800.0f;
        if (space.type == "lobby") score += 5200.0f;
        if (space.type == "entrance") score += 4700.0f;
        if (space.type == "core" || space.type == "elevator") score += 7900.0f;
        return score;
    }

    if (typology == BuildingTypology::Residential) {
        float score = 0.0f;
        if (space.type == "corridor" || space.type == "lobby") {
            score += 450.0f;
        }
        if (!space.unitId.empty()) {
            score += 180.0f;
        }
        return score;
    }

    if (typology == BuildingTypology::Villa) {
        float score = 0.0f;
        if (floorLevel == 0 && (space.type == "living" || space.type == "kitchen" || space.type == "entrance")) {
            score += 180.0f;
        }
        if (floorLevel > 0 && space.type == "bedroom") {
            score += 120.0f;
        }
        if (space.type == "core" || space.type == "stairs" || space.type == "elevator") {
            score -= 600.0f;
        }
        return score;
    }

    return 0.0f;
}

bool ShouldPreferCirculationAdjacency(BuildingTypology typology,
                                      const SemanticSpace& space) {
    if (typology == BuildingTypology::Office) {
        return space.zone == "public" && space.unitId.empty() &&
            (space.type == "office" || space.type == "meeting_room" || space.type == "shop");
    }

    if (typology == BuildingTypology::Residential) {
        return space.zone == "public" && space.unitId.empty();
    }

    if (typology == BuildingTypology::Retail) {
        return space.type == "shop" || space.type == "bathroom";
    }

    return false;
}

} // namespace Building
} // namespace Moon
