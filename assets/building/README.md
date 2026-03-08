# Moon Engine AI Procedural Building System

### Unified Architecture & Semantic Layout Specification (V12)

This document merges and updates the previous system documents into a
**single specification** for the Moon Engine AI‑driven procedural
building generator.

The goal of this system:

AI generates **one structured JSON blueprint**, and the engine converts
it into a **fully rendered building**.

Users may also **edit the JSON manually** to refine the architecture.

------------------------------------------------------------------------

# 1. System Goal

Pipeline:

User Prompt\
↓\
AI JSON Generation\
↓\
Schema Validator\
↓\
Semantic Layout Resolver\
↓\
Procedural Generators\
↓\
Mesh Builder\
↓\
Renderer

Key rule:

**JSON is the single source of truth.**

The engine never edits geometry manually.

------------------------------------------------------------------------

# 2. Core Design Principles

## 2.1 AI Generates Architecture, Not Geometry

AI outputs:

-   building structure
-   room types
-   relationships
-   constraints

AI must NOT generate:

-   vertices
-   triangles
-   meshes
-   normals
-   UVs

Geometry is generated procedurally by the engine.

------------------------------------------------------------------------

## 2.2 Grid System

All coordinates align to a grid.

Grid size:

0.5 meters

Valid coordinates:

0\
0.5\
1\
1.5\
2

Benefits:

-   stable AI generation
-   avoids floating point issues
-   simplifies geometry generation

------------------------------------------------------------------------

# 3. Building Structure Model

Real architecture is hierarchical.

The system represents buildings using **four structural concepts**:

core\
circulation\
unit\
void

Structure:

building\
floor\
core\
circulation\
unit\
rooms\
void

This structure supports nearly all real-world architecture types.

------------------------------------------------------------------------

# 4. Structural Concepts

## 4.1 Core

Vertical service structure for tall buildings.

Examples:

-   elevators
-   stair shafts
-   mechanical rooms
-   service shafts

Typical placement:

center of building.

Example:

``` json
{
 "space_id": "core",
 "type": "core",
 "area_preferred": 120
}
```

------------------------------------------------------------------------

## 4.2 Circulation

Movement spaces connecting rooms.

Examples:

-   corridor
-   lobby
-   entrance
-   hallway
-   ring corridor (mall)

Resolver places circulation spaces early.

------------------------------------------------------------------------

## 4.3 Unit

A logical grouping of spaces.

Examples:

-   apartment
-   office suite
-   shop
-   hotel room

Example:

``` json
{
 "space_id": "living",
 "unit_id": "apartment_A",
 "type": "living"
}
```

Resolver rule:

spaces with the same unit_id cluster together.

------------------------------------------------------------------------

## 4.4 Void

Empty architectural space.

Examples:

-   shopping mall atrium
-   courtyard
-   lightwell
-   lobby void

Example:

``` json
{
 "space_id": "atrium",
 "type": "void",
 "area_preferred": 400
}
```

Void areas cannot be occupied by other spaces.

------------------------------------------------------------------------

# 5. Zoning System

Spaces can declare functional zones.

``` json
"zone": "public | private | service | circulation"
```

Resolver placement order:

circulation → public → private → service

This improves layout realism.

------------------------------------------------------------------------

# 6. Root JSON Schema

``` json
{
 "schema": "moon_building_v12",
 "grid": 0.5,
 "building_type": "apartment | villa | office | mall",
 "style": {},
 "mass": {},
 "program": {}
}
```

------------------------------------------------------------------------

# 7. Mass System

Mass defines building volumes.

Example:

``` json
"mass":{
 "footprint_area":1200,
 "floors":6
}
```

Footprint area represents **per floor area**.

Aspect ratio depends on building type.

Default ratios:

villa → 1.2\
apartment → 1.8\
office → 2.2\
mall → 3.0

------------------------------------------------------------------------

# 8. Program Layout

``` json
"program":{
 "floors":[
  {
   "level":0,
   "spaces":[]
  }
 ]
}
```

