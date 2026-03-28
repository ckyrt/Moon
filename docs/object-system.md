# Object System

This is the human-facing source of truth for the object system.

## Purpose

The object system is the top-level semantic layer for reusable world objects.

It answers:

- what the object is
- which variants it supports
- which parameters are exposed
- which graph asset builds it

## Ownership

- `assets/objects/catalog.json` is the semantic object catalog
- `assets/objects/index.json` is the runtime graph registry
- `assets/objects/components/*` stores reusable object graph assets
- `engine/core/Object/*` implements loading, inheritance, resolution, and build requests
- `engine/core/CSG/*` is one geometry backend used by object graphs

## Layer Model

1. Object definition
2. Object graph
3. Geometry backend
4. Built runtime meshes/lights

`Object` is the semantic owner.

`Blueprint` is the current graph format.

`CSG` is an implementation technique inside the graph layer, not the semantic owner of object assets.

## Current Asset Contract

### Semantic object entry

Defined in `assets/objects/catalog.json`.

Key fields:

- `id`
- `display_name`
- `description`
- `category`
- `graph_asset`
- `tags`
- `defaults`
- `parameters`
- `variants`
- optional `base_object`

### Graph asset entry

Registered in `assets/objects/index.json`.

Each entry maps a runtime graph id to a `.json` asset file under `assets/objects/components/*`.

## Authoring Rules

- Prefer reusing existing parts before creating a new complex asset
- Keep semantic object metadata in `catalog.json`
- Keep graph implementation details in component JSON files
- Do not create a second semantic registry
- Do not make CSG-specific concerns visible at the semantic object layer unless required by runtime behavior

## Scene Usage Direction

Scene placement should reference semantic object ids and optional variants/parameter overrides.

The scene should not depend directly on low-level component asset paths unless it is intentionally previewing raw graph assets.

## Canonical References

- `assets/objects/catalog.json`
- `assets/objects/index.json`
- `assets/objects/components/furniture/chair_v1.json`
- `assets/objects/test_room.json`
