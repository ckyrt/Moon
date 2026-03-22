# Terrain Module

## Responsibilities

- store and manage large-world terrain surface data
- provide a stable runtime module for terrain-specific systems
- keep terrain generation independent from editor tooling and AI input flows
- expose chunk-based dirty tracking for future mesh, material, collision, water, and vegetation rebuilds

## Current Runtime Structure

Phase 1 is centered around these runtime types:

- `Heightmap` - normalized terrain height storage
- `TerrainData` - unified terrain payload shared by AI generation, editor tools, and file import
- `TerrainProfile` - runtime configuration such as chunk size and height scale
- `TerrainRuntimeState` - derived state for chunk counts and dirty status
- `ITerrainSystem` / `TerrainSystem` - terrain runtime entry point
- `TerrainComponent` - scene-facing wrapper

## Data Flow Rule

Different input methods must converge into the same terrain data model:

- AI text generation -> `TerrainData`
- editor sculpting and painting -> `TerrainData`
- imported heightmaps -> `TerrainData`

The runtime should only consume the unified data and build the world from it.

## Dependencies

- `engine/core` for scene and component integration
- `engine/render` for future terrain rendering
- `engine/environment` as a read-only input source for weather, wind, and lighting driven terrain effects

## Current Phase

Phase 1 currently includes:

- dedicated `engine/terrain` static library
- heightmap storage
- terrain profile and runtime state
- chunk layout and dirty tracking
- scene-facing `TerrainComponent`

## Planned Next Steps

1. build terrain mesh generation from `Heightmap`
2. rebuild only dirty chunks
3. add terrain material layers and painting
4. feed terrain data into future water and vegetation modules

## Cave Strategy

The primary world surface stays heightmap-based.

Future cave support should be added as a separate module that connects to terrain entrances, instead of forcing the whole terrain runtime to become voxel-based on day one.
