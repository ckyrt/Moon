# Building Massing System

## Goal

`EngineMassing` is a standalone rule-driven building massing module for Moon.

It is designed for this workflow:

1. User describes the target building shape in chat.
2. AI selects an appropriate generation strategy.
3. AI outputs a JSON rule set instead of raw mesh data.
4. The engine compiles the rule set into a mass model.
5. The user tweaks parameters, transforms, or curves in the editor.
6. The engine recompiles immediately and updates the result.

This keeps the source of truth in rules and parameters instead of editable mesh topology.

## Current Scope

Implemented in this pass:

- standalone `EngineMassing` project
- rule schema for `primitive`, `extrude`, `revolve`, `sweep`, `loft`, `csg`, `deform`, `array`, `group`
- recursive JSON parser / serializer
- compiler from massing rules to current CSG blueprint JSON
- direct compilation support for `primitive`, `group`, `csg`, `array`
- placeholder compilation path for `extrude`, `revolve`, `sweep`, `loft`, `deform`

Not implemented yet:

- real curve meshing for `extrude`, `revolve`, `sweep`, `loft`
- deformation operators
- editor inspector / gizmo UI
- LLM intent classifier and prompt runner
- bidirectional sync from generated blueprint back to editable rule graph

## Rule Format

```json
{
  "schema_version": 1,
  "version": 1,
  "name": "tower_mass",
  "description": "A tapered podium tower generated from chat",
  "unit": "meter",
  "ai": {
    "source": "chat",
    "strategy": "csg"
  },
  "editor": {
    "exposed_params": ["tower_width", "tower_depth", "tower_height"]
  },
  "root": {
    "id": "root_group",
    "name": "root_group",
    "type": "group",
    "children": [
      {
        "id": "podium",
        "name": "podium",
        "type": "primitive",
        "primitive": "cube",
        "material": "concrete",
        "params": {
          "size_x": 30.0,
          "size_y": 8.0,
          "size_z": 24.0
        },
        "transform": {
          "position": [0.0, 4.0, 0.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        }
      },
      {
        "id": "tower",
        "name": "tower",
        "type": "primitive",
        "primitive": "cube",
        "material": "glass",
        "params": {
          "size_x": 16.0,
          "size_y": 48.0,
          "size_z": 16.0
        },
        "transform": {
          "position": [0.0, 32.0, 0.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        }
      }
    ]
  }
}
```

## Recommended Integration

Suggested pipeline:

1. `Chat text -> AI strategy selector`
2. `AI strategy selector -> EngineMassing RuleSet JSON`
3. `MassRuleParser::ParseFromString`
4. `MassRuleCompiler::CompileToBlueprint`
5. existing `CSG::BlueprintLoader` / `CSGBuilder`
6. runtime scene update in editor

## Strategy Selection Guidelines

Suggested AI routing rules:

- `primitive`: boxy modern buildings, simple towers, base masses
- `extrude`: footprint + height driven buildings, setbacks, profile-based shells
- `revolve`: domes, silos, round towers, bowls
- `sweep`: curved bridges, canopies, arched envelopes
- `loft`: organic transitions between multiple floor profiles
- `csg`: carved podiums, voids, terraces, subtractive facade cuts
- `deform`: leaning, twisting, tapering, bulging forms
- `array`: repetitive balconies, fins, columns, modular towers

## Next Steps

Best next implementation steps:

1. Add a real `extrude` compiler that converts closed 2D profiles into manifold meshes.
2. Add editor metadata for sliders, curve handles, and presets.
3. Add a prompt contract so the LLM always outputs schema-valid JSON.
4. Add a live preview bridge in the editor that recompiles on parameter change.

## Geometry Path

Besides `MassRuleCompiler`, the module now also includes `MassMeshBuilder`.

That path generates runtime meshes directly for:

- `primitive`
- `extrude`
- `revolve`
- `sweep`
- `loft`
- `deform`
- `array`
- `group`
- `csg` via mesh boolean operations

This is the path intended for real building massing forms that cannot be represented cleanly by the current CSG blueprint schema alone.

## Remaining Gaps

The system now covers most common architectural massing families, but a few production-grade capabilities are still future work:

- robust triangulation for highly concave profiles with holes
- high-quality frame transport for long sweep paths with minimal twist
- adaptive tessellation / LOD controls
- NURBS / bezier curve editing instead of polyline-only curves
- topology-safe advanced deform stacks and modifier history
- editor viewport handles and live parameter widgets

## Node Tree Model

The correct mental model is a rule tree, not a single generator.

Each node represents one step in shape construction:

- source nodes: `primitive`, `extrude`, `revolve`, `sweep`, `loft`
- modifier nodes: `deform`, `array`
- composition nodes: `group`, `csg`

So a final building mass is usually the result of many sequential and nested operations.
`CSG` is only one node type inside that tree, not the whole system.

A realistic tower may be built as:

1. `extrude` a podium footprint
2. `loft` the tower shaft across multiple profiles
3. `deform` the shaft with twist or taper
4. `array` fins or balcony modules
5. `csg` subtract atriums, sky gardens, or corner cuts
6. `group` everything into one editable massing assembly

See sample: `assets/massing/complex_twisted_tower.json`
