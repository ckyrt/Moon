# Building AI Context

Use this as a short prompt when generating `moon_building` JSON.

## Output Rules

- output valid JSON only
- no comments, no prose, no geometry, no meshes, no CSG
- use `schema: moon_building`
- use `grid: 0.5`
- use only supported `building_type` values:
  `villa`, `apartment`, `office`, `office_tower`, `cbd_residential`, `mall`, `shopping_center`, `retail_center`

## Root Shape

```json
{
  "schema": "moon_building",
  "grid": 0.5,
  "building_type": "apartment",
  "style": {
    "category": "modern",
    "facade": "concrete",
    "roof": "flat",
    "window_style": "full_height",
    "material": "concrete_white"
  },
  "mass": {
    "footprint_area": 900,
    "floors": 6,
    "total_height": 21
  },
  "program": {
    "floors": []
  }
}
```

## Allowed Space Keys

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

## Key Structure

Think in this order:

1. `core`
2. `void`
3. `circulation`
4. `unit_id` groups
5. rooms inside each unit

Use these common types when useful:

- `core`, `void`, `corridor`, `entrance`, `lobby`, `mechanical`
- `living`, `dining`, `kitchen`, `bedroom`, `bathroom`, `office`, `meeting_room`, `shop`, `storage`, `laundry`, `garage`, `balcony`, `terrace`

Use these zones only:

- `public`
- `private`
- `service`
- `circulation`

## Typology Short Rules

- `villa`: public ground floor, private upper floor, small stair/core block
- `apartment`: shared `core` + shared `corridor` + multiple residential `unit_id`
- `office_tower`: lower podium floors for lobby / meeting / support, upper tower floors for office program, `core` on every floor
- `cbd_residential`: shared `core` + shared `corridor` + multiple residential `unit_id`; for taller towers add one non-ground amenity floor
- `mall` / `shopping_center` / `retail_center`: include a `void`, put `corridor` around it, connect every `shop` to circulation

## Adjacency Hints

- use `connected`, `nearby`, `share_wall`, `around`, `facing`
- malls: corridor should use `around` to reference the `void`
- shops should connect to `corridor` or `lobby`

## Final Reminder

- generate architecture, not geometry
- keep layouts realistic and validator-friendly
- if unsure, prefer simple repeated floors with clear `core`, `corridor`, and `unit_id` structure