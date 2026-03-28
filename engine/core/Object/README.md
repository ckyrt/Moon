# Object Generation System

This module is the semantic entry point for procedural world objects.

Layer split:

- `Object/*`: object-facing catalog, variants, inheritance, defaults, and build requests.
- `CSG/*`: one geometry backend that currently executes object graphs.
- `Massing/*`: another shape-generation backend that can feed the same object layer.
- `ObjectCatalog`: editor/game-facing object families, tags, defaults, variants, and graph asset references.
- `ObjectCatalog`: supports `base_object` inheritance so related objects can share one definition and only override differences.
- `ObjectFactory`: resolves a build request into concrete parameters and builds a referenced object graph asset.
- `ObjectLibrary`: one-stop entry point that loads assets and builds objects.

Design intent:

- `CSG` is part of the object system, not a sibling semantic system.
- `Blueprint` should be read as the current object graph format, not as a CSG-only concept.
- a single object graph can mix multiple construction techniques
- `csg` is one node/operator family inside the graph
- `mass` or other generators can be added as more node/back-end options
- object-level metadata should not need to classify which sub-technique the graph uses
- we do not need a separate `graphs/` asset concept unless object assets start mixing multiple backend index formats in practice

Recommended scene-data shape for the future:

```json
{
  "object_id": "dining_table",
  "variant_id": "large",
  "transform": {
    "position": [0, 0, 0],
    "rotation_euler": [0, 0, 0],
    "scale": [1, 1, 1]
  },
  "parameter_overrides": {
    "w": 220.0
  }
}
```

That keeps placement data and generation data separate:

- generation chooses what the object is and how it is shaped
- scene json decides where it lives

Default asset layout:

- `assets/objects/catalog.json`: semantic object catalog
- `assets/objects/index.json`: object blueprint registry
- `assets/objects/components/*`: reusable blueprint/component assets used by objects
