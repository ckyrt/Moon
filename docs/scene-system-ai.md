# Scene System AI Guide

This file defines the first-pass AI-facing contract for scene composition.

## Goal

Keep asset design AI-first while keeping scene edits structured and safe.

- buildings and objects may be generated as full JSON assets
- scene changes should be expressed as `scene_edit_ops`
- the scene file stores placed building instances and object instances together

## Scene Root

Use this root shape:

```json
{
  "schema": "moon_scene_design",
  "version": 1,
  "building_instances": [],
  "object_instances": []
}
```

## Building Instances

Each building instance should include:

- `instance_id`
- `name`
- `building_json`
- `transform`

Example:

```json
{
  "instance_id": "building_main",
  "name": "Main Building",
  "building_json": "{ ... moon_building json ... }",
  "transform": {
    "position": [0.0, 0.0, 0.0],
    "rotation_euler": [0.0, 0.0, 0.0],
    "scale": [1.0, 1.0, 1.0]
  }
}
```

## Object Instances

Each object instance should include:

- `instance_id`
- `name`
- either `asset_id` or `object_json`
- optional `parameter_overrides`
- `transform`

Example:

```json
{
  "instance_id": "table_living_01",
  "name": "Living Table",
  "asset_id": "table_v1",
  "parameter_overrides": {
    "w": 140.0
  },
  "transform": {
    "position": [2.0, 0.0, 4.0],
    "rotation_euler": [0.0, 90.0, 0.0],
    "scale": [1.0, 1.0, 1.0]
  }
}
```

## Scene Edit Ops Root

Use this root shape:

```json
{
  "operations": []
}
```

## Supported Ops

### `place_building`

Required fields:

- `op`
- `instance`

`instance` must be a valid building instance payload.

### `place_object`

Required fields:

- `op`
- `instance`

`instance` must be a valid object instance payload.

### `move_instance`

Required fields:

- `op`
- `instance_id`

Use one of:

- `position` for absolute move
- `delta` for relative move

### `remove_instance`

Required fields:

- `op`
- `instance_id`

### `replace_instance_asset`

Required fields:

- `op`
- `instance_id`

Building replacement:

- `building_json`

Object replacement:

- `asset_id` or `object_json`

Optional:

- `parameter_overrides`

## Design Boundary

Use this split:

- asset design change: generate or regenerate full building/object JSON
- scene placement change: emit `scene_edit_ops`

Examples:

- "make this house 30 floors" -> regenerate building asset JSON
- "move the table east by 2 meters" -> `move_instance`
- "delete the chair" -> `remove_instance`
- "place this building at the origin" -> `place_building`

## Avoid

- directly rewriting the whole scene file for small placement edits
- inventing unsupported scene ops
- mixing scene instance fields into building asset JSON
- mixing scene instance fields into object blueprint JSON
