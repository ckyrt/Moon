# Moon Engine --- AI Procedural Building System

### Optimized Architecture Specification (V8)

This document defines the **improved architecture**, **JSON schema**,
**AI generation rules**, and **example JSON** used by the Moon Engine
procedural building system.

The goal:

AI generates **one structured JSON document**, and the **engine converts
it into a fully rendered building**.

Users can then **edit the JSON manually** to adjust the final result.

------------------------------------------------------------------------

# 1. System Goal

Pipeline:

User Prompt\
↓\
AI JSON Generation\
↓\
JSON Validator\
↓\
Space Graph Builder\
↓\
Procedural Generators\
↓\
Mesh Builder\
↓\
Renderer

Key rule:

**JSON is the single source of truth.**

The engine never edits geometry directly.

------------------------------------------------------------------------

# 2. Core Design Principles

## 2.1 AI Generates Structure Only

AI must generate **architectural layout**, not geometry.

AI must NOT generate:

-   vertices
-   meshes
-   normals
-   UVs
-   triangles

Only high‑level architectural data.

------------------------------------------------------------------------

## 2.2 Grid System

All coordinates must align to a grid.

Grid size:

    0.5 meters

Valid coordinates:

    0
    0.5
    1
    1.5
    2
    ...

Invalid examples:

    0.37
    1.28

Benefits:

-   improves AI reliability
-   avoids floating precision issues
-   simplifies geometry generation

------------------------------------------------------------------------

# 3. Generation Pipeline

AI Prompt\
↓\
AI JSON Generation\
↓\
Schema Validator\
↓\
Mass Processor\
↓\
Floor Generator\
↓\
Space Graph Builder\
↓\
Wall Generator\
↓\
Door Generator\
↓\
Stair Generator\
↓\
Facade Generator\
↓\
Mesh Builder\
↓\
Renderer

------------------------------------------------------------------------

# 4. Root JSON Schema

``` json
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

  "masses": [],
  "floors": []
}
```

Fields:

  Field    Description
  -------- --------------------
  schema   schema version
  grid     grid size
  style    architecture style
  masses   building volumes
  floors   floor definitions

------------------------------------------------------------------------

# 5. Mass System

Mass defines **building volumes**.

``` json
"masses":[
  {
    "mass_id":"main",
    "origin":[0,0],
    "size":[12,10],
    "floors":2
  }
]
```

Supports:

-   wings
-   terraces
-   L‑shapes
-   villas
-   complex buildings

------------------------------------------------------------------------

# 6. Floor Definition

Each floor belongs to a specific mass.

``` json
{
  "level":0,
  "mass_id":"main",
  "floor_height":3.2,
  "spaces":[]
}
```

Typical heights:

  Type          Height
  ------------- --------
  Residential   3.0m
  Villa         3.2m
  Commercial    4.5m

------------------------------------------------------------------------

# 7. Space Definition

Rooms are defined using rectangles.

``` json
{
 "space_id":1,

 "rects":[
   {
     "rect_id":"r1",
     "origin":[0,0],
     "size":[6,4]
   }
 ],

 "properties":{
   "usage_hint":"living",
   "is_outdoor":false,
   "stairs":false,
   "ceiling_height":3.2
 },

 "anchors":[]
}
```

## Valid usage_hint values (MUST use exactly these):

**Required rooms:**
- `living` - Living room / lounge
- `dining` - Dining room  
- `kitchen` - Kitchen
- `bedroom` - Bedroom
- `bathroom` - Bathroom / toilet

**Circulation:**
- `corridor` - Hallway / corridor
- `entrance` - Entrance hall / foyer

**Storage:**
- `closet` - Closet / wardrobe / walk-in closet
- `storage` - Storage room / utility room
- `laundry` - Laundry room

**Work & Special:**
- `office` - Office / study

**Outdoor:**
- `balcony` - Balcony (must set is_outdoor: true)
- `terrace` - Terrace (must set is_outdoor: true)

**Vehicle:**
- `garage` - Garage / parking
- `stairwell` - Stairwell space

**IMPORTANT:** AI must use these exact lowercase values. Do not invent new values.

------------------------------------------------------------------------

# 8. Stair System

