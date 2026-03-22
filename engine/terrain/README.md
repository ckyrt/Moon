# Engine Terrain

`engine/terrain` is the dedicated runtime module for terrain data and terrain-specific logic.

Current scope for phase 1:

- define a stable terrain data model
- keep the terrain runtime independent from AI, editor, and renderer-specific input flows
- provide chunk-aware dirty tracking for future mesh, material, collision, and vegetation builds

Planned ownership:

- `Heightmap`: primary large-world surface representation
- `TerrainData`: unified terrain payload shared by AI generation, editor tools, and file import
- `TerrainSystem`: runtime state, chunk layout, dirty tracking
- `TerrainComponent`: scene-facing entry point

Non-goals for phase 1:

- final terrain rendering
- splatmap painting tools
- water integration
- vegetation scattering
- cave runtime

Design rule:

Different input methods must converge into the same terrain data model:

- AI text generation -> `TerrainData`
- editor sculpt/paint -> `TerrainData`
- imported heightmap -> `TerrainData`

The runtime should only consume the unified data and build the world from it.
