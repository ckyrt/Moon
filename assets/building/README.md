# Moon Engine AI Procedural Building System

### Unified Architecture & Semantic Layout Specification

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
 "schema": "moon_building",
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

Current validator / resolver support is centered on these types:

Residential

-   villa
-   apartment
-   cbd_residential

Commercial

-   mall
-   shopping_center
-   retail_center

Office

-   office
-   office_tower

Types not listed above should be treated as future extensions, not stable inputs.

------------------------------------------------------------------------

# 13.1 Finer Typology Rules

These are not just style suggestions. They describe the structure the
current validator and resolver are designed to accept well.

## Office Tower: podium + tower

Typical pattern:

-   lower floors = podium
-   upper floors = tower

Recommended program split:

-   ground floor: `lobby`, `core`, `meeting_room`, optional small `shop`
-   lower podium floor(s): shared office, conference, support, amenity
-   upper tower floors: repeated `office` floors around one strong `core`

Current system expectations:

-   ground floor should contain `lobby` or `entrance`
-   every floor should contain a `core`
-   retail should stay on lower podium floors, not upper tower floors

## Mall / Shopping Center: ring corridor

Typical pattern:

-   central `void` = atrium
-   one or more `corridor` spaces around the void
-   `shop` spaces connected to the concourse / ring circulation

Current system expectations:

-   each retail floor should include a `void`
-   circulation around that void should use adjacency with `relationship: around`
-   each `shop` should connect to a circulation space such as `corridor` or `lobby`

## CBD Residential: amenity floor

Typical pattern:

-   ground floor: `lobby` + `core`
-   typical residential floors: `core` + `corridor` + multiple apartment `unit_id` groups
-   one special upper floor: shared amenity floor

Examples of amenity spaces:

-   resident lounge
-   gym
-   coworking
-   kids room
-   sky lounge

Current system expectations:

-   residential floors should include shared `core` and `corridor`
-   multiple residential `unit_id` groups are expected
-   taller `cbd_residential` towers should reserve one non-ground amenity floor instead of making every upper floor a repeated apartment floor

------------------------------------------------------------------------

# 14. Example Apartment Building

``` json
{
 "schema":"moon_building",
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
 "schema":"moon_building",
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
    {
     "space_id":"ring_corridor",
     "type":"corridor",
     "zone":"circulation",
     "area_preferred":300,
     "adjacency":[
      {"to":"atrium","relationship":"around","importance":"required"}
     ]
    },

    {
     "space_id":"shop1",
     "unit_id":"shop1",
     "type":"shop",
     "adjacency":[
      {"to":"ring_corridor","relationship":"connected","importance":"required"}
     ]
    },
    {
     "space_id":"shop2",
     "unit_id":"shop2",
     "type":"shop",
     "adjacency":[
      {"to":"ring_corridor","relationship":"connected","importance":"required"}
     ]
    },
    {
     "space_id":"shop3",
     "unit_id":"shop3",
     "type":"shop",
     "adjacency":[
      {"to":"ring_corridor","relationship":"connected","importance":"required"}
     ]
    }
   ]
  }]
 }
}
```

------------------------------------------------------------------------

# 15.1 Example Office Tower Pattern

``` json
{
 "schema":"moon_building",
 "grid":0.5,
 "building_type":"office_tower",

 "mass":{
  "footprint_area":1200,
  "floors":4
 },

 "program":{
  "floors":[
   {
    "level":0,
    "spaces":[
     {"space_id":"core_gf","type":"core","zone":"service"},
     {"space_id":"main_lobby","type":"lobby","zone":"circulation"},
     {"space_id":"meeting_hub","type":"meeting_room","zone":"public"},
     {"space_id":"cafe","unit_id":"cafe","type":"shop","zone":"public"}
    ]
   },
   {
    "level":1,
    "spaces":[
     {"space_id":"core_l1","type":"core","zone":"service"},
     {"space_id":"shared_office_l1","type":"office","zone":"public"},
     {"space_id":"training_l1","type":"meeting_room","zone":"public"}
    ]
   },
   {
    "level":2,
    "spaces":[
     {"space_id":"core_l2","type":"core","zone":"service"},
     {"space_id":"open_office_l2","type":"office","zone":"public"}
    ]
   },
   {
    "level":3,
    "spaces":[
     {"space_id":"core_l3","type":"core","zone":"service"},
     {"space_id":"open_office_l3","type":"office","zone":"public"}
    ]
   }
  ]
 }
}
```

