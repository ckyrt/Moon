
# Building System V10 JSON Schema Specification
Version: 1.0
Schema ID: moon_building_v10
Purpose: AI‑friendly procedural building generation with strong validation and flexible design.

This document serves TWO purposes:
1. JSON schema specification
2. AI generation guide

AI systems must follow this document when generating building layouts.

---

# Design Goals

The schema is designed so that:

• AI can generate rich and diverse buildings  
• Generated layouts remain geometrically valid  
• Validation rules prevent impossible structures  
• Procedural systems can reliably construct walls, doors, and windows  

---

# CRITICAL GLOBAL RULES

1. ALL field names MUST use snake_case
2. ONLY fields defined in this document are allowed
3. Enum values are case‑sensitive
4. All coordinates MUST align to the grid
5. Spaces MUST fully cover the building mass
6. Geometry MUST NOT overlap
7. Indoor spaces MUST connect to at least one other indoor space

---

# Top Level Structure

```json
{
  "schema": "moon_building_v10",
  "grid": 0.5,
  "style": {...},
  "masses": [...],
  "floors": [...]
}
```

---

# Root Fields

| Field | Type | Required | Description |
|------|------|------|------|
schema | string | yes | must equal moon_building_v10 |
grid | number | yes | grid alignment size |
style | object | yes | building style |
masses | array | yes | building volumes |
floors | array | yes | building floors |

---

# Style Object

Allowed Fields

| Field | Allowed Values |
|------|------|
category | modern, classical, industrial, minimalist, traditional |
facade | glass_white, brick, concrete, metal, wood |
roof | flat, pitched, gable, hip, mansard |
window_style | full_height, standard, small, ribbon, punched |
material | concrete_white, brick_red, glass, metal_grey, wood_natural |

Example

```json
{
 "category":"modern",
 "facade":"glass_white",
 "roof":"flat",
 "window_style":"full_height",
 "material":"concrete_white"
}
```

---

# Mass Object

| Field | Type | Required |
|------|------|------|
mass_id | string | yes |
origin | [number,number] | yes |
size | [number,number] | yes |
floors | integer | yes |

Rules

• origin must align to grid  
• size must align to grid  
• floors ≥ 1  

Example

```json
{
 "mass_id":"main_house",
 "origin":[0,0],
 "size":[16,12],
 "floors":2
}
```

---

# Floor Object

| Field | Type | Required |
|------|------|------|
level | integer | yes |
mass_id | string | yes |
floor_height | number | yes |
spaces | array | yes |

Rules

• level must start at 0  
• level < mass.floors

---

# Space Object

| Field | Type | Required |
|------|------|------|
space_id | integer | yes |
rects | array | yes |
properties | object | yes |
anchors | array | optional |

Rules

• space_id must be unique  
• rect count ≤ 4  
• space geometry must stay inside mass

---

# Space Properties

| Field | Allowed Values |
|------|------|
usage_hint | living, bedroom, kitchen, bathroom, corridor, storage, office, dining, entrance, balcony, terrace |
is_outdoor | true / false |
has_stairs | true / false |
ceiling_height | number |

Rules

• balcony and terrace MUST have is_outdoor = true  
• minimum indoor space size = 2m x 2m

Example

```json
{
 "usage_hint":"living",
 "is_outdoor":false,
 "has_stairs":false,
 "ceiling_height":2.8
}
```

---

# Rect Object

| Field | Type |
|------|------|
rect_id | string |
origin | [number,number] |
size | [number,number] |

Rules

• must align to grid  
• must stay inside mass  
• must not overlap other rectangles

Example

```json
{
 "rect_id":"living_main",
 "origin":[0,0],
 "size":[6,5]
}
```

---

# Anchor Object

Optional placement hints

| Field | Type |
|------|------|
name | string |
position | [number,number] |
type | string |
rotation | number |
metadata | string |

Allowed anchor types

furniture_zone  
sofa_zone  
bed_zone  
door_hint  
stair_hint  
window_hint

Rules

• anchors must be inside the space geometry  
• if has_stairs = true, stair_hint anchor required

Example

```json
{
 "name":"stairs",
 "position":[4,3],
 "type":"stair_hint"
}
```

---

# Layout Validation Rules

The following MUST pass validation

1 Mass must be fully covered by spaces
2 Spaces must not overlap
3 Rectangles must not overlap
4 Coordinates must align to grid
5 Indoor spaces must share a wall with another indoor space
6 Anchors must be inside space geometry
7 Floors must match mass height

---

# AI Generation Protocol

This section instructs AI systems how to produce valid building layouts.

AI should follow the steps below.

---

## Step 1 – Choose Style

Select values from the style enum list.

Example

modern villa  
minimalist house  
industrial loft

---

## Step 2 – Define Mass

Choose a building footprint.

Typical sizes

small house
8x8 – 12x10

villa
12x10 – 20x16

mansion
20x16 – 40x30

---

## Step 3 – Define Floors

Common patterns

2 floor house  
3 floor townhouse  
1 floor bungalow

---

## Step 4 – Create Spaces

Divide the mass into spaces such as

living  
kitchen  
dining  
bedroom  
bathroom  
corridor  

Ensure spaces fully cover the mass.

---

## Step 5 – Add Anchors

Optional hints for layout.

Examples

sofa_zone  
bed_zone  
stair_hint

---

# Recommended Room Sizes

living
5m – 10m

bedroom
3m – 5m

bathroom
2m – 3m

corridor
1.2m – 2m

kitchen
3m – 6m

---

# Outdoor Spaces

balcony or terrace must have

is_outdoor = true

---

# Common Errors

| Error | Cause |
|------|------|
invalid enum | value not in allowed list |
grid violation | coordinate not multiple of grid |
rect overlap | geometry collision |
space outside mass | invalid layout |
missing stair anchor | has_stairs true but no anchor |

---

# Example AI Request

Example prompt

Generate a modern two‑story villa using the moon_building_v10 schema.
Include:
• living room
• kitchen
• dining
• 3 bedrooms
• 2 bathrooms
• corridor
• balcony

The AI must output valid JSON following the schema.

---

# Version History

| Version | Notes |
|------|------|
v10 | AI‑optimized schema with generation protocol |
