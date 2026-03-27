# Object Generation System

This module sits above the low-level procedural shape builders.

Layer split:

- `CSG/*`: current implementation of the procedural object graph runtime.
- `ObjectCatalog`: editor/game-facing object families, tags, defaults, variants, and graph asset references.
- `ObjectCatalog`: supports `base_object` inheritance so related objects can share one definition and only override differences.
- `ObjectFactory`: resolves a build request into concrete parameters and builds a referenced object graph asset.
- `ObjectLibrary`: one-stop entry point that loads assets and builds objects.

Design intent:

- the object layer should reference one unified object graph asset
- that graph can internally use group/reference/boolean and later add mass nodes
- object-level metadata should not need to classify which sub-technique the graph uses

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
- `assets/csg/index.json`: CSG backend registry
