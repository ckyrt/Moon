# Object System

This is the human-facing source of truth for the current object system.

## Purpose

The object system is now asset-library driven.

It answers:

- which asset ids exist
- where each asset file lives
- which category and tags each asset exposes
- how a selected asset builds geometry

## Ownership

- `assets/objects/index.json` is the runtime object registry
- `assets/objects/components/*` stores reusable object graph assets
- `engine/core/Object/*` implements index loading and direct asset builds
- `engine/core/CSG/*` is one geometry backend used by object graphs

## Layer Model

1. Index entry
2. Object graph asset
3. Geometry backend
4. Built runtime meshes/lights

`Blueprint` is the current graph format.

`CSG` is an implementation technique inside the graph layer, not a separate semantic registry.

## Current Asset Contract

### Runtime object entry

Defined in `assets/objects/index.json`.

Each entry should include:

- `id`
- `path`
- `description`
- `category`
- `tags`

`id` is the buildable object id. Scene and tooling should reference this id directly.

### Graph asset file

Stored under `assets/objects/components/*`.

Each graph asset owns:

- parameters and their defaults
- references to reusable sub-assets
- the actual graph used to build geometry/lights

## Authoring Rules

- Prefer reusing existing parts before creating a new complex asset
- Keep searchable metadata in `index.json`
- Keep shape and parameter implementation details in component JSON files
- Treat random selection as choosing among approved assets, not inventing new parameter combinations
- If you need a new visual/spec variant, create a new asset id instead of adding a semantic catalog layer

## Scene Usage Direction

Scene placement should reference concrete asset ids plus optional parameter overrides.

Examples:

- `television_v1`
- `fridge_v1`
- `staircase_v1`

## Canonical References

- `assets/objects/index.json`
- `assets/objects/components/furniture/chair_v1.json`
- `assets/objects/test_room.json`
