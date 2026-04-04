# Building Authoring Workflow

This document defines the staged authoring workflow for AI-generated building assets.

## Product Goal

The product should support:

- AI-generated full building asset JSON
- AI-driven redesign by rewriting the current building asset JSON
- command-based scene composition for placement and instance edits

The engine should not hide invalid authored data.
If the authored building JSON is invalid, validation should fail so AI can correct the asset.

## Core Boundary

Use this split:

- building design change: AI rewrites a full `moon_building` asset JSON
- scene placement change: AI emits `scene_edit_ops`

Examples:

- "make this villa three floors" -> rewrite building asset JSON
- "move the sofa near the window" -> scene op
- "turn this mall into an office tower" -> rewrite building asset JSON
- "delete the chair in the lobby" -> scene op

## Workflow Shape

Use one staged workflow framework with typology-specific templates.

Shared stage set:

1. `form`
2. `vertical`
3. `plate`
4. `program`
5. `facade`
6. `scene_composition`

Not every building type needs every stage with the same weight.

## Workflow Templates

### `residential_lite`

Use for:

- villa
- house
- townhouse
- low-complexity two-floor residential

Stage order:

1. `form`
2. `vertical`
3. `program`
4. `facade`
5. `scene_composition`

Notes:

- `plate` is usually unnecessary for simple residential assets
- room relationships matter more than complex core logic

### `residential_midrise`

Use for:

- small apartment buildings
- low-rise to mid-rise residential blocks
- three-floor apartment buildings

Stage order:

1. `form`
2. `vertical`
3. `program`
4. `facade`
5. `scene_composition`

Notes:

- stairs, corridors, and unit organization must be confirmed early

### `office_tower`

Use for:

- office towers
- high-rise office buildings
- podium + tower office buildings

Stage order:

1. `form`
2. `vertical`
3. `plate`
4. `program`
5. `facade`
6. `scene_composition`

Notes:

- core and elevator logic must be solved before detailed planning
- `plate` is a distinct approval step for tower work

### `retail_mall`

Use for:

- shopping malls
- retail centers
- shopping centers

Stage order:

1. `form`
2. `vertical`
3. `plate`
4. `program`
5. `facade`
6. `scene_composition`

Notes:

- atriums, voids, and circulation loops are first-class design decisions

## Stage Intent

### `form`

Goal:

- confirm massing, footprint, floor count, and overall height

Typical concerns:

- footprint
- setbacks
- podium
- roof
- entry massing

### `vertical`

Goal:

- confirm floor height strategy and vertical circulation

Typical concerns:

- stairs
- elevators
- cores
- escalators for retail
- double-height spaces

### `plate`

Goal:

- approve floor plate logic before detailed space planning

Typical concerns:

- typical floors
- voids
- atriums
- special floors
- setbacks by level

### `program`

Goal:

- define actual rooms, tenant zones, units, and adjacencies

Typical concerns:

- unit mix
- lobbies
- corridors
- shops
- office zones
- service spaces

### `facade`

Goal:

- finalize facade language after spatial organization is stable

Typical concerns:

- windows
- balconies
- curtain walls
- canopies
- materials

### `scene_composition`

Goal:

- place furniture, props, and site objects without redesigning the building asset

Typical concerns:

- furniture placement
- kiosks
- landscaping
- transforms

This stage should use `scene_edit_ops`, not a rewritten building JSON.

## Authoring Rule

For stages `form`, `vertical`, `plate`, `program`, and `facade`:

- AI should output a full valid `moon_building` JSON asset
- AI may focus on one design layer per turn
- unchanged parts of the building should remain stable

For stage `scene_composition`:

- AI should output `scene_edit_ops`
- the building asset remains unchanged unless the user requests a building redesign

## Current Engine Alignment

Current code already supports part of this direction:

- authoring contract: `engine/building/SemanticBuildingTypes.*`
- validation: `engine/building/SemanticBuildingValidator.*`
- resolution: `engine/building/LayoutResolver.*` and `engine/building/SemanticBuildingResolver.*`
- generation: `engine/building/*Generator.*`
- scene editing boundary: `docs/scene-system-ai.md`

Current workflow metadata lives in:

- `engine/building/BuildingAuthoringWorkflow.*`

This metadata should guide prompts, editor orchestration, and future stage-specific validation.
