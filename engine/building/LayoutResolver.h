#pragma once

#include "BuildingTypology.h"
#include "BuildingTypes.h"
#include "SemanticBuildingTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Moon {
namespace Building {

class LayoutResolver {
public:
    LayoutResolver();
    ~LayoutResolver();

    bool Resolve(const SemanticBuilding& input,
                 BuildingDefinition& output,
                 std::string& error);

    void SetGridSize(float gridSize) { m_gridSize = gridSize; }
    void SetVerbose(bool verbose) { m_verbose = verbose; }

private:
    struct AllocatedSpace {
        std::string spaceId;
        std::string unitId;
        std::string type;
        float area;
        GridPos2D position;
        GridSize2D size;
        int floorLevel;
    };

    bool ResolveInternal(const SemanticBuilding& input,
                         BuildingDefinition& output,
                         std::string& error);

    bool CalculateFootprint(const SemanticBuilding& input);
    bool AllocateSpaces(const SemanticBuilding& input);
    bool GenerateRectangles(const SemanticBuilding& input);
    void BuildOutput(BuildingDefinition& output, const SemanticBuilding& input);

    BuildingTypology GetLayoutStrategy(const SemanticBuilding& input) const;
    float ComputeMallRingBand(float corridorArea, const AllocatedSpace& voidSpace) const;
    float CalculateTotalArea(const std::vector<SemanticSpace>& spaces) const;
    float GetDefaultAreaForSpaceType(const std::string& spaceType) const;
    float GetDefaultFloorHeight(const SemanticBuilding& input, int floorLevel) const;
    int GetPriorityWeight(const std::string& priority) const;
    float GetTargetAspectRatio(const SemanticBuilding& input, const SemanticSpace& space) const;
    bool IsCirculationSpace(const SemanticSpace& space) const;
    bool IsCoreSpace(const SemanticSpace& space) const;
    bool IsVoidSpace(const SemanticSpace& space) const;
    bool IsServiceSpace(const SemanticSpace& space) const;
    bool RequiresExteriorWall(const SemanticSpace& space) const;
    int GetZoneWeight(const SemanticSpace& space) const;
    std::vector<const SemanticSpace*> BuildPlacementOrder(const SemanticBuilding& input,
                                                          const SemanticFloor& floor) const;
    GridSize2D CalculateOptimalDimensions(float area, float aspectRatio) const;
    GridPos2D SnapToGrid(const GridPos2D& v) const;
    Rect CreateRect(const std::string& rectId, const GridPos2D& origin, const GridSize2D& size) const;
    bool RectanglesOverlap(const GridPos2D& aPos, const GridSize2D& aSize,
                           const GridPos2D& bPos, const GridSize2D& bSize) const;
    bool FitsWithoutOverlap(const GridPos2D& position,
                            const GridSize2D& size,
                            const std::vector<AllocatedSpace>& placedSpaces) const;
    bool FindFirstFitPosition(const GridSize2D& size,
                              const std::vector<AllocatedSpace>& placedSpaces,
                              GridPos2D& outPosition) const;
    bool TryPlaceNearConnectedSpace(const SemanticSpace& space,
                                    const GridSize2D& size,
                                    const std::vector<AllocatedSpace>& placedSpaces,
                                    GridPos2D& outPosition) const;
    bool TryPlaceNearCoreOrCirculation(const SemanticBuilding& input,
                                       const SemanticSpace& space,
                                       const GridSize2D& size,
                                       const std::vector<AllocatedSpace>& placedSpaces,
                                       GridPos2D& outPosition) const;
    bool TryPlaceUsingStairAlignment(const SemanticSpace& space,
                                     const GridSize2D& size,
                                     const std::vector<AllocatedSpace>& placedSpaces,
                                     GridPos2D& outPosition) const;
    bool TryPlaceInCirculationBand(const SemanticBuilding& input,
                                   const SemanticSpace& space,
                                   const GridSize2D& size,
                                   const std::vector<AllocatedSpace>& placedSpaces,
                                   GridPos2D& outPosition) const;
    bool TryPlaceInsideUnitCluster(const SemanticSpace& space,
                                   const GridSize2D& size,
                                   const std::vector<AllocatedSpace>& placedSpaces,
                                   GridPos2D& outPosition) const;
    bool TryPlaceInMallRetailBand(const SemanticBuilding& input,
                                  const SemanticSpace& space,
                                  const GridSize2D& size,
                                  const std::vector<AllocatedSpace>& placedSpaces,
                                  GridPos2D& outPosition) const;
    bool TryPlaceOnExteriorWall(const SemanticSpace& space,
                                const GridSize2D& size,
                                const std::vector<AllocatedSpace>& placedSpaces,
                                GridPos2D& outPosition) const;

    float m_gridSize = 0.5f;
    bool m_verbose = false;
    GridSize2D m_footprint;
    std::vector<AllocatedSpace> m_allocatedSpaces;
    std::vector<Rect> m_reservedRects;
};

} // namespace Building
} // namespace Moon
