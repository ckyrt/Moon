# Building Assets

This folder contains building test data for two layers:

- `moon_building_v10`: geometric input the engine consumes directly
- `moon_building_v11`: semantic input that `LayoutResolver` converts to V10 first

Use this README as the human-facing summary.
Use `assets/building/building-v11-context.md` as the single source of truth for AI prompt context.

## Directory Layout

```text
assets/building/
    basic/            simple V10 cases
    complex/          larger V10 layouts
    residential/      V10 residential examples
    test_building.json legacy V10 test input
    simple_apartment_v11.json minimal V11 example
    test_house_v11.json main V11 house example
```

## V10 Geometric Format

Use V10 when you want exact rectangles.

- Root keys: `schema`, `grid`, `style`, `masses`, `floors`
- Grid is `0.5`
- `masses[*]` defines footprint origin, size, floor count
- `floors[*].spaces[*].rects[*]` defines exact geometry
- Geometry must stay inside the mass and align to grid
- Indoor spaces should connect sensibly; overlapping rects are invalid

Core style values used by current assets:

- `category`: `modern`, `classical`, `industrial`, `minimalist`, `traditional`
- `facade`: `glass_white`, `brick`, `concrete`, `metal`, `wood`
- `roof`: `flat`, `pitched`, `gable`, `hip`, `mansard`
- `window_style`: `full_height`, `standard`, `small`, `ribbon`, `punched`
- `material`: `concrete_white`, `brick_red`, `glass`, `metal_grey`, `wood_natural`

## V11 Semantic Format

Use V11 when you want to describe rooms and relationships, not coordinates.

Allowed root keys:

- `schema`
- `building_type`
- `style`
- `mass`
- `program`

Allowed `mass` keys:

- `footprint_area`
- `floors`
- `total_height`

Important:

- `footprint_area` means per-floor footprint area, not total building area
- Unknown keys should not be added
- Snake_case only

Allowed space keys:

- `space_id`
- `type`
- `area_min`
- `area_max`
- `area_preferred`
- `priority`
- `adjacency`
- `constraints`

Allowed adjacency keys:

- `to`
- `relationship`
- `importance`

Preferred values:

- `relationship`: `connected`, `nearby`
- `importance`: `required`, `preferred`

Avoid depending on `separated`, `visual`, or `optional`. They are parsed, but not strongly enforced.

Allowed constraint keys:

- `aspect_ratio_max`
- `natural_light`
- `exterior_access`
- `ceiling_height`
- `min_width`
- `connects_to_floor`
- `connects_from_floor`

## Current Resolver Behavior

What the current `LayoutResolver` actually uses:

- area range and preferred area
- `priority` for placement order
- `adjacency` as a placement hint
- `aspect_ratio_max`
- `natural_light`
- `exterior_access`
- `min_width`
- `ceiling_height`
- `connects_from_floor` for upper stair alignment

Current heuristics are simple and deterministic:

- footprint is derived from `footprint_area` with a residential aspect ratio
- circulation rooms like `corridor`, `stairs`, `entrance` are placed early
- rooms requiring daylight or exterior access are pushed to outer walls
- connected rooms try to place near already placed neighbors
- unresolved detail still falls back to rectangle packing

Do not treat V11 as a full constraint solver. It is a guided layout generator.

## Default Room Areas

If a V11 space omits area values, the resolver falls back to built-in defaults.

| Type | Default | Typical Range |
|------|---------|---------------|
| `living` | 30 m² | 25-40 m² |
| `dining` | 15 m² | 12-20 m² |
| `kitchen` | 12 m² | 8-15 m² |
| `bedroom` | 15 m² | 12-18 m² |
| `bathroom` | 5 m² | 4-8 m² |
| `entrance` | 6 m² | 4-8 m² |
| `corridor` | 8 m² | 5-12 m² |
| `stairs` | 6 m² | 4-8 m² |
| `office` | 12 m² | 10-15 m² |
| `storage` | 4 m² | 3-6 m² |
| `balcony` | 6 m² | 4-10 m² |
| `terrace` | 12 m² | 8-20 m² |

Unknown types fall back to `10 m²`.

## Practical V11 Pattern

Keep inputs sparse and realistic.

- Ground floor: `entrance`, `living`, `dining`, `kitchen`, `corridor`, `stairs`, optional guest bathroom or storage
- Upper floor: `bedroom`, `bathroom`, `corridor`, `stairs`
- Use `natural_light: "required"` or `exterior_access: true` only for rooms that really need an exterior wall
- Keep corridor and stair area conservative
- Declare only adjacencies that matter

Minimal V11 shape:

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

## Example Files

- `simple_apartment_v11.json`: smallest semantic example, good for parser and default-area testing
- `test_house_v11.json`: current main V11 house sample for resolver iteration
- `test_building.json`: legacy V10 compatibility sample
- `basic/`, `residential/`, `complex/`: direct V10 reference assets

## Runtime Flow

Current sample flow in the engine:

1. Load a V11 file if present
2. Parse semantic rooms and constraints
3. Run `LayoutResolver`
4. Serialize resolved layout to V10 JSON
5. Continue through `BuildingPipeline` and CSG generation

If V11 parsing or resolution fails, the sample falls back to a V10 asset.
