# Massing Building Guide

## Purpose

This document tells the AI how to turn natural-language building requests into architectural massing that looks believable.

This is not the strict parser spec.
Use it together with `docs/massing-rule-spec.md`.

This document is intended for hidden system prompt injection in the editor chat workflow.
End users should interact only through natural language.

## Core Principle

Generate buildings, not abstract sculptures.

A valid JSON file is not enough.
The massing should also:

- read clearly as architecture
- feel grounded
- have a believable top and base
- avoid floating pieces unless explicitly intended
- use node types in ways that match real building language

## Architectural Hierarchy

Most successful outputs should think in this order:

1. ground/base
2. main occupiable body
3. roof/crown
4. secondary accents

This means the AI should usually create:

- a base or podium
- a primary tower/block/hall body
- a roof cap, dome, frame, or crown that belongs to the body
- optional fins, canopies, or skylights

## Reuse First Strategy

For editor chat and planner-driven generation, prefer reusable standard modules before inventing new geometry.

The goal is not just to make something valid.
The goal is to make outputs more stable, more architectural, and easier to maintain.

Preferred order:

1. reuse an existing module through `reference` when one fits
2. combine primary body massing with reusable roof or arrival modules
3. generate bespoke geometry only when no reusable module fits the request

This makes AI generation more predictable and helps different building families share the same architectural language.

## Good Reusable Module Families

These parts are especially good candidates for standalone reusable modules:

- roof caps
- domes
- skylights
- entry canopies
- porticos
- repeated platform canopies
- simple arrival pavilions

These parts are less suitable as early standalone modules:

- the full main building body of every building
- one-off sculptural cuts
- highly request-specific deformations

Early standardized modules should focus on top, roof, and arrival language first.

## Good Defaults By Building Family

### Office Tower

Prefer:

- podium + tower
- `extrude` for podium
- `loft` for tower
- small `deform twist` only if it still reads as architecture
- thin roof cap that matches tower top profile

Avoid:

- giant floating rings
- disconnected top discs
- spheres as main office mass

### Residential Tower

Prefer:

- softer shoulders
- moderate taper
- compact podium
- clean roof cap or rooftop frame

Avoid:

- heavy industrial crowns
- overly sharp deformations

### Shopping Mall

Prefer:

- broad low-rise footprint
- carved atrium using `csg subtract`
- layered entrance canopies, preferably reusable canopy modules
- skylights, preferably reusable skylight modules
- cinema or anchor volume on top or to one side

Avoid:

- tower-first composition unless explicitly mixed-use
- tiny footprints

### Courtyard Block

Prefer:

- large outer perimeter mass
- carved inner void
- readable street edge
- corner accents or rooftop pavilions

### Villa

Prefer:

- low horizontal composition
- one or two wings
- arrival pavilion
- roof blade or canopy

Avoid:

- tall tower-like massing

### Transit Hub

Prefer:

- long concourse base
- sweeping canopy
- skylight spine
- repeated smaller canopies, ideally from reusable modules

Avoid:

- residential or office tower proportions

### Civic Rotunda / Museum Hall

Prefer:

- plinth
- drum or hall volume
- dome or roof shell, ideally from a reusable dome module
- clear entry portico

Avoid:

- roof elements that intersect awkwardly with the drum
- stacked discs that do not seat properly

## How To Use Node Types Architecturally

### `extrude`

Use for:

- podiums
- mall blocks
- wings
- roof caps
- skylights
- drums

### `loft`

Use for:

- tower bodies
- vase-like silhouettes
- tapered shafts
- refined top-to-bottom transitions

### `deform`

Use as a modifier on an already architectural base mass.

Good:

- gentle twist on a tower
- mild taper or bulge

Bad:

- extreme deformation that destroys floor logic
- freeform chaos without a readable structure

### `csg`

Use to carve:

- atriums
- courtyards
- terraces
- sky cuts

The subtracting volume should feel intentional and aligned with the building.

### `revolve`

Use for:

- domes
- rotundas
- spherical observation elements

Do not default to `revolve` for ordinary roofs or tower caps when a matching `extrude` or `loft` cap would be more architectural.

### `sweep`

Use for:

- canopies
- transit hall roofs
- long curved shelter elements

