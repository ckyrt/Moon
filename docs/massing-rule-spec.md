# Massing Rule Spec

## Purpose

This document is the strict rule reference for AI-generated `EngineMassing` JSON.

The AI must follow this document exactly.
If a field, node type, or parameter is not listed here, the AI must not invent it.

This document is intended for hidden system use inside the editor chat workflow.
End users do not need to see or edit this file directly.

## Output Contract

The AI must return exactly one valid JSON object.

The AI must not:

- wrap the JSON in markdown fences
- add commentary before or after the JSON
- invent unsupported node types
- invent unsupported primitive types
- emit partial JSON

## Root Object

Allowed top-level fields:

- `schema_version`: integer, usually `1`
- `version`: integer, usually `1`
- `name`: string
- `description`: string
- `unit`: string, use `"meter"`
- `ai`: object
- `editor`: object
- `root`: object, required

Example:

```json
{
  "schema_version": 1,
  "version": 1,
  "name": "office_tower",
  "description": "Curated office tower",
  "unit": "meter",
  "ai": {
    "source": "editor_chat"
  },
  "editor": {
    "exposed_params": ["height"]
  },
  "root": {
    "type": "group",
    "name": "root",
    "children": []
  }
}
```

## Node Model

Every node is a JSON object.

Common optional fields allowed on any node:

- `id`: string
- `name`: string
- `type`: string, required
- `material`: string
- `transform`: object
- `params`: object
- `editor`: object

Inline parameters are also supported.
That means numeric fields such as `height`, `radius`, `segments`, `size_x` may appear directly on the node and will be merged into `params`.
Prefer `params` for consistency.

## Transform Object

Allowed fields:

- `position`: float[3]
- `rotation`: float[3]
- `scale`: float[3]

Use meters for positions.
Use degrees for rotations.

Example:

```json
"transform": {
  "position": [0.0, 12.0, 0.0],
  "rotation": [0.0, 15.0, 0.0],
  "scale": [1.0, 1.0, 1.0]
}
```

## Allowed Node Types

Only these `type` values are allowed:

- `primitive`
- `extrude`
- `revolve`
- `sweep`
- `loft`
- `csg`
- `deform`
- `array`
- `group`
- `reference`

Any other `type` is invalid.

## Reference Node

Required:

- `type: "reference"`
- `ref`

Allowed fields:

- `id`
- `name`
- `material`
- `transform`
- `params`
- `editor`
- `ref`

`ref` rules:

- references an external JSON asset under `assets/massing`
- may be written as a file name like `planned_glazed_rotunda_dome.json`
- may also be written as `massing/<file>.json`
- local in-file modules are not supported yet

Behavior:

- the referenced asset is parsed and expanded during build
- the `reference` node `transform` is applied after the referenced asset is built
- `material` on the `reference` node may override referenced item materials
- recursive reference cycles are invalid

Use `reference` for:

- reusable domes
- roof caps
- skylights
- canopies
- standardized arrival pieces

Example:

```json
{
  "type": "reference",
  "name": "roof_module",
  "ref": "planned_linear_skylight_module.json",
  "transform": {
    "position": [0.0, 22.0, 0.0],
    "rotation": [0.0, 0.0, 0.0],
    "scale": [1.0, 1.0, 1.0]
  }
}
```

## Primitive Node

Required:

- `type: "primitive"`
- `primitive`

Allowed `primitive` values:

- `cube`
- `sphere`
- `cylinder`
- `capsule`
- `cone`
- `torus`

Typical `params`:

- cube: `size_x`, `size_y`, `size_z`
- sphere: `radius`, optional `segments`, `rings`
- cylinder: `radius`, `height`, optional `segments`
- capsule: `radius`, `height`, optional `segments`, `rings`
- cone: `radius`, `height`, optional `segments`
- torus: `radius_outer`, `radius_inner`, optional `segments_major`, `segments_minor`

Example:

```json
{
  "type": "primitive",
  "primitive": "sphere",
  "params": {
    "radius": 8.0,
    "segments": 32,
    "rings": 20
  }
}
```

## Extrude Node

Required:

- `type: "extrude"`
- one closed 2D profile in `profile` or `profiles`
- `params.height`

Profiles:

- 2D points only
- use `closed: true`
- profile lies in the XZ footprint plane
- extrusion height is vertical along Y

Example:

```json
{
  "type": "extrude",
  "params": {
    "height": 24.0
  },
  "profiles": [
    {
      "closed": true,
      "points": [[-10, -8], [10, -8], [10, 8], [-10, 8]]
    }
  ]
}
```

## Revolve Node

Required:

- `type: "revolve"`
- one 2D profile in `profile` or `profiles`

