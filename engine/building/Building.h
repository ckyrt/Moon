#pragma once

/**
 * @file Building.h
 * @brief Moon Engine Building System V8
 * 
 * AI Procedural Building System
 * Main include file for the building module
 * 
 * Usage:
 * 
 * ```cpp
 * #include <Building/Building.h>
 * 
 * using namespace Moon::Building;
 * 
 * // Create pipeline
 * BuildingPipeline pipeline;
 * 
 * // Process JSON
 * GeneratedBuilding building;
 * std::string error;
 * if (pipeline.ProcessBuilding(jsonString, building, error)) {
 *     // Use building.walls, building.doors, building.windows, etc.
 * } else {
 *     // Handle error
 * }
 * ```
 * 
 * JSON Schema: moon_building_v8
 * Grid Size: 0.5 meters
 * 
 * Pipeline Stages:
 * 1. Schema Validator - Parse and validate JSON
 * 2. Layout Validator - Check spatial constraints
 * 3. Mass Processor - Process building volumes
 * 4. Floor Generator - Generate floor structure
 * 5. Space Graph Builder - Build connectivity graph
 * 6. Wall Generator - Generate wall segments
 * 7. Door Generator - Place doors
 * 8. Stair Generator - Generate stairs
 * 9. Facade Generator - Generate exterior elements
 * 
 * See docs/moon_ai_building_system_v8.md for full specification
 */

// Core types
#include "BuildingTypes.h"

// Validators
#include "SchemaValidator.h"
#include "LayoutValidator.h"

// Generators
#include "SpaceGraphBuilder.h"
#include "WallGenerator.h"
#include "DoorGenerator.h"
#include "StairGenerator.h"
#include "FacadeGenerator.h"

// Main pipeline
#include "BuildingPipeline.h"