Stairs connect floors.

``` json
"stairs":{
  "type":"L",
  "connect_to_level":1
}
```

Supported:

-   straight
-   L
-   U
-   spiral

Engine calculates:

-   step height
-   number of steps
-   landing
-   railing

------------------------------------------------------------------------

# 9. Layout Validator

The engine validates AI output.

Checks include:

-   grid alignment
-   rectangle overlap
-   minimum room size
-   boundary violations
-   floor connectivity

Repair actions:

-   shrink room
-   reposition rectangles
-   merge small rooms

------------------------------------------------------------------------

# 10. Space Graph

The engine constructs a **connectivity graph**.

Example:

    Living ↔ Corridor
    Bedroom ↔ Corridor
    Corridor ↔ Stair

Used for:

-   door placement
-   navigation
-   gameplay logic

------------------------------------------------------------------------

# 11. Wall Generation

Rules:

Same space_id → no wall

Different space_id → interior wall

No neighbour → exterior wall

Open edge → railing

------------------------------------------------------------------------

# 12. Door Generation

Doors are placed between connected spaces.

Algorithm:

1.  detect shared edge\
2.  edge length ≥ 1.2m\
3.  check door_hint anchors\
4.  place door

Door types:

-   interior
-   sliding
-   glass
-   balcony

------------------------------------------------------------------------

# 13. Facade Generator

Exterior walls become architectural elements.

Pipeline:

Exterior walls\
↓\
Facade grid\
↓\
Window placement\
↓\
Balcony placement\
↓\
Columns / panels\
↓\
Mesh generation

Facade elements:

-   windows
-   balconies
-   columns
-   railings
-   glass panels

------------------------------------------------------------------------

# 14. Anchor System

Anchors allow procedural placement of objects.

Example:

``` json
{
 "name":"sofa_zone",
 "pos":[3,2],
 "type":"furniture_zone"
}
```

Anchor types:

-   furniture_zone
-   sofa_zone
-   bed_zone
-   door_hint
-   stair_hint

------------------------------------------------------------------------

# 15. Example JSON

``` json
{
 "schema": "moon_building_v8",
 "grid": 0.5,

 "style":{
   "category":"modern",
   "facade":"glass_white",
   "roof":"flat",
   "window_style":"full_height",
   "material":"concrete_white"
 },

 "masses":[
   {"mass_id":"main","origin":[0,0],"size":[12,10],"floors":2}
 ],

 "floors":[
  {
   "level":0,
   "mass_id":"main",
   "floor_height":3.2,

   "spaces":[
    {
      "space_id":1,
      "rects":[{"rect_id":"r1","origin":[0,0],"size":[6,4]}],
      "properties":{"usage_hint":"living","is_outdoor":false,"stairs":false,"ceiling_height":3.2},
      "anchors":[{"name":"sofa_zone","pos":[3,2],"type":"furniture_zone"}]
    }
   ]
  }
 ]
}
```

------------------------------------------------------------------------

# 16. AI Generation Rules

When prompting AI:

Allowed top-level keys:

-   schema
-   grid
-   style
-   masses
-   floors

AI must NOT generate additional fields.

AI must output **valid JSON only**.

------------------------------------------------------------------------

# 17. AI Prompt Template

Example prompt:

    Generate a building JSON using the Moon Engine building schema.

    Rules:

    - Output valid JSON only
    - Do not include explanations
    - Only use the keys: schema, grid, style, masses, floors
    - All coordinates must align to 0.5m grid
    - Rooms must not overlap
    - Buildings must represent realistic architecture
    - Do not generate meshes or geometry

Example request:

    Create a modern two‑floor villa with terrace and large living room.

------------------------------------------------------------------------

# 18. Capabilities

This system supports:

-   villas
-   luxury houses
-   apartments
-   commercial buildings

With PBR rendering it can achieve:

-   archviz style buildings
-   houseporn‑level visuals
-   Unreal‑quality environments

------------------------------------------------------------------------

# Final Architecture

AI Prompt\
↓\
AI JSON Generation\
↓\
Validator\
↓\
Procedural Generators\
↓\
Mesh Builder\
↓\
Renderer

JSON remains the **editable blueprint** for both AI and users.
