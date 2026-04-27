#include <MoonRender/MoonRender.h>

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

namespace {

std::string ResolveSdkAssetsRoot()
{
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

    std::filesystem::path exePath(modulePath);
    std::filesystem::path sdkAssets = exePath.parent_path().parent_path().parent_path().parent_path() / "MoonRenderSDK" / "assets";
    return sdkAssets.string();
}

MoonRender::MaterialHandle CreateMaterial(
    MoonRender::World& world,
    MoonRender::MaterialPreset preset,
    const MoonRender::Color& color,
    float roughness,
    float tiling)
{
    MoonRender::MaterialDesc material{};
    material.preset = preset;
    material.baseColor = color;
    material.roughness = roughness;
    material.tiling = tiling;
    return world.CreateMaterial(material);
}

} // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    std::string assetRoot = ResolveSdkAssetsRoot();

    MoonRender::Runtime runtime;
    MoonRender::RuntimeDesc runtimeDesc{};
    runtimeDesc.appName = "Moon Hello SDK";
    runtimeDesc.width = 1600;
    runtimeDesc.height = 900;
    runtimeDesc.assetRoot = assetRoot.c_str();
    if (!runtime.Initialize(runtimeDesc)) {
        MessageBoxA(nullptr, "Failed to initialize MoonRenderSDK.", "Moon", MB_OK | MB_ICONERROR);
        return 1;
    }

    MoonRender::World world = runtime.CreateWorld();

    MoonRender::MaterialHandle groundMaterial = CreateMaterial(
        world,
        MoonRender::MaterialPreset::ConcreteFloor,
        { 0.64f, 0.66f, 0.68f, 1.0f },
        0.82f,
        3.0f);
    MoonRender::MaterialHandle wallMaterial = CreateMaterial(
        world,
        MoonRender::MaterialPreset::Plaster,
        { 0.82f, 0.80f, 0.76f, 1.0f },
        0.72f,
        2.0f);
    MoonRender::MaterialHandle propMaterial = CreateMaterial(
        world,
        MoonRender::MaterialPreset::Metal,
        { 0.72f, 0.74f, 0.78f, 1.0f },
        0.28f,
        1.0f);

    world.CreateSky({ MoonRender::SkyType::Procedural, 1.0f });
    world.SetWeather({ MoonRender::WeatherType::Cloudy, 15.5f, 0.5f });
    world.SetWind({ { 0.8f, 0.0f, 0.2f }, 4.5f, 0.35f });
    world.CreateDirectionalLight({});
    world.CreatePointLight({ { 8.0f, 4.0f, -6.0f }, { 1.0f, 0.82f, 0.68f, 1.0f }, 12.0f, 20.0f, true });

    world.CreateGround({ { 180.0f, 180.0f }, groundMaterial });
    world.CreateFloor({ { 0.0f, 0.05f, 0.0f }, { 24.0f, 24.0f }, groundMaterial });
    world.CreateWall({ { -12.0f, 0.0f, -12.0f }, { 12.0f, 0.0f, -12.0f }, 4.0f, 0.35f, wallMaterial });
    world.CreateWall({ { 12.0f, 0.0f, -12.0f }, { 12.0f, 0.0f, 12.0f }, 4.0f, 0.35f, wallMaterial });
    world.CreateWall({ { 12.0f, 0.0f, 12.0f }, { -12.0f, 0.0f, 12.0f }, 4.0f, 0.35f, wallMaterial });
    world.CreateWall({ { -12.0f, 0.0f, 12.0f }, { -12.0f, 0.0f, -12.0f }, 4.0f, 0.35f, wallMaterial });
    world.CreateWaterPlane({ { 22.0f, 0.2f, 12.0f }, { 48.0f, 32.0f }, { 0.10f, 0.34f, 0.58f, 0.62f } });
    world.CreateTerrain({ 257, 1200.0f, 1200.0f, 140.0f, 4242, true, false, true });

    world.CreatePrimitive({ { -3.0f, 1.0f, 0.0f }, { 0.0f, 25.0f, 0.0f }, { 2.0f, 2.0f, 2.0f } }, propMaterial);
    world.CreatePrimitive({ { 3.5f, 0.75f, 2.0f }, { 0.0f, -18.0f, 0.0f }, { 1.5f, 1.5f, 1.5f } }, propMaterial);

    MoonRender::CameraHandle camera = world.CreateCamera({
        { 24.0f, 16.0f, -28.0f },
        { 0.0f, 3.0f, 0.0f },
        60.0f,
        0.1f,
        5000.0f
    });
    world.SetMainCamera(camera);

    auto previousTick = std::chrono::steady_clock::now();
    while (runtime.PollEvents()) {
        auto now = std::chrono::steady_clock::now();
        float deltaSeconds = std::chrono::duration<float>(now - previousTick).count();
        previousTick = now;

        runtime.Tick(deltaSeconds);
        runtime.Render(world);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    runtime.Shutdown();
    return 0;
}
