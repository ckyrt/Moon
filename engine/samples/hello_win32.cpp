#include <windows.h>
#include <chrono>
#include <thread>

#include "../core/Assets/AssetPaths.h"
#include "../core/Camera/FPSCameraController.h"
#include "../core/CSG/CSGOperations.h"
#include "../core/EngineCore.h"
#include "../core/Geometry/MeshGenerator.h"
#include "../core/Logging/Logger.h"
#include "../core/Mesh/Mesh.h"
#include "../core/Profiling/FPSCounter.h"
#include "../core/Scene/Light.h"
#include "../core/Scene/Material.h"
#include "../core/Scene/MeshRenderer.h"
#include "../core/Scene/Scene.h"
#include "../core/Scene/SceneNode.h"
#include "../core/Scene/Skybox.h"
#include "../environment/EnvironmentComponent.h"
#include "../environment/EnvironmentTypes.h"
#include "../render/diligent/DiligentRenderer.h"
#include "../render/IRenderer.h"
#include "../render/RenderCommon.h"
#include "../render/SceneRenderer.h"
#include "../terrain/TerrainComponent.h"
#include "../vehicle/VehicleComponent.h"
#include "../vehicle/VehicleFactory.h"
#include "../vehicle/VehicleInteractionService.h"
#include "HelloEngineImGui.h"
#include "TestScenes.h"

#include <memory>

static const wchar_t* kWndClass = L"UGC_Editor_WndClass";
static IRenderer* g_pRenderer = nullptr;
static Moon::PerspectiveCamera* g_pCamera = nullptr;

