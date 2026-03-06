# Building System V8 JSON Schema Specification

**Version:** 1.0  
**Last Updated:** 2026-03-06  
**Schema ID:** `moon_building_v8`

---

## ⚠️ CRITICAL RULES

1. **ALL field names MUST use snake_case** (e.g., `mass_id`, `usage_hint`)
2. **ONLY the fields listed below are allowed** - any other field will cause validation error
3. **Enums MUST match exactly** - case-sensitive, no variations allowed
4. **All coordinates MUST be multiples of 0.5 meters** (grid alignment requirement)

---

## Top-Level Structure

```json
{
  "schema": "moon_building_v8",
  "grid": 0.5,
  "style": { ... },
  "masses": [ ... ],
  "floors": [ ... ]
}
```

### Root Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `schema` | string | ✅ | MUST be `"moon_building_v8"` |
| `grid` | number | ✅ | Grid size in meters (typically `0.5`) |
| `style` | object | ✅ | Building style configuration |
| `masses` | array | ✅ | List of building masses (volumes) |
| `floors` | array | ✅ | List of floor definitions |

---

## Style Object

**Allowed Fields:**

| Field | Type | Required | Allowed Values |
|-------|------|----------|----------------|
| `category` | string | ✅ | `"modern"`, `"classical"`, `"industrial"`, `"minimalist"`, `"traditional"` |
| `facade` | string | ✅ | `"glass_white"`, `"brick"`, `"concrete"`, `"metal"`, `"wood"` |
| `roof` | string | ✅ | `"flat"`, `"pitched"`, `"gable"`, `"hip"`, `"mansard"` |
| `window_style` | string | ✅ | `"full_height"`, `"standard"`, `"small"`, `"ribbon"`, `"punched"` |
| `material` | string | ✅ | `"concrete_white"`, `"brick_red"`, `"glass"`, `"metal_grey"`, `"wood_natural"` |

**Example:**
```json
{
  "category": "modern",
  "facade": "glass_white",
  "roof": "flat",
  "window_style": "full_height",
  "material": "concrete_white"
}
```

---

## Mass Object

**Allowed Fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `mass_id` | string | ✅ | Unique identifier (e.g., `"main_house"`) |
| `origin` | [number, number] | ✅ | Bottom-left corner `[X, Y]` in meters, grid-aligned |
| `size` | [number, number] | ✅ | Dimensions `[width, depth]` in meters, grid-aligned |
| `floors` | integer | ✅ | Number of floors in this mass (e.g., `2`) |

**Example:**
```json
{
  "mass_id": "main_house",
  "origin": [0.0, 0.0],
  "size": [10.0, 8.0],
  "floors": 2
}
```

---

## Floor Object

**Allowed Fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `level` | integer | ✅ | Floor level (0 = ground floor, 1 = first floor, etc.) |
| `mass_id` | string | ✅ | Which mass this floor belongs to |
| `floor_height` | number | ✅ | Floor-to-floor height in meters (typically `3.0`) |
| `spaces` | array | ✅ | List of spaces (rooms) on this floor |

**Example:**
```json
{
  "level": 0,
  "mass_id": "main_house",
  "floor_height": 3.0,
  "spaces": [ ... ]
}
```

---

## Space Object

**Allowed Fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `space_id` | integer | ✅ | Unique numeric identifier |
| `rects` | array | ✅ | List of rectangles defining the space geometry |
| `properties` | object | ✅ | Space properties |
| `anchors` | array | ❌ | Optional placement hints for furniture/objects |

**Example:**
```json
{
  "space_id": 1,
  "rects": [ ... ],
  "properties": { ... },
  "anchors": [ ... ]
}
```

---

## Space Properties Object

**Allowed Fields:**

| Field | Type | Required | Allowed Values |
|-------|------|----------|----------------|
| `usage_hint` | string | ✅ | `"living"`, `"bedroom"`, `"kitchen"`, `"bathroom"`, `"corridor"`, `"storage"`, `"office"`, `"dining"`, `"entrance"`, `"balcony"`, `"terrace"` |
| `is_outdoor` | boolean | ✅ | `true` or `false` |
| `has_stairs` | boolean | ✅ | `true` or `false` (does this space contain stairs?) |
| `ceiling_height` | number | ✅ | Ceiling height in meters (typically `2.8` or `3.0`) |

**Example:**
```json
{
  "usage_hint": "living",
  "is_outdoor": false,
  "has_stairs": false,
  "ceiling_height": 2.8
}
```

---

## Rect Object

**Allowed Fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `rect_id` | string | ✅ | Unique identifier within the space |
| `origin` | [number, number] | ✅ | Bottom-left corner `[X, Y]` in meters, grid-aligned |
| `size` | [number, number] | ✅ | Dimensions `[width, depth]` in meters, grid-aligned |

