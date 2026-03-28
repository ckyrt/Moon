# Vertical Transport Cases

This building pipeline now treats vertical circulation as its own system instead of hiding it inside generic `core` rooms.

## Supported JSON patterns

### 1. Enclosed high-rise stairwell

- Use `type: "stairwell"`
- Use one `vertical_systems` entry spanning `floor_from` to `floor_to`
- The generator keeps one continuous shaft and emits one stair flight per floor interval

### 2. Elevator shaft

- Use `type: "elevator"`
- Only generated when the JSON explicitly declares it
- The shaft is continuous from the first served level to the highest served level

### 3. Open internal villa stair

- Use `type: "stair"`
- Use `mode: "open"`
- Use `placement: "internal"`
- For a single connection, use one entry such as `0 -> 1`

### 4. External stair

- Use `type: "stair"`
- Use `mode: "open"`
- Use `placement: "external"`
- Connect outdoor spaces such as `terrace`, `balcony`, or `garden_entry`

### 5. Segmented open stair across multiple levels

- Use multiple `vertical_systems` entries
- One entry per connected interval, for example `0 -> 1` and `1 -> 2`
- Reuse the same rect if the stair is stacked in one open void

## Current design rule

- `stairwell` means a continuous enclosed vertical system
- `elevator` means a continuous enclosed shaft
- `stair` means a stair segment or open stair connection
- A stair system is no longer represented as a fake room that automatically grows walls around itself
