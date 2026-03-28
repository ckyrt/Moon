# Object Assets

This directory is the semantic entry point for world objects.

Layering:

- `assets/objects/catalog.json`: object-facing definitions used by gameplay, tools, and scene data.
- `assets/objects/index.json`: registry of buildable object blueprints.
- `assets/objects/components/*`: reusable blueprint parts and object generators.
- future backends can live beside `csg`, such as `massing`, without changing object-facing paths.

System ownership:

- `object` is the semantic system users and gameplay talk to.
- `blueprint` is the current object graph format.
- `csg` is one object graph technique, not the owner of object assets.

Simplification rule:

- do not introduce an extra `graphs/` asset layer unless we truly need multiple graph registries.
- if an object asset is procedurally built, it is already a graph in implementation terms; that does not require a separate top-level asset directory.

Authoring rules:

1. Add reusable shape parts before adding a new complex object.
2. Reuse parameterized parts when shape family is the same.
3. Only split into separate parts when the shape family actually changes.

Examples:

- round wooden table leg and round wooden chair leg should reuse the same part with different parameters.
- a round metal tube leg and a rectangular wooden leg should be separate parts because the profile family is different.
- nightstands and wardrobes should inherit from a shared storage object when only proportions differ.
