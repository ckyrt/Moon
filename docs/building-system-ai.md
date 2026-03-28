# Building System AI Guide

This file is the AI-facing guide for generating new building assets.

## Goal

When a player asks for a new building, generate semantic `moon_building` JSON that the current pipeline can validate and preview.

## Output Rules

- output valid JSON only for building asset files
- do not output prose or comments inside the JSON
- generate architecture, not meshes
- do not generate vertices, normals, UVs, CSG trees, or renderer-specific data
- keep values grid-friendly

## Root Format

Use this root shape:

```json
{
  "schema": "moon_building",
  "grid": 0.5,
  "building_type": "apartment",
  "style": {},
  "mass": {},
  "program": {
    "floors": []
  }
}
```

## Use These Core Keywords

### Root keywords

- `schema`
- `grid`
- `building_type`
- `style`
- `mass`
- `program`

### Floor keywords

- `level`
- `name`
- `spaces`

### Space keywords

- `space_id`
- `unit_id`
- `type`
- `zone`
- `area_min`
- `area_max`
- `area_preferred`
- `priority`
- `adjacency`
- `constraints`

### Adjacency keywords

- `to`
- `relationship`
- `importance`

## Supported Building Types

- `villa`
- `apartment`
- `office`
- `office_tower`
- `cbd_residential`
- `mall`
- `shopping_center`
- `retail_center`

## Preferred Space Types

- `core`
- `void`
- `corridor`
- `entrance`
- `lobby`
- `stairs`
- `mechanical`
- `living`
- `dining`
- `kitchen`
- `bedroom`
- `bathroom`
- `office`
- `meeting_room`
- `shop`
- `storage`
- `laundry`
- `garage`
- `balcony`
- `terrace`

## Allowed Zones

- `public`
- `private`
- `service`
- `circulation`

## Typology Hints

### villa

- public spaces on lower floors
- private bedrooms on upper floors when multi-story
- simple stair/core logic

### apartment

- shared `core`
- shared `corridor`
- multiple residential `unit_id` groups

### office_tower

- lobby/support/podium logic on lower floors
- office-focused upper floors
- `core` on every floor

### cbd_residential

- shared `core`
- shared `corridor`
- repeated residential units
- optional amenity floor above ground

### mall / shopping_center / retail_center

- include a `void`
- place `corridor` around the void when appropriate
- connect every `shop` to circulation

## Relationship Hints

Use relationships such as:

- `connected`
- `nearby`
- `share_wall`
- `around`
- `facing`

## Good Reference Files

- demo building: `assets/building/test_building.json`
- fixtures: `assets/building/fixtures/*.json`
- references: `assets/building/reference/*.json`
- asset list: `assets/building/catalog.json`

## Recommended Workflow

1. choose the building type
2. define mass and floor count
3. define floor-by-floor spaces
4. add `core`, `corridor`, `void`, and `unit_id` structure when needed
5. add adjacency and constraints
6. keep the result simple enough for validators and generators

## Avoid

- generating geometry-level data
- inventing new root schema names
- omitting circulation in multi-space buildings
- using inconsistent `space_id`
- forcing detailed shape instructions when semantic program data is enough
