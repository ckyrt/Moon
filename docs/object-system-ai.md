# Object System AI Guide

This file is the AI-facing guide for generating new object assets.

## Goal

When a player asks for a new object, generate object data that fits the current Moon object pipeline.

## Output Targets

There are two valid outputs.

### 1. New semantic object entry

Use this when adding a new user-facing object family.

Write or extend:

- `assets/objects/catalog.json`

Required ideas:

- stable `id`
- user-facing `display_name`
- `category`
- `graph_asset`
- `defaults`
- `parameters`
- optional `variants`

### 2. New graph asset JSON

Use this when the requested shape needs a new reusable implementation asset.

Write under:

- `assets/objects/components/.../*.json`

Register it in:

- `assets/objects/index.json`

## Use These Core Keywords

### Semantic catalog keywords

- `id`
- `display_name`
- `description`
- `category`
- `graph_asset`
- `tags`
- `defaults`
- `parameters`
- `variants`
- `base_object`

### Graph JSON keywords

- `schema_version`
- `id`
- `name`
- `parameters`
- `anchors`
- `root`
- node `type`
- `primitive`
- `group`
- `reference`
- `csg`
- `light`
- `transform`
- `position`
- `rotation_euler`
- `scale`
- `overrides`
- `material`
- `children`
- `operation`
- `left`
- `right`

## Format Rules

- output valid JSON only for asset files
- use `schema_version: 1` unless an existing file clearly uses a newer schema
- use centimeters for object graph dimensions
- prefer parameterized reusable assets over hardcoded one-off geometry
- use `reference` nodes to reuse existing parts
- use `csg` only when subtraction/boolean composition is actually needed
- keep transforms simple and readable
- prefer `group.output.mode = "separate"` unless merge behavior is required

## Expression Rules

- only use plain numbers or simple `$parameter_name` references in transform values
- do not use array indexing expressions such as `$anchor[0]`, `$anchor[1]`, or `$anchor[2]`
- do not invent expression syntax that is not already shown in the provided examples
- if you define `anchors`, treat them as metadata or whole positions; do not index into them inside expressions

Correct:

```json
{
  "transform": {
    "position": [40, 0, 0]
  }
}
```

Also correct:

```json
{
  "parameters": {
    "wheel_x": 40,
    "wheel_y": 0
  },
  "transform": {
    "position": ["$wheel_x", "$wheel_y", 0]
  }
}
```

Wrong:

```json
{
  "anchors": {
    "wheel_center": [40, 0, 0]
  },
  "transform": {
    "position": ["$wheel_center[0]", "$wheel_center[1]", 0]
  }
}
```

## Material Rules

Use only materials already allowed by:

- `assets/objects/index.json`

Common safe values:

- `wood`
- `wood_painted`
- `metal`
- `fabric`
- `plastic`
- `concrete`
- `glass`

## Reuse Strategy

Before creating a new asset, check:

- `assets/objects/catalog.json` for an existing semantic family
- `assets/objects/index.json` for an existing graph asset
- `assets/objects/components/furniture/parts/*`
- `assets/objects/components/architectural/*`

Prefer:

- inheritance with `base_object`
- variant overrides
- part reuse through `reference`

Create a new part only when the shape family is genuinely different.

## Good Reference Files

- semantic catalog: `assets/objects/catalog.json`
- graph registry: `assets/objects/index.json`
- reusable furniture graph: `assets/objects/components/furniture/chair_v1.json`
- reusable architectural graph: `assets/objects/components/architectural/wall_with_window_v1.json`
- composed preview scene: `assets/objects/test_room.json`
- CSG opening example: `assets/objects/test_walls_with_openings.json`

## Recommended Workflow

1. Identify whether the request needs a new semantic object, a new graph asset, or both
2. Reuse existing parts if possible
3. Create or update the graph JSON
4. Register the graph in `assets/objects/index.json` if new
5. Add or update the semantic entry in `assets/objects/catalog.json`
6. Keep names stable and machine-friendly

## Avoid

- inventing unsupported node types
- inventing unsupported material names
- mixing semantic catalog fields into component graph files
- hardcoding a one-off asset when a parameterized reusable asset is appropriate
- treating CSG as the semantic owner of the object system