**Example:**
```json
{
  "rect_id": "living_main",
  "origin": [0.0, 0.0],
  "size": [6.0, 5.0]
}
```

---

## Anchor Object (Optional)

**Allowed Fields:**

| Field | Type | Required | Allowed Values / Description |
|-------|------|----------|------------------------------|
| `name` | string | ✅ | Descriptive name (e.g., `"sofa_zone"`) |
| `position` | [number, number] | ✅ | Position `[X, Y]` in meters within space |
| `type` | string | ✅ | `"furniture_zone"`, `"sofa_zone"`, `"bed_zone"`, `"door_hint"`, `"stair_hint"`, `"window_hint"` |
| `rotation` | number | ❌ | Rotation in degrees (default: `0.0`) |
| `metadata` | string | ❌ | Additional JSON metadata |

**Example:**
```json
{
  "name": "sofa_zone",
  "position": [3.0, 2.5],
  "type": "sofa_zone",
  "rotation": 90.0
}
```

---

## Complete Example

```json
{
  "schema": "moon_building_v8",
  "grid": 0.5,
  "style": {
    "category": "modern",
    "facade": "glass_white",
    "roof": "flat",
    "window_style": "full_height",
    "material": "concrete_white"
  },
  "masses": [
    {
      "mass_id": "main_house",
      "origin": [0.0, 0.0],
      "size": [10.0, 8.0],
      "floors": 2
    }
  ],
  "floors": [
    {
      "level": 0,
      "mass_id": "main_house",
      "floor_height": 3.0,
      "spaces": [
        {
          "space_id": 1,
          "rects": [
            {
              "rect_id": "living_main",
              "origin": [0.0, 0.0],
              "size": [6.0, 5.0]
            }
          ],
          "properties": {
            "usage_hint": "living",
            "is_outdoor": false,
            "has_stairs": false,
            "ceiling_height": 2.8
          },
          "anchors": [
            {
              "name": "sofa_zone",
              "position": [3.0, 2.5],
              "type": "sofa_zone"
            }
          ]
        },
        {
          "space_id": 2,
          "rects": [
            {
              "rect_id": "kitchen_main",
              "origin": [6.0, 0.0],
              "size": [4.0, 5.0]
            }
          ],
          "properties": {
            "usage_hint": "kitchen",
            "is_outdoor": false,
            "has_stairs": false,
            "ceiling_height": 2.8
          }
        }
      ]
    },
    {
      "level": 1,
      "mass_id": "main_house",
      "floor_height": 3.0,
      "spaces": [
        {
          "space_id": 3,
          "rects": [
            {
              "rect_id": "bedroom_main",
              "origin": [0.0, 0.0],
              "size": [5.0, 5.0]
            }
          ],
          "properties": {
            "usage_hint": "bedroom",
            "is_outdoor": false,
            "has_stairs": false,
            "ceiling_height": 2.8
          }
        }
      ]
    }
  ]
}
```

---

## Validation Rules

### Grid Alignment
- All `origin` and `size` coordinates MUST be multiples of `grid` value (typically 0.5)
- Invalid: `[1.3, 2.7]` 
- Valid: `[1.0, 2.5]`, `[0.0, 3.0]`

### Space IDs
- Must be unique across all floors
- Must be positive integers

### Mass References
- Every `floor.mass_id` MUST reference an existing `mass.mass_id`

### Floor Levels
- Must start from 0 (ground floor)
- Must be consecutive integers

### Spatial Constraints
- Rectangles within the same space MUST NOT overlap
- Rectangles MUST fit within the parent mass boundaries

---

## Common Errors

| Error | Cause | Fix |
|-------|-------|-----|
| `Missing required field: schema` | Missing or wrong field name | Use `"schema": "moon_building_v8"` |
| `Missing required style field: window_style` | Using camelCase | Use `window_style` not `windowStyle` |
| `Mass is missing required fields` | Wrong field names | Use `mass_id`, `origin`, `size`, `floors` |
| `Coordinate not grid-aligned` | Using non-multiple of 0.5 | Round to nearest 0.5: `2.7` → `2.5` or `3.0` |
| `Duplicate space_id` | Same ID used twice | Ensure all `space_id` values are unique |

---

## For AI JSON Generators

**CRITICAL INSTRUCTIONS:**

1. ✅ **ONLY use field names from this document** - do NOT invent new fields
2. ✅ **ONLY use enum values from this document** - do NOT create variations
3. ✅ **ALL field names MUST be snake_case** - NEVER use camelCase
4. ✅ **ALL coordinates MUST be multiples of 0.5**
5. ✅ Check all required fields are present
6. ✅ Validate all cross-references (mass_id, space_id)

**Before generating JSON:**
- Review the allowed field names table
- Review the allowed enum values
- Verify grid alignment for all coordinates
- Check that all required fields are present

---

## Schema Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-03-06 | Initial specification with strict field validation |
