# Object Assets

This directory is the semantic entry point for world objects.

Layering:

- `assets/objects/catalog.json`: object-facing definitions used by gameplay, tools, and scene data.
- `assets/csg/*`: one geometry backend that can implement object generators.
- future backends can live beside `csg`, such as `massing`, without changing object-facing paths.

Authoring rules:

1. Add reusable shape parts before adding a new complex object.
2. Reuse parameterized parts when shape family is the same.
3. Only split into separate parts when the shape family actually changes.

Examples:

- round wooden table leg and round wooden chair leg should reuse the same part with different parameters.
- a round metal tube leg and a rectangular wooden leg should be separate parts because the profile family is different.
- nightstands and wardrobes should inherit from a shared storage object when only proportions differ.
