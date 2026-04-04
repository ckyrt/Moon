# Building System

This is the human-facing source of truth for the building system.

## Purpose

The building system turns semantic building JSON into generated building data and previewable geometry.

The authoring contract is semantic JSON, not manual geometry.

## Ownership

- building authoring starts from `moon_building` JSON
- `engine/building/BuildingPipeline.*` orchestrates validation and generation
- `engine/building/SemanticBuildingValidator.*` validates authored semantic JSON
- `engine/building/LayoutResolver.*` and `engine/building/SemanticBuildingResolver.*` resolve authored semantic input into internal building data
- `engine/building/SchemaValidator.*` is a compatibility facade around authoring validation and resolution
- `engine/building/*Generator.*` files produce generated building data from resolved definitions
- `engine/building/BuildingToObjectBlueprintConverter.*` converts generated buildings into object-preview data for editor/runtime preview

## Pipeline

1. semantic building JSON
2. semantic validation
3. semantic resolution into `BuildingDefinition`
4. resolved layout validation
5. structural/layout generation
6. building-to-object conversion for preview/runtime assembly
7. mesh/object preview

## Key Rule

JSON is the single source of truth for authored buildings.

`BuildingDefinition` is an internal resolved runtime structure.

It is not the external authoring contract.

Current code organization follows three layers:

- `Authoring`: authored semantic JSON parsing and validation
- `Resolution`: semantic JSON to resolved `BuildingDefinition`
- `Generation`: resolved building to geometry, preview, and runtime outputs

On top of those layers, product workflow should be staged authoring:

- `form`
- `vertical`
- `plate`
- `program`
- `facade`
- `scene_composition`

Workflow details live in:

- `docs/building-authoring-workflow.md`
- `engine/building/BuildingAuthoringWorkflow.*`

## Root Format

Current authored buildings use:

- `schema: "moon_building"`
- `grid`
- `building_type`
- `style`
- `mass`
- `program`

## Semantic Model

Buildings are described in terms of:

- masses
- floors
- spaces
- units
- circulation
- cores
- voids
- adjacency and constraints

AI or tools should describe architecture and relationships, not vertices or triangles.

## Current Typology Direction

Supported families used in assets and tooling include:

- `villa`
- `apartment`
- `office`
- `office_tower`
- `cbd_residential`
- `mall`
- `shopping_center`
- `retail_center`

## Authoring Rules

- keep JSON semantic and validator-friendly
- use consistent `space_id`
- use `unit_id` when spaces belong to a reusable unit grouping
- keep circulation explicit
- use `void` for atriums/courtyards/lightwells
- prefer realistic adjacency over overfitted geometry hints

## Canonical References

- `assets/building/catalog.json`
- `assets/building/test_building.json`
- `assets/building/fixtures/apartment_building.json`
- `assets/building/reference/corporate_office_tower.json`
- `engine/building/BuildingPipeline.h`