Expected usage:

- profile is usually open
- profile is revolved around the Y axis

Typical `params`:

- `segments`

Use `revolve` for:

- domes
- rotundas
- bowls
- ring-like crowns

Do not use `revolve` as the default main body for ordinary office or residential towers.

## Sweep Node

Required:

- `type: "sweep"`
- one closed 2D profile
- one 3D path

Use `sweep` for:

- canopies
- bridges
- station roofs
- long roof spines

Do not use `sweep` as the default main occupiable tower body.

## Loft Node

Required:

- `type: "loft"`
- two or more closed 2D profiles
- `params.levels`

`params.levels`:

- array of floats
- same count as `profiles`
- increasing vertical Y levels

Typical `params`:

- `levels`
- `segments_per_span`

Use `loft` for:

- tapered towers
- vase-like towers
- towers with changing floor plate
- sculpted but still architectural shells

## CSG Node

Required:

- `type: "csg"`
- `operation`
- exactly 2 children

Allowed `operation` values:

- `union`
- `subtract`
- `intersect`

Use `csg` for:

- atriums
- terrace cuts
- podium carvings
- courtyard voids

## Deform Node

Required:

- `type: "deform"`
- exactly 1 child

Supported deform modes through `params.mode`:

- `twist`
- `taper`
- `bend`
- `shear`
- `bulge`

Typical `params`:

- `angle`
- `bottom_scale`
- `top_scale`
- `subdivide`
- `shear_x`
- `shear_z`
- `amount`
- `axis`

Use `deform` carefully.
Prefer moderate values.

## Array Node

Required:

- `type: "array"`
- exactly 1 child

Typical `params`:

- `count_x`
- `count_y`
- `count_z`
- `spacing_x`
- `spacing_y`
- `spacing_z`
- `rotate_step_y`

Use `array` for:

- facade fins
- repeated canopies
- pavilion repetitions
- structural rhythm

Do not use `array` as the only main building mass.

## Group Node

Required:

- `type: "group"`
- `children`

Use `group` as the root in most architectural outputs.
Use `reference` instead of copying large reusable subgraphs inline when a standard module already exists.

## Curve Formats

### 2D Curve

Allowed fields:

- `id`
- `closed`
- `points`

Point formats:

- `[x, y]`
- `{ "x": value, "y": value }`

### 3D Curve

Allowed fields:

- `id`
- `closed`
- `points`

Point formats:

- `[x, y, z]`
- `{ "x": value, "y": value, "z": value }`

## Materials

Recommended material names:

- `stone`
- `glass`
- `metal`
- `plaster`

Use simple material names already common in current massing samples.

## Naming Guidance

Prefer stable architectural names:

- `podium`
- `tower_shell`
- `atrium_void`
- `roof_cap`
- `entry_canopy`
- `cinema_volume`
- `lower_sphere`
- `roof_module`
- `rotunda_dome_module`

Avoid vague names like:

- `thing`
- `shape1`
- `random_node`

## Hard Validation Rules

The AI must always satisfy these:

- `root` must exist
- every node must have a valid `type`
- `primitive` nodes must have a valid `primitive`
- `csg` nodes must have exactly 2 children
- `array` nodes must have exactly 1 child
- `deform` nodes must have exactly 1 child
- `reference` nodes must have a non-empty `ref`
- `extrude` must have a closed 2D profile and a positive height
- `sweep` must have one profile and one path
- `loft` must have profile count equal to `levels` count
- numbers should be finite and positive where required

## Repair Guidance

If the engine returns an error, the AI must repair the current JSON instead of starting from scratch unless the structure is unrecoverable.

Common repairs:

- invalid node type -> replace with supported type
- invalid primitive type -> replace with supported primitive
- missing `primitive` on primitive node -> add it
- wrong child count on `csg` / `array` / `deform` -> fix child count
- unsupported top cap -> convert to `extrude` or valid `primitive`
- repeated reusable geometry -> replace with `reference` when a standard module exists

## AI Reuse Strategy

The AI should prefer reusable standard modules before inventing new bespoke geometry.

Preferred order:

1. use a supported `reference` module if one matches the request
2. combine core body massing with referenced roof or arrival modules
3. only generate new bespoke geometry when no existing module fits

Good early module candidates:

- dome modules
- skylight modules
- entry canopy modules
- roof cap modules
- portico modules

## Preferred Hidden Usage

When editor chat is implemented, the system should inject this document into the hidden prompt context together with:

- `docs/massing-building-guide.md`
- selected `assets/massing/planned_*.json` examples
- current JSON, if any
- latest parse/build error, if any

The user should not need to know this file exists.