namespace {

Moon::EnvironmentComponent* FindEnvironmentComponent(Moon::Scene* scene)
{
    if (!scene) {
        return nullptr;
    }

    Moon::SceneNode* environmentNode = scene->FindNodeByName("Terrain Environment");
    if (!environmentNode) {
        return nullptr;
    }

    return environmentNode->GetComponent<Moon::EnvironmentComponent>();
}

Moon::TerrainComponent* FindTerrainComponent(Moon::Scene* scene)
{
    if (!scene) {
        return nullptr;
    }

    Moon::TerrainComponent* found = nullptr;
    scene->TraverseActive([&](Moon::SceneNode* node) {
        if (!node || found) {
            return;
        }

        if (Moon::TerrainComponent* terrain = node->GetComponent<Moon::TerrainComponent>()) {
            found = terrain;
        }
    });
    return found;
}

Moon::Vector3 ResolveVehicleSpawn(Moon::Scene* scene, const Moon::Vector3& preferredPosition)
{
    Moon::TerrainComponent* terrain = FindTerrainComponent(scene);
    if (!terrain) {
        MOON_LOG_WARN("HelloEngine", "No terrain component found for vehicle spawn search; using preferred spawn.");
        return preferredPosition;
    }

    Moon::Vector3 bestPosition = preferredPosition;
    Moon::Vector3 bestNormal(0.0f, 1.0f, 0.0f);
    float bestScore = -1.0f;

    for (int z = -8; z <= 8; ++z) {
        for (int x = -8; x <= 8; ++x) {
            const Moon::Vector3 candidate(
                preferredPosition.x + static_cast<float>(x) * 18.0f,
                preferredPosition.y,
                preferredPosition.z + static_cast<float>(z) * 18.0f);

            float groundHeight = 0.0f;
            Moon::Vector3 groundNormal(0.0f, 1.0f, 0.0f);
            if (!terrain->SampleWorldHeightAndNormal(candidate, groundHeight, groundNormal)) {
                continue;
            }

            const float flatness = groundNormal.y;
            const float distancePenalty =
                (std::abs(static_cast<float>(x)) + std::abs(static_cast<float>(z))) * 0.02f;
            const float score = flatness - distancePenalty;
            if (score > bestScore) {
                bestScore = score;
                bestNormal = groundNormal;
                bestPosition = Moon::Vector3(candidate.x, groundHeight, candidate.z);
            }
        }
    }

    MOON_LOG_INFO(
        "HelloEngine",
        "Resolved vehicle spawn from preferred (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f) normal=(%.2f, %.2f, %.2f) score=%.3f",
        preferredPosition.x,
        preferredPosition.y,
        preferredPosition.z,
        bestPosition.x,
        bestPosition.y,
        bestPosition.z,
        bestNormal.x,
        bestNormal.y,
        bestNormal.z,
        bestScore);

    return bestPosition;
}

const wchar_t* WeatherLabel(Moon::WeatherType weather)
{
    switch (weather) {
    case Moon::WeatherType::Clear:
        return L"Clear";
    case Moon::WeatherType::Cloudy:
        return L"Cloudy";
    case Moon::WeatherType::Rain:
        return L"Rain";
    case Moon::WeatherType::Snow:
        return L"Snow";
    case Moon::WeatherType::Fog:
        return L"Fog";
    case Moon::WeatherType::Storm:
        return L"Storm";
    }

    return L"Unknown";
}

const wchar_t* TimeLabel(float hours)
{
    if (hours < 6.0f) {
        return L"Night";
    }
    if (hours < 8.0f) {
        return L"Dawn";
    }
    if (hours < 16.5f) {
        return L"Day";
    }
    if (hours < 19.5f) {
        return L"Sunset";
    }
    return L"Night";
}

void SetWeatherPreset(Moon::EnvironmentComponent* environment, Moon::WeatherType weather)
{
    if (!environment) {
        return;
    }

    environment->SetWeather(weather, 1.25f);
    MOON_LOG_INFO("HelloEngine", "Weather preset requested: %ls", WeatherLabel(weather));
}

void SetFixedTimePreset(Moon::EnvironmentComponent* environment, float hours)
{
    if (!environment) {
        return;
    }

    Moon::EnvironmentProfile profile = environment->GetProfile();
    profile.lockToFixedTime = true;
    profile.enableDayNightCycle = false;
    profile.fixedTimeHours = hours;
    environment->SetProfile(profile);
    environment->SetTimeOfDay(hours);

    Moon::EnvironmentState state = environment->GetState();
    state.timeOfDay.paused = true;
    state.timeOfDay.timeScale = 0.0f;
    environment->GetSystem().SetState(state);
}

} // namespace

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (HelloEngineImGui::HandleWin32Message(hWnd, msg, wParam, lParam)) {
        return true;
    }

    switch (msg) {
    case WM_SIZE:
        if (g_pRenderer && wParam != SIZE_MINIMIZED) {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            g_pRenderer->Resize(width, height);
            if (g_pCamera && height > 0) {
                g_pCamera->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
            }
        }
        break;
    case WM_PAINT:
        ValidateRect(hWnd, nullptr);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
        SetCurrentDirectoryW(exePath);
    }

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWndClass;
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, 1280, 720 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(
        0,
        kWndClass,
        L"Hello Engine (DiligentRenderer)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    Moon::Core::Logger::Init();

    EngineCore engine;
    engine.Initialize();

    Moon::PerspectiveCamera* camera = engine.GetCamera();
    Moon::InputSystem* inputSystem = engine.GetInputSystem();
    inputSystem->SetWindowHandle(hwnd);

    g_pCamera = camera;

    RECT cr;
    GetClientRect(hwnd, &cr);
    float width = static_cast<float>(cr.right - cr.left);
    float height = static_cast<float>(cr.bottom - cr.top);
    camera->SetAspectRatio(width / height);

    Moon::FPSCameraController cameraController(camera, inputSystem);
    cameraController.SetMoveSpeed(34.0f);
    cameraController.SetSprintMultiplier(5.0f);
    cameraController.SetMouseSensitivity(20.0f);

    DiligentRenderer renderer;
    g_pRenderer = &renderer;

    RenderInitParams params{};
    params.windowHandle = hwnd;
    params.width = cr.right - cr.left;
    params.height = cr.bottom - cr.top;

    if (!renderer.Initialize(params)) {
        MessageBoxA(hwnd, "Failed to initialize DiligentRenderer!", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    HelloEngineImGui::Initialize(hwnd, &renderer);

    TestScenes::TestTerrain(&engine);

    Moon::Scene* scene = engine.GetScene();
    Moon::SceneNode* mainLightNode = scene ? scene->FindNodeByName("Environment Sun") : nullptr;
    Moon::EnvironmentComponent* environment = FindEnvironmentComponent(scene);
    const Moon::Vector3 kPreferredVehicleSpawn(35.0f, 4.5f, 28.0f);
    const Moon::Vector3 kVehicleSpawn = ResolveVehicleSpawn(scene, kPreferredVehicleSpawn);
    Moon::VehicleFactory::CreateBuggy(scene, engine.GetPhysicsSystem(), kVehicleSpawn);
    camera->SetPosition(Moon::Vector3(kVehicleSpawn.x, kVehicleSpawn.y + 3.5f, kVehicleSpawn.z - 5.5f));
    camera->LookAt(Moon::Vector3(kVehicleSpawn.x, kVehicleSpawn.y + 1.5f, kVehicleSpawn.z));
    auto vehicleInteraction = std::make_unique<Moon::VehicleInteractionService>(
        scene,
        camera,
        inputSystem,
        &cameraController);

    constexpr float kLightYawDegPerSec = 60.0f;

    bool running = true;
    MSG msg;
    auto prev = std::chrono::high_resolution_clock::now();

    Moon::FPSCounter fpsCounter(60);
    auto lastTitleUpdate = prev;

    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        auto now = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(now - prev).count();
        prev = now;

        fpsCounter.Update(static_cast<float>(dt));

        auto timeSinceUpdate = std::chrono::duration<double>(now - lastTitleUpdate).count();
        if (timeSinceUpdate >= 0.5) {
            wchar_t title[256];
            const Moon::EnvironmentState* environmentState = environment ? &environment->GetState() : nullptr;
            const wchar_t* weatherText =
                environmentState ? WeatherLabel(environmentState->weather.target) : L"NoEnv";
            const wchar_t* timeText =
                environmentState ? TimeLabel(environmentState->timeOfDay.timeOfDayHours) : L"NoEnv";
            swprintf_s(
                title,
                L"Moon Engine - HelloEngine | FPS: %.1f | Frame Time: %.2f ms | F7 Force Enter | F8 Focus Vehicle | RMB+WASD Fly | E Enter/Exit Vehicle | %ls | %ls",
                fpsCounter.GetFPS(),
                fpsCounter.GetFrameTimeMs(),
                weatherText,
                timeText);
            SetWindowTextW(hwnd, title);
            lastTitleUpdate = now;
        }

        cameraController.Update(static_cast<float>(dt));
        vehicleInteraction->BeginFrame();
        engine.Tick(dt);
        if (scene) {
            scene->TraverseActive([](Moon::SceneNode* node) {
                if (!node) {
                    return;
                }

                if (Moon::VehicleComponent* vehicle = node->GetComponent<Moon::VehicleComponent>()) {
                    vehicle->PostPhysicsSync();
                }
            });
        }
        vehicleInteraction->EndFrame(static_cast<float>(dt));

        if (inputSystem && inputSystem->IsKeyPressed(Moon::KeyCode::F8)) {
            camera->SetPosition(Moon::Vector3(kVehicleSpawn.x, kVehicleSpawn.y + 3.5f, kVehicleSpawn.z - 5.5f));
            camera->LookAt(Moon::Vector3(kVehicleSpawn.x, kVehicleSpawn.y + 1.5f, kVehicleSpawn.z));
        }

        if (inputSystem && mainLightNode) {
            if (environment) {
                if (inputSystem->IsKeyPressed(Moon::KeyCode::D1)) {
                    SetWeatherPreset(environment, Moon::WeatherType::Clear);
                }
                if (inputSystem->IsKeyPressed(Moon::KeyCode::D2)) {
                    SetWeatherPreset(environment, Moon::WeatherType::Cloudy);
                }
                if (inputSystem->IsKeyPressed(Moon::KeyCode::D3)) {
                    SetWeatherPreset(environment, Moon::WeatherType::Rain);
                }
                if (inputSystem->IsKeyPressed(Moon::KeyCode::D4)) {
                    SetWeatherPreset(environment, Moon::WeatherType::Snow);
                }
                if (inputSystem->IsKeyPressed(Moon::KeyCode::D5)) {
                    SetWeatherPreset(environment, Moon::WeatherType::Fog);
                }
                if (inputSystem->IsKeyPressed(Moon::KeyCode::D6)) {
                    SetWeatherPreset(environment, Moon::WeatherType::Storm);
                }
                if (inputSystem->IsKeyPressed(Moon::KeyCode::D7)) {
                    SetFixedTimePreset(environment, 6.1f);
                }
                if (inputSystem->IsKeyPressed(Moon::KeyCode::D8)) {
                    SetFixedTimePreset(environment, 11.8f);
                }
                if (inputSystem->IsKeyPressed(Moon::KeyCode::D9)) {
                    SetFixedTimePreset(environment, 18.1f);
                }
                if (inputSystem->IsKeyPressed(Moon::KeyCode::D0)) {
                    SetFixedTimePreset(environment, 22.2f);
                }
            }

            float yawDelta = 0.0f;
            if (inputSystem->IsKeyDown(Moon::KeyCode::Left)) {
                yawDelta -= kLightYawDegPerSec * static_cast<float>(dt);
            }
            if (inputSystem->IsKeyDown(Moon::KeyCode::Right)) {
                yawDelta += kLightYawDegPerSec * static_cast<float>(dt);
            }
            if (yawDelta != 0.0f) {
                mainLightNode->GetTransform()->Rotate(Moon::Vector3(0.0f, yawDelta, 0.0f), true);
            }
        }

        HelloEngineImGui::BeginFrame();

        renderer.BeginFrame();
        Moon::SceneRendererUtils::RenderScene(&renderer, scene, camera);
        HelloEngineImGui::RenderDebugGrid(camera);
        HelloEngineImGui::RenderUI(camera, &fpsCounter);
        renderer.EndFrame();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    HelloEngineImGui::Shutdown();
    renderer.Shutdown();
    g_pRenderer = nullptr;
    g_pCamera = nullptr;
    engine.Shutdown();
    Moon::Core::Logger::Shutdown();

    return 0;
}
