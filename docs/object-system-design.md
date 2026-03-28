# Object System Design

## Decision

`Object` is the top-level semantic system.

- `CSG` is one geometry construction technique inside the object system.
- `Blueprint` belongs to the object graph layer, not to the semantic `CSG` layer.
- `Massing` is another shape-generation technique that can participate in the same object pipeline.

## Mental Model

The clean stack is:

1. `Object definition`
2. `Object graph`
3. `Geometry backends / node families`
4. `Built meshes / lights / runtime instances`

Where:

- `Object definition` answers "what is this thing in gameplay/editor terms?"
- `Object graph` answers "how is this thing assembled?"
- `CSG` answers "which boolean or primitive operations build part of the graph?"
- `Massing` answers "which volumetric generation steps create bigger forms that may feed the graph?"

## Recommended Ownership

### 1. Object layer owns:

- semantic IDs and categories
- inheritance like `base_object`
- variants and default parameter sets
- editor-facing tags and search metadata
- placement-facing asset references
- the single semantic asset entry point for object definitions

### 2. Object graph layer owns:

- reusable assembly graphs
- `primitive`, `group`, `reference`, `light`
- `csg` as one graph operator
- future `mass`, `mesh`, `scatter`, `deform`, or similar nodes if needed

### 3. CSG layer owns:

- boolean execution
- primitive mesh generation used by graph nodes
- CSG-specific validation and build options

### 4. Massing layer owns:

- large-scale form generation rules
- direct mesh generation for architectural forms
- optional compilation into object graphs when that representation is sufficient

## Asset Layout

Recommended asset split:

```text
assets/
  objects/
    catalog.json
    index.json
    components/
  massing/
```

Interpretation:

- `assets/objects/catalog.json` is the semantic object asset root.
- `assets/objects/index.json` is the current blueprint registry used by object generation.
- `assets/objects/components/*` are implementation assets used by current object entries.
- `assets/massing/*` remains an authored source format for volumetric generation.

This is intentionally simpler than adding a separate `graphs/` root. The key distinction is semantic ownership, not one more folder layer.

## Buildings

For buildings, the intended flow should be:

1. building intent/schema
2. massing and semantic layout generation
3. object graph assembly
4. CSG only where carving/openings/boolean composition is actually needed

So:

- building is not a synonym for CSG
- blueprint should not live conceptually under CSG
- CSG-generated walls/openings are just one implementation technique used by building/object generation

## Transition Rule

During active development we should prefer direct cleanup over compatibility wrappers:

- keep `assets/objects/catalog.json` as the only object-facing entry asset
- keep blueprint runtime code in `engine/core/Object`
- keep `assets/objects/index.json` as the blueprint registry until a real backend-agnostic graph runtime exists