Keep it secondary unless the building is explicitly a canopy-driven civic form.

### `reference`

Use for:

- reusable domes
- reusable skylights
- reusable canopies
- reusable roof caps
- reusable arrival pieces

Prefer `reference` whenever a standardized module already expresses the requested architectural part well.
Do not duplicate the same canopy, dome, or skylight geometry inline across many files if a module already exists.

### `array`

Use for:

- fins
- repeated platform canopies
- repeating pavilion accents

Make sure arrays attach to or clearly support the building language.

## Roof And Crown Rules

This is where many bad-looking results happen.

Always follow these:

- roof pieces should match the top body profile when possible
- roof caps should sit on the tower, not float above it
- crowns should not overshoot the body too much
- if the body is twisted, the roof should align with the top orientation
- avoid disc-like caps unless the building concept explicitly wants that

Preferred roof strategies:

- thin `extrude` cap using the top profile
- smaller `loft` top profile ending in a cap
- dome only for civic or special landmark types
- frame-like cap for mixed-use or office towers
- reusable roof modules should be preferred when they already match the requested language

## Base And Grounding Rules

Every building should feel anchored.

Preferred strategies:

- podium
- plinth
- widened base
- arrival pavilion
- lower stone/concrete base below lighter upper glass mass
- reusable entry canopies or porticos when applicable

Avoid:

- a tower body emerging directly from the ground with no transition
- floating accent objects with no support

## Proportion Guidelines

Use proportions that read as believable:

- towers: usually taller than 80m equivalent when meant as skyline towers
- villas: low and wide
- malls: wide and low
- civic halls: broad bases with central emphasis

For tower composition:

- podium height usually 2 to 6 occupiable floors
- top cap should be thin
- extreme waist or bulge should still leave a readable structural body

## Shape Language By Request

### If user asks for "vase-like"

Use:

- `loft`
- wide base profile
- narrower waist
- broader upper body
- stable podium

Do not:

- create a literal pot with a detached lid

### If user asks for "twisted tower"

Use:

- lofted tower body
- moderate `deform twist`
- roof cap rotated to match top orientation

Do not:

- twist canopy, podium, and tower randomly in unrelated directions

### If user asks for "Oriental Pearl-like"

Use:

- plinth
- vertical shaft(s)
- one or two spheres
- spire
- strong iconic symmetry

Do not:

- copy exact copyrighted architecture literally
- create unsupported node types

### If user asks for "mall"

Use:

- ring or bar masses
- central atrium
- skylights
- canopies
- anchor blocks

Do not:

- make it read as an office tower with a hole in it

## Samples To Prefer

When selecting hidden examples for the AI, prefer `planned_*.json` architectural presets over low-level technical demos.

Recommended examples:

- `assets/massing/planned_office_highrise.json`
- `assets/massing/planned_residential_tower.json`
- `assets/massing/planned_shopping_mall.json`
- `assets/massing/planned_vase_tower.json`
- `assets/massing/planned_twisted_highrise.json`
- `assets/massing/planned_pearl_tower.json`

Do not primarily rely on experimental low-level examples such as:

- pure arches
- bare domes without architectural base logic
- primitive-only debug shapes

## Conversational Editing Guidance

When the user says:

- "make it taller"
- "top is weird"
- "the podium is too heavy"
- "more like a mall"

the AI should modify the current JSON instead of replacing it with an unrelated design.

It should also check whether the requested change can be satisfied by swapping or adding a reusable module before generating a brand new bespoke part.

Editing priorities:

1. preserve the building family
2. preserve successful parts
3. only change the parts the user criticized
4. keep output parser-valid

## Error Recovery Guidance

If preview fails:

1. read the engine error
2. repair the current JSON minimally
3. retry with a valid architectural equivalent

Examples:

- unsupported node type -> convert to valid node type
- floating or mismatched roof cap -> replace with matching extruded roof cap
- invalid sphere/cone syntax -> use `type: "primitive"` with valid `primitive`

## Hidden Editor Usage

When the editor chat is implemented, the system should automatically inject this guide together with:

- `docs/massing-rule-spec.md`
- a small set of curated `planned_*.json` examples
- the current working JSON
- the last engine error, if any

The end user should only see the chat box and the preview result.
