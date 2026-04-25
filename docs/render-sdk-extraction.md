# MoonRenderSDK Extraction Plan

## Goal

`MoonRenderSDK` should be a standalone rendering and scene runtime repo. It should feel like a small, mature 3D engine package that other projects can embed without knowing Moon editor internals.

External projects should be able to:

- create a window
- initialize a renderer
- create a 3D world
- add ground, sky, weather, terrain, objects, walls, floors, grass, rivers, ocean
- configure materials, lights, shadows, and camera
- tick and render frames

They should not need to know about:

- `EditorApp`
- editor bridge / CEF / WebUI
- object copilot
- building authoring workflows
- current internal scene traversal details
- Diligent backend setup details

## Repo Boundary

### New Repo: `MoonRenderSDK`

Owns runtime rendering and common 3D scene components:

- platform window wrapper
- render device and render loop
- render backend integration
- scene graph / entity component basics
- transform / camera / input basics
- mesh and primitive generation
- material system
- texture loading and texture manager
- shader assets
- PBR render pipeline
- skybox / procedural sky
- environment, weather, fog, wind
- lighting and shadows
- terrain runtime and visual mesh generation
- water, river, ocean, grass, shrub visual generation
- runtime object creation API

### Existing Repo: `Moon`

Keeps editor and high-level authoring systems:

- EditorApp / editor UI
- editor bridge / CEF / WebUI
- AI / object copilot tools
- building authoring workflow
- semantic building generation
- massing generation
- project-specific sample data
- experimental extraction tools

Moon should consume `MoonRenderSDK`; it should not be the only place where rendering can run.

## Proposed Repo Layout

```text
MoonRenderSDK/
  include/
    MoonRender/
      MoonRender.h
      Runtime.h
      Window.h
      World.h
      Types.h
      Handles.h
      Materials.h
      Terrain.h
      Environment.h
      Lights.h
      Camera.h
  src/
    runtime/
      Runtime.cpp
      World.cpp
      WindowWin32.cpp
    core/
      Math/
      Camera/
      Input/
      Mesh/
      Scene/
      Assets/
    render/
      IRenderer.h
      SceneRenderer.cpp
      diligent/
    environment/
    terrain/
    materials/
  assets/
    shaders/
    textures/
      materials/
  examples/
    hello_scene/
    terrain_weather/
    material_gallery/
  cmake/
  CMakeLists.txt
  README.md
```

## Public API Shape

The public API should use stable descriptors and handles. It should not expose internal pointers such as `SceneNode*`, `Material*`, or `DiligentRenderer*`.

```cpp
#include <MoonRender/MoonRender.h>

int main()
{
    MoonRender::Runtime runtime;
    runtime.Initialize({
        .appName = "External Scene",
        .width = 1280,
        .height = 720
    });

    MoonRender::World world = runtime.CreateWorld();

    world.CreateSky({
        .type = MoonRender::SkyType::Procedural,
        .intensity = 1.0f
    });

    world.SetWeather({
        .type = MoonRender::WeatherType::Clear,
        .timeOfDayHours = 12.0f
    });

    auto groundMat = world.CreateMaterial({
        .preset = MoonRender::MaterialPreset::ConcreteFloor,
        .mapping = MoonRender::MappingMode::Triplanar
    });

    world.CreateGround({
        .size = { 100.0f, 100.0f },
        .material = groundMat
    });

    world.CreateDirectionalLight({
        .direction = { -0.35f, -0.8f, 0.25f },
        .intensity = 10.0f,
        .castShadows = true
    });

    while (runtime.PollEvents()) {
        runtime.Tick();
        runtime.Render(world);
    }

    runtime.Shutdown();
}
```

## Public API Categories

### Runtime

- `Runtime::Initialize(RuntimeDesc)`
- `Runtime::Shutdown()`
- `Runtime::PollEvents()`
- `Runtime::Tick()`
- `Runtime::Render(World&)`
- `Runtime::Resize(width, height)`
- `Runtime::GetWindowHandle()`

### World

- `CreateWorld`
- `DestroyWorld`
- `CreateEntity`
- `DestroyEntity`
- `SetTransform`
- `GetTransform`

### Scene Creation

- `CreatePrimitive`
- `CreateMesh`
- `CreateObject`
- `CreateGround`
- `CreateFloor`
- `CreateWall`
- `CreateRoom`
- `CreateTerrain`
- `CreateGrass`
- `CreateRiver`
- `CreateOcean`

### Materials

- `CreateMaterial`
- `SetMaterialPreset`
- `SetPBR`
- `SetTexture`
- `SetTextureSet`
- `SetMappingMode`
- `SetWaterMaterial`
- `SetTransparentMaterial`

Material presets should ship with the SDK asset pack:

- concrete
- concrete floor
- rock
- brick
- stone
- plaster
- ceramic tile
- wood
- wood floor
- metal
- glass
- fabric
- leather
- carpet
- plastic
- rubber
- water
- foliage / grass

### Environment

- `CreateSky`
- `SetTimeOfDay`
- `SetWeather`
- `SetWind`
- `SetFog`
- `SetCloudCoverage`
- `SetEnvironmentMap`

### Lighting and Shadows

- `CreateDirectionalLight`
- `CreatePointLight`
- `CreateSpotLight`
- `SetLight`
- `SetShadowSettings`
- `SetIBL`

### Camera

- `CreateCamera`
- `SetMainCamera`
- `SetCameraPerspective`
- `LookAt`
- `SetOrbitCamera`
- `SetFlyCamera`

## What Moves From Current Moon

Move into `MoonRenderSDK`:

- `engine/core/Math`
- `engine/core/Camera`
- `engine/core/Input`
- `engine/core/Mesh`
- `engine/core/Geometry`
- `engine/core/Assets/MeshManager.*`
- `engine/core/Texture`
- `engine/core/Scene/Component.*`
- `engine/core/Scene/Transform.*`
- `engine/core/Scene/SceneNode.*`
- `engine/core/Scene/Scene.*`
- `engine/core/Scene/MeshRenderer.*`
- `engine/core/Scene/Material.*`
- `engine/core/Scene/Light.*`
- `engine/core/Scene/Skybox.*`
- `engine/render`
- `engine/environment`
- `engine/terrain`
- `assets/shaders`
- `assets/textures/materials`

Keep in `Moon`, depending on SDK:

- `engine/building`
- `engine/massing`
- `engine/objects` if it stays authoring-heavy
- `editor`
- `tools/object_copilot`
- `tools/object_copilot_agent`
- project-specific generated assets

Move later or split carefully:

- `engine/physics`: can become optional SDK module `MoonRenderPhysics`
- `engine/vehicle`: should stay in Moon or become an optional gameplay module
- `engine/core/CSG`: useful for object creation, but it depends on `manifold`; make it optional
- `engine/core/Object`: expose only runtime object placement in SDK, keep authoring/catalog tooling in Moon

## Dependency Rules

`MoonRenderSDK` dependency direction should be:

```text
Public API
  -> Runtime facade
    -> Scene / Materials / Terrain / Environment
      -> Render backend
        -> Diligent / DirectX
```

Forbidden in SDK:

- editor includes
- CEF
- React/WebUI
- OpenAI/API clients
- object copilot Python service
- Moon-specific building authoring prompts

## Build Output

Recommended first target:

- `MoonRenderSDK.lib`
- `MoonRenderSDK.dll` later
- `MoonRenderSDK.assets/`
- `examples/hello_scene.exe`

External integration should look like:

```cmake
find_package(MoonRenderSDK CONFIG REQUIRED)
target_link_libraries(MyProject PRIVATE MoonRender::SDK)
```

## Migration Steps

### Phase 1: Stabilize API In Current Repo

Already started:

- `engine/api/MoonRenderWindow.h`
- `engine/api/MoonRenderWorld.h`
- `docs/render-api.md`

Next:

- change API return values from raw internal pointers to handles
- add `MoonRuntime` facade
- add explicit material descriptors and asset root config

### Phase 2: Create `MoonRenderSDK` Repo

Create a new repo with:

- public headers
- copied runtime modules
- shaders/material assets
- one example app
- CMake build

At this phase the code can still be mostly copied from Moon, but public headers must hide internals.

### Phase 3: Make Moon Consume The SDK

Replace Moon's local render/core/environment/terrain copies with either:

- git submodule
- subtree
- package dependency
- sibling repo reference during development

Editor and building tools call SDK APIs instead of directly owning the render runtime.

### Phase 4: Harden SDK

- ABI/export macros
- semantic versioning
- asset pack versioning
- backend abstraction
- tests for material presets, terrain generation, render smoke tests
- public examples

## Recommended Immediate Decision

Do not put only wrapper APIs in the separate repo. Put the render runtime itself there:

- material system
- render pipeline
- shader assets
- sky/weather/light/shadow
- terrain/water/grass runtime visuals
- window/runtime/world facade

Then Moon becomes a client of that repo, just like any other external project.