Each floor contains semantic spaces.

------------------------------------------------------------------------

# 9. Space Definition

``` json
{
 "space_id":"living_1",
 "unit_id":"apt_a",
 "type":"living",
 "area_preferred":30,
 "zone":"public"
}
```

Fields:

space_id --- unique identifier\
unit_id --- logical grouping\
type --- space function\
area_preferred --- preferred size\
zone --- functional classification

------------------------------------------------------------------------

# 10. Adjacency System

Adjacency expresses spatial relationships.

Example:

``` json
{
 "to":"kitchen",
 "relationship":"connected",
 "importance":"required"
}
```

Supported relationships:

connected\
nearby\
facing\
around\
share_wall

------------------------------------------------------------------------

# 11. Default Room Areas

If area is not provided the resolver uses defaults.

living --- 30 m²\
dining --- 15 m²\
kitchen --- 12 m²\
bedroom --- 15 m²\
bathroom --- 5 m²\
corridor --- 8 m²\
stairs --- 6 m²\
office --- 12 m²\
shop --- 80 m²

Unknown types default to 10 m².

------------------------------------------------------------------------

# 12. Layout Resolver Algorithm

Recommended layout order:

1.  calculate footprint
2.  place core
3.  place void spaces
4.  place circulation
5.  divide unit blocks
6.  place rooms inside units

This order matches real architectural planning.

------------------------------------------------------------------------

# 13. Supported Building Types

This architecture supports:

Residential

-   villas
-   apartments
-   townhouses

Commercial

-   shopping malls
-   supermarkets
-   retail centers

Office

-   office towers
-   CBD buildings
-   co‑working spaces

Public

-   schools
-   hospitals
-   hotels

------------------------------------------------------------------------

# 14. Example Apartment Building

``` json
{
 "schema":"moon_building_v12",
 "grid":0.5,
 "building_type":"apartment",

 "mass":{
  "footprint_area":900,
  "floors":6
 },

 "program":{
  "floors":[{
   "level":0,
   "spaces":[
    {"space_id":"core","type":"core","area_preferred":120},

    {"space_id":"corridor","type":"corridor","area_preferred":80},

    {"space_id":"living_a","unit_id":"apt_a","type":"living"},
    {"space_id":"bedroom_a","unit_id":"apt_a","type":"bedroom"},
    {"space_id":"bathroom_a","unit_id":"apt_a","type":"bathroom"},

    {"space_id":"living_b","unit_id":"apt_b","type":"living"},
    {"space_id":"bedroom_b","unit_id":"apt_b","type":"bedroom"},
    {"space_id":"bathroom_b","unit_id":"apt_b","type":"bathroom"}
   ]
  }]
 }
}
```

------------------------------------------------------------------------

# 15. Example Shopping Mall

``` json
{
 "schema":"moon_building_v12",
 "grid":0.5,
 "building_type":"mall",

 "mass":{
  "footprint_area":3000,
  "floors":2
 },

 "program":{
  "floors":[{
   "level":0,
   "spaces":[
    {"space_id":"atrium","type":"void","area_preferred":600},
    {"space_id":"ring_corridor","type":"corridor","area_preferred":300},

    {"space_id":"shop1","unit_id":"shop1","type":"shop"},
    {"space_id":"shop2","unit_id":"shop2","type":"shop"},
    {"space_id":"shop3","unit_id":"shop3","type":"shop"}
   ]
  }]
 }
}
```

------------------------------------------------------------------------

# 16. AI Generation Rules

AI must:

-   output valid JSON only
-   use allowed fields
-   align coordinates to grid
-   generate realistic layouts

AI must NOT:

-   create meshes
-   create geometry
-   invent schema fields

------------------------------------------------------------------------

# 17. Final System Pipeline

AI Prompt\
↓\
AI JSON Blueprint\
↓\
Schema Validation\
↓\
Semantic Layout Resolver\
↓\
Procedural Generators\
↓\
Mesh Builder\
↓\
Renderer

The JSON blueprint remains editable by both **AI and users**.
