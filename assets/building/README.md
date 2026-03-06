# Building Test Data

This directory contains test building definitions in `moon_building_v8` schema format.

These JSON files are used for:
- Unit testing the Building System
- AI training data reference
- Example buildings for developers
- Validation of building generation logic

## File Structure

```
building/
├── basic/              # Simple test cases
│   ├── minimal_building.json
│   ├── simple_room.json
│   └── multi_floor.json
├── residential/        # Residential buildings
│   ├── villa.json
│   ├── luxury_villa.json
│   └── apartment.json
└── complex/            # Complex layouts
    └── l_shaped.json
```

## Schema Version

All files use `moon_building_v8` schema with:
- Grid alignment: 0.5m
- Snake_case field names
- Mandatory fields: schema, grid, style, masses, floors

## Usage in Tests

C++ tests load these files via `TestHelpers::LoadTestData(filename)`.

## AI Reference

These files demonstrate correct building definitions and can be used as training examples for AI-generated building layouts.