------------------------------------------------------------------------

# 15.2 Example CBD Residential Amenity Pattern

``` json
{
 "schema":"moon_building",
 "grid":0.5,
 "building_type":"cbd_residential",

 "mass":{
  "footprint_area":900,
  "floors":4
 },

 "program":{
  "floors":[
   {
    "level":0,
    "spaces":[
     {"space_id":"core_gf","type":"core","zone":"service"},
     {"space_id":"lobby_gf","type":"lobby","zone":"circulation"}
    ]
   },
   {
    "level":1,
    "spaces":[
     {"space_id":"core_l1","type":"core","zone":"service"},
     {"space_id":"corridor_l1","type":"corridor","zone":"circulation"},
     {"space_id":"sky_lounge","type":"living","zone":"public"},
     {"space_id":"cowork","type":"office","zone":"public"}
    ]
   },
   {
    "level":2,
    "spaces":[
     {"space_id":"core_l2","type":"core","zone":"service"},
     {"space_id":"corridor_l2","type":"corridor","zone":"circulation"},
     {"space_id":"living_a","unit_id":"apt_a","type":"living","zone":"public"},
     {"space_id":"bedroom_a","unit_id":"apt_a","type":"bedroom","zone":"private"},
     {"space_id":"living_b","unit_id":"apt_b","type":"living","zone":"public"},
     {"space_id":"bedroom_b","unit_id":"apt_b","type":"bedroom","zone":"private"}
    ]
   }
  ]
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

------------------------------------------------------------------------

# 18. Engine Implementation Notes

This section is the engine-side implementation supplement for the same
system. The repository should treat this file as the single maintained
building document.

## 18.1 Runtime Pipeline

Actual runtime flow in the current engine:

Semantic JSON (`moon_building`)\
↓\
`SchemaValidator`\
↓\
`LayoutResolver`\
↓\
post-resolution semantic consistency check\
↓\
`LayoutValidator`\
↓\
`SpaceGraphBuilder`\
↓\
`WallGenerator` / `DoorGenerator` / `StairGenerator` / `FacadeGenerator`\
↓\
`BuildingToCSGConverter`

## 18.2 Internal Geometry Boundary

`BuildingDefinition` is the engine's internal resolved geometry model.

It is used to carry:

- masses
- floors
- spaces
- rects

It is **not** an external semantic schema and must not be repackaged as
`moon_building` input.

## 18.3 Module Responsibilities

`SchemaValidator`

- validates input fields and typology constraints
- invokes `LayoutResolver`
- verifies that required semantic relations still hold after resolve

`LayoutResolver`

- computes footprint from typology and mass intent
- places semantic spaces into concrete rectangles
- preserves reserved geometry such as mall ring circulation
- outputs resolved geometry for later generators

`LayoutValidator`

- checks bounds, overlaps, grid alignment, minimum size, stair links

`SpaceGraphBuilder`

- derives topology from resolved rectangles
- supports door placement and semantic post-checks

`BuildingToCSGConverter`

- converts floor, wall, door, window, and stair output into CSG blueprint JSON
- output can be consumed directly by `BlueprintLoader::ParseFromString()`

## 18.4 Maintenance Rules

When extending the system:

1. Add typology rules in `SchemaValidator` before adding placement logic in `LayoutResolver`.
2. Re-check required adjacency semantics after every layout-resolution change.
3. Keep sample and debug paths on semantic input or resolved geometry APIs only; do not mix them into fake semantic round-trips.

