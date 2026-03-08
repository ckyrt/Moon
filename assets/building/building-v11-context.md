# Building V11 Context

Use this when generating or editing `assets/building/*.json` V11 test files.

## Output Goal

Generate valid JSON for `moon_building_v11` only.

## Allowed Root Keys

- `schema`
- `building_type`
- `style`
- `mass`
- `program`

## Allowed `style` Keys

- `category`
- `facade`
- `roof`
- `window_style`
- `material`

## Allowed `mass` Keys

- `footprint_area`
- `floors`
- `total_height`

`footprint_area` means per-floor footprint area.

## Allowed Space Keys

- `space_id`
- `type`
- `area_min`
- `area_max`
- `area_preferred`
- `priority`
- `adjacency`
- `constraints`

## Allowed `adjacency` Keys

- `to`
- `relationship`
- `importance`

Prefer:

- `relationship`: `connected`, `nearby`
- `importance`: `required`, `preferred`

Avoid depending on `separated`, `visual`, `optional`.

## Allowed `constraints` Keys

- `aspect_ratio_max`
- `natural_light`
- `exterior_access`
- `ceiling_height`
- `min_width`
- `connects_to_floor`
- `connects_from_floor`

## Current Resolver Support

Actually useful today:

- `area_min`, `area_max`, `area_preferred`
- `priority`
- `adjacency` as a placement hint
- `aspect_ratio_max`
- `natural_light`
- `exterior_access`
- `min_width`
- `ceiling_height`
- `connects_from_floor`

## Recommended Pattern

- Ground floor: `entrance`, `living`, `dining`, `kitchen`, `corridor`, `stairs`, optional guest bathroom or storage
- Upper floor: `bedroom`, `bathroom`, `corridor`, `stairs`
- Keep corridor and stair area conservative
- Use only snake_case
- Do not invent fields
- Do not add comments

## Minimal Shape

```json
{
  "schema": "moon_building_v11",
  "building_type": "residential_house",
  "style": {
    "category": "modern",
    "facade": "concrete",
    "roof": "flat",
    "window_style": "full_height",
    "material": "concrete_white"
  },
  "mass": {
    "footprint_area": 120,
    "floors": 2
  },
  "program": {
    "floors": []
  }
}
```