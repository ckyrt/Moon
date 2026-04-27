# Moon Render API 整理

这份文档面向“其他 repo 想调用当前 Moon 渲染能力”的场景。当前工程已经有渲染、场景、材质、天空、天气、地形、建筑/物体生成等模块，但公开边界偏底层，外部项目直接使用会比较散。建议把对外入口收敛到 `engine/api/MoonRenderWorld.h`，底层继续复用现有 `Scene`、`MeshRenderer`、`Material`、`EnvironmentComponent`、`TerrainComponent` 和 `DiligentRenderer`。

## 现有模块能力

### 渲染后端

文件：`engine/render`

- `IRenderer`
  - `Initialize(RenderInitParams)`
  - `Shutdown()`
  - `Resize(width, height)`
  - `BeginFrame()`
  - `EndFrame()`
  - `RenderFrame()`
  - `SetViewProjectionMatrix(float[16])`
  - `DrawMesh(Mesh*, Matrix4x4)`
  - `DrawCube(Matrix4x4)`
- `DiligentRenderer`
  - DirectX/Diligent 后端
  - PBR surface pass
  - 透明 pass
  - water material pass
  - skybox / procedural sky
  - precipitation volume
  - directional shadow map
  - point light shadow cube map
  - material texture binding: albedo, AO, roughness, metalness, normal
  - object picking render target
  - preview render target
- `SceneRendererUtils`
  - `PrepareRender(renderer, scene, camera, environmentState)`
  - `RenderMeshes(renderer, scene)`
  - `RenderScene(renderer, scene, camera, environmentState)`

### 场景与组件

文件：`engine/core/Scene`

- `Scene`
  - create / destroy node
  - find node by name / id
  - traverse / traverse active
  - update
- `SceneNode`
  - transform
  - parent / children
  - add / get / remove component
- `MeshRenderer`
  - bind `Mesh`
  - visible flag
  - render through `IRenderer`
- `Material`
  - PBR parameters: base color, metallic, roughness, opacity
  - preset materials: concrete, rock, brick, stone, plaster, tile, wood, metal, glass, fabric, leather, carpet, plastic, rubber
  - texture maps: albedo, normal, AO, roughness, metalness
  - mapping mode: UV / triplanar
  - shading model: default lit / water
- `Light`
  - directional / point / spot
  - color / intensity / range / attenuation / spot angles
  - cast shadows flag
- `Skybox`
  - none / equirectangular HDR / cubemap / procedural
  - intensity / rotation / tint / IBL flag

### 几何与物体

文件：`engine/core/Geometry`、`engine/core/Assets`、`engine/core/Object`

- `MeshManager`
  - `CreateCube`
  - `CreateSphere`
  - `CreatePlane`
  - `CreateCylinder`
  - `CreateCone`
  - `CreateTorus`
  - `CreateCapsule`
  - `CreateQuad`
  - register / get named mesh
- `MeshGenerator`
  - primitive mesh generation
  - terrain from heightmap
  - river from polyline
- Object blueprint system
  - `ObjectFactory::BuildObject`
  - blueprint database / primitive / CSG style object definitions

### 环境、天气、天空

文件：`engine/environment`

- `EnvironmentComponent`
  - profile
  - weather transition
  - time of day
  - runtime state
- `EnvironmentState`
  - time of day
  - weather: clear, cloudy, rain, snow, fog, storm
  - wind
  - atmosphere: sun direction/color/intensity, sky colors, fog density, cloud coverage

### 地形、水体、草地

文件：`engine/terrain`

- `TerrainComponent`
  - profile
  - heightmap data
  - resize / get / set height samples
  - sample world height and normal
  - chunk dirty/runtime state
- `ProceduralTerrainGenerator`
  - `CreateOpenWorldLandscape(settings)`
  - `CreateFromWorldBuildSpec(spec)`
- `TerrainVisualBuilder`
  - terrain mesh
  - river mesh
  - ocean mesh
  - grass mesh
  - shrub mesh
  - precipitation mesh

### 建筑

文件：`engine/building`

- wall generation
- facade generation
- door / stair generation
- building pipeline
- floor plate / layout / semantic building systems
- building-to-object blueprint converter

这一块能力很强，但目前还不是一个简单“CreateBuilding/CreateWall/CreateFloor”的对外 API，需要再封装一层。

## 新增对外入口

文件：

- `engine/api/MoonRenderWindow.h`
- `engine/api/MoonRenderWorld.h`

命名空间：`Moon::Api`

### 窗体

- `RenderWindow::Create(RenderWindowDesc)`
- `RenderWindow::PollEvents()`
- `RenderWindow::ShouldClose()`
- `RenderWindow::RequestClose()`
- `RenderWindow::GetNativeHandle()`
- `RenderWindow::GetWidth()`
- `RenderWindow::GetHeight()`
- `RenderWindow::SetResizeCallback(callback)`

### 基础对象

- `RenderWorld(EngineCore&)`
- `RenderWorld(Scene&, MeshManager&)`
- `GetScene()`
- `GetMeshManager()`
- `CreateNode(name, TransformDesc)`
- `AddMesh(node, mesh)`
- `AddMaterial(node, MaterialDesc)`
- `CreatePrimitive(PrimitiveDesc)`

### 常用场景物体

- `CreateGround(FloorDesc)`
- `CreateFloor(FloorDesc)`
- `CreateWall(WallDesc)`
- `CreateWaterPlane(WaterPlaneDesc)`
- `CreateRiver(RiverDesc)`
- `CreateProceduralTerrain(settings, createRivers, createOcean, createGrass)`

### 天空、天气、光照

- `CreateEnvironment(profile, weather, timeOfDayHours)`
- `CreateProceduralSky(intensity)`
- `CreateHdrSkybox(hdrPath, intensity)`
- `CreateDirectionalLight(name, position, lookAt, color, intensity)`
- `CreatePointLight(name, position, color, intensity, range, castShadows)`
- `CreateSpotLight(name, position, lookAt, color, intensity, range, innerConeDeg, outerConeDeg, castShadows)`

### 帧更新与渲染

- `Tick(deltaTimeSeconds)`
- `Render(DiligentRenderer&, Camera&, environmentState)`

渲染循环仍由宿主项目控制：

```cpp
#include "engine/api/MoonRenderWindow.h"
#include "engine/api/MoonRenderWorld.h"

EngineCore engine;
engine.Initialize();

DiligentRenderer renderer;
Moon::Api::RenderWindow window;
Moon::Api::RenderWindowDesc windowDesc;
windowDesc.title = L"Moon External Renderer";
windowDesc.width = 1280;
windowDesc.height = 720;
window.Create(windowDesc);

Moon::Api::RenderWorld world(engine);
world.CreateProceduralSky();
world.CreateEnvironment({}, Moon::WeatherType::Clear, 12.0f);

Moon::Api::FloorDesc ground;
ground.name = "Ground";
ground.width = 80.0f;
ground.depth = 80.0f;
world.CreateGround(ground);

Moon::Api::WallDesc wall;
wall.start = Moon::Vector3(-5.0f, 0.0f, -4.0f);
wall.end = Moon::Vector3(5.0f, 0.0f, -4.0f);
world.CreateWall(wall);

Moon::TerrainGenerationSettings terrain;
terrain.hasOcean = true;
world.CreateProceduralTerrain(terrain);

RenderInitParams params;
params.windowHandle = window.GetNativeHandle();
params.width = window.GetWidth();
params.height = window.GetHeight();
renderer.Initialize(params);
window.SetResizeCallback([&](uint32_t width, uint32_t height) {
    renderer.Resize(width, height);
    if (height > 0) {
        engine.GetCamera()->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }
});

while (window.PollEvents()) {
    world.Tick(dt);
    renderer.BeginFrame();
    world.Render(renderer, *engine.GetCamera());
    renderer.EndFrame();
}
```

## 目前还没真正统一的接口

- 窗体创建：新增了轻量 `Moon::Api::RenderWindow`，但目前只覆盖 Win32。后续如果要跨平台，需要把它移动成 `engine/platform` 层，并补 SDL/GLFW/原生平台实现。
- 后端无关渲染：`IRenderer` 太薄，`SceneRendererUtils` 目前直接依赖 `DiligentRenderer`。如果要支持 bgfx/Vulkan/DX12 多后端，建议把 skybox、shadow、material binding、scene lights 抽到更完整的 `ISceneRenderer`。
- 建筑 API：已有 building pipeline，但还缺稳定 facade，例如 `CreateBuilding`、`CreateRoom`、`CreateWallWithOpening`、`CreateFloorPlate`。
- 植被/草地：已有 grass mesh builder，但缺植被实例、LOD、风动画、材质参数的外部 API。
- 天气粒子：渲染器支持 precipitation volume，但外部 API 目前主要通过 `EnvironmentComponent` 触发雨雪状态。
- 资源打包：材质 preset 使用相对贴图路径，外部 repo 需要约定 asset root。

## 建议最终 API 分层

### 1. Platform

- `CreateWindow`
- `PollEvents`
- `GetNativeHandle`
- `Resize`
- `ShouldClose`

### 2. Render Device

- `CreateRenderer`
- `InitializeRenderer`
- `BeginFrame`
- `RenderWorld`
- `EndFrame`
- `ShutdownRenderer`

### 3. World Authoring

- `CreateScene`
- `CreateEntity`
- `CreatePrimitive`
- `CreateGround`
- `CreateFloor`
- `CreateWall`
- `CreateTerrain`
- `CreateRiver`
- `CreateOcean`
- `CreateGrass`
- `CreateObjectFromBlueprint`
- `CreateBuilding`

### 4. Environment

- `CreateSky`
- `SetTimeOfDay`
- `SetWeather`
- `SetWind`
- `SetFog`
- `SetCloudCoverage`

### 5. Lighting

- `CreateDirectionalLight`
- `CreatePointLight`
- `CreateSpotLight`
- `SetShadowSettings`
- `SetIBL`

### 6. Materials

- `CreateMaterial`
- `SetMaterialPreset`
- `SetPBR`
- `SetTextures`
- `SetMappingMode`
- `SetWaterMaterial`

## 下一步建议

短期：让另一个 repo include `engine/api/MoonRenderWorld.h`，先用当前 facade 跑通创建世界和渲染循环。

中期：新增 `RenderWindow` 和 `MoonRuntime`，把 Win32 窗口、EngineCore、DiligentRenderer、主循环包装成一个更完整 SDK 入口。

长期：把 `DiligentRenderer` 专属能力上移成后端无关接口，并为 DLL/export 做 ABI 清理。

## 单独拆 repo 的建议

可以拆，而且建议拆成一个独立 `MoonRenderSDK` repo。这个 repo 不应该只是 wrapper API，而应该拥有材质、渲染管线、shader、天空/天气/光照/阴影、地形/水体/草地等 runtime 能力。外部项目只依赖稳定 API、头文件、导入库和运行时资源，不关心内部 `SceneNode`、`DiligentRenderer`、`TerrainVisualBuilder` 怎么组织。

建议 repo 结构：

```text
MoonRenderSDK/
  include/
    MoonRender/
      MoonRuntime.h
      MoonWorld.h
      MoonWindow.h
      MoonTypes.h
  src/
    MoonRuntime.cpp
    MoonWorld.cpp
    MoonWindowWin32.cpp
  adapters/
    MoonEngine/
      MoonEngineAdapter.cpp
      MoonEngineAdapter.h
  third_party/
    diligent/
  assets/
    shaders/
    textures/
  examples/
    hello_world/
  MoonRenderSDK.sln
  MoonRenderSDK.vcxproj
  README.md
```

接口 repo 里应该暴露 C++ SDK 风格 API：

- `MoonRuntime`：初始化、关闭、主循环、resize、render frame
- `MoonWindow`：创建窗口、消息循环、native handle
- `MoonWorld`：创建地面、天空、天气、地形、物体、墙、floor、草地、河流、海洋、光照、阴影
- `MoonTypes`：只放稳定 POD/enum，例如 `Vec3`、`Transform`、`MaterialDesc`、`WeatherType`

当前 `engine/api` 可以作为第一步，但还不是最终 SDK：它仍然直接 include 了 Moon 内部类型。下一步要把它改成“public API + engine adapter”两层：

- public API 不暴露 `SceneNode*`、`DiligentRenderer&`、`EngineCore&`
- adapter 层在 Moon 主 repo 内实现，把 public API 翻译到现有 engine 组件
- 外部项目不需要 clone SDK 仓库，只使用发布包中的 `include/`、`assets/`、`MoonRenderSDK.dll`、`MoonRenderSDK.lib`

最稳的迁移顺序：

1. 在 Moon 主 repo 保留 `engine/api`，先稳定调用语义。
2. 新建 `MoonRenderSDK` repo，把 public header、runtime 代码、shader/material assets 迁过去。
3. SDK 用 Visual Studio 编译出 `MoonRenderSDK.dll` 和 import lib。
4. 外部项目通过发布包使用 DLL；Moon 主 repo 作为 SDK consumer，直接链接 `E:\game_engine\MoonRenderSDK` 的头文件和 `dll/lib` 产出。
5. 最后让 Moon 的 editor/building/tools 调 SDK，而不是直接拥有渲染 runtime。

更完整的拆分边界见 [`docs/render-sdk-extraction.md`](render-sdk-extraction.md)。
