# Building System

This is the human-facing source of truth for the building system.

## Purpose

The building system turns semantic building JSON into generated building data and previewable geometry.

The authoring contract is semantic JSON, not manual geometry.

## Ownership

- building authoring starts from `moon_building` JSON
- `engine/building/BuildingPipeline.*` orchestrates validation and generation
- `engine/building/SchemaValidator.*` parses and validates semantic input
- `engine/building/*Generator.*` files produce resolved building data
- `engine/building/BuildingToObjectBlueprintConverter.*` converts generated buildings into object-preview data for editor/runtime preview

## Pipeline

1. semantic building JSON
2. schema validation
3. layout validation and resolution
4. structural/layout generation
5. building-to-object conversion for preview/runtime assembly
6. mesh/object preview

## Key Rule

JSON is the single source of truth for authored buildings.

`BuildingDefinition` is an internal resolved runtime structure.

It is not the external authoring contract.

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
