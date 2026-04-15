#include "ObjectCopilotAgentClient.h"
#include "ObjectCopilotPreview.h"
#include "ObjectCopilotSession.h"

#include "../../engine/core/Camera/FPSCameraController.h"
#include "../../engine/core/EngineCore.h"
#include "../../engine/core/Logging/Logger.h"
#include "../../engine/core/Profiling/FPSCounter.h"
#include "../../engine/render/RenderCommon.h"
#include "../../engine/render/SceneRenderer.h"
#include "../../engine/render/diligent/DiligentRenderer.h"

#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "imgui.h"
#include "ImGuiImplWin32.hpp"
#include "ImGuizmo.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

constexpr wchar_t kWindowClassName[] = L"MoonObjectCopilotWindow";
constexpr size_t kLargeTextBufferSize = 64 * 1024;

struct UiLayout {
    ImVec2 leftPos;
    ImVec2 leftSize;
    ImVec2 previewPos;
    ImVec2 previewSize;
    ImVec2 advancedPos;
    ImVec2 advancedSize;
};

struct AppState {
    EngineCore engine;
    DiligentRenderer renderer;
    Diligent::ImGuiImplWin32* imgui = nullptr;
    Moon::Tooling::ObjectCopilotSession session;
    Moon::Tooling::ObjectCopilotPreview* preview = nullptr;
    std::string promptDraft;
    std::string patchDraft;
    std::string objectJsonDraft;
    std::string statusText;
    std::string errorText;
    RECT previewViewport = { 0, 0, 0, 0 };
    bool showAdvanced = false;
};

AppState* g_app = nullptr;

void ApplyPatchDraft(AppState& app);
void RefreshPreviewFromSession(AppState& app, const std::string& successMessage);

void ConfigureImGuiFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const std::filesystem::path fontPath = "C:\\Windows\\Fonts\\msyh.ttc";
    if (std::filesystem::exists(fontPath)) {
        ImFontConfig fontConfig = {};
        fontConfig.OversampleH = 2;
        fontConfig.OversampleV = 2;
        fontConfig.PixelSnapH = false;
        io.Fonts->AddFontFromFileTTF(
            fontPath.string().c_str(),
            18.0f,
            &fontConfig,
            io.Fonts->GetGlyphRangesChineseFull());
    }

    if (io.Fonts->Fonts.empty()) {
        io.Fonts->AddFontDefault();
    }
}

UiLayout BuildUiLayout(float displayWidth, float displayHeight) {
    const float padding = 12.0f;
    const float spacing = 12.0f;
    const float leftWidth = 420.0f;

    UiLayout layout = {};
    layout.leftPos = ImVec2(padding, padding);
    layout.leftSize = ImVec2(leftWidth, std::max(120.0f, displayHeight - padding * 2.0f));

    const float rightX = layout.leftPos.x + layout.leftSize.x + spacing;
    const float rightWidth = std::max(320.0f, displayWidth - rightX - padding);

    layout.previewPos = ImVec2(rightX, padding);
    layout.previewSize = ImVec2(rightWidth, std::max(220.0f, displayHeight - padding * 2.0f));
    layout.advancedPos = ImVec2(rightX + 24.0f, padding + 24.0f);
    layout.advancedSize = ImVec2(std::min(760.0f, rightWidth - 48.0f), std::max(280.0f, displayHeight - 120.0f));
    return layout;
}

RECT MakePreviewViewportRect(const UiLayout& layout) {
    RECT rect = {};
    rect.left = static_cast<LONG>(layout.previewPos.x);
    rect.top = static_cast<LONG>(layout.previewPos.y);
    rect.right = static_cast<LONG>(layout.previewPos.x + layout.previewSize.x);
    rect.bottom = static_cast<LONG>(layout.previewPos.y + layout.previewSize.y);
    return rect;
}

std::string MakeDefaultPatchDraft() {
    return R"({
  "summary": "Increase the default size parameter a little",
  "operations": [
    {
      "op": "replace",
      "path": "/object_blueprint/parameters/size/default",
      "value": 140.0
    }
  ]
})";
}

bool CopyStringToBuffer(const std::string& source, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return false;
    }

    const size_t copyLength = std::min(bufferSize - 1, source.size());
    memcpy(buffer, source.data(), copyLength);
    buffer[copyLength] = '\0';
    return source.size() < bufferSize;
}

std::string ChatRoleLabel(Moon::Tooling::ChatRole role) {
    switch (role) {
    case Moon::Tooling::ChatRole::User:
        return "User";
    case Moon::Tooling::ChatRole::Assistant:
        return "Assistant";
    case Moon::Tooling::ChatRole::System:
    default:
        return "System";
    }
}

std::string GetAgentServiceUrl(const AppState& app) {
    const auto& worldState = app.session.GetWorldState();
    if (worldState.contains("agent") &&
        worldState["agent"].is_object() &&
        worldState["agent"].contains("service_url") &&
        worldState["agent"]["service_url"].is_string()) {
        return worldState["agent"]["service_url"].get<std::string>();
    }
    return "http://127.0.0.1:8008";
}

std::string ChatRoleForJson(Moon::Tooling::ChatRole role) {
    switch (role) {
    case Moon::Tooling::ChatRole::User:
        return "user";
    case Moon::Tooling::ChatRole::Assistant:
        return "assistant";
    case Moon::Tooling::ChatRole::System:
    default:
        return "system";
    }
}

nlohmann::json BuildConversationJson(const AppState& app) {
    nlohmann::json items = nlohmann::json::array();
    for (const auto& message : app.session.GetMessages()) {
        items.push_back({
            {"role", ChatRoleForJson(message.role)},
            {"text", message.text}
        });
    }
    return items;
}

void UpdateDraftsFromSession(AppState& app) {
    app.objectJsonDraft = app.session.GetObjectBlueprintPrettyJson();
}

void RefreshPreviewFromSession(AppState& app, const std::string& successMessage) {
    std::string previewError;
    if (app.preview->RebuildFromObjectJson(app.session.GetObjectBlueprintPrettyJson(), previewError)) {
        app.statusText = successMessage;
        app.errorText.clear();
    } else {
        app.errorText = previewError;
    }
}

void SubmitPrompt(AppState& app) {
    if (app.promptDraft.empty()) {
        return;
    }

    const std::string prompt = app.promptDraft;
    app.session.AddMessage(Moon::Tooling::ChatRole::User, app.promptDraft);

    Moon::Tooling::AgentPatchResponse agentResponse;
    std::string requestError;
    if (!Moon::Tooling::ObjectCopilotAgentClient::RequestPatch(
            GetAgentServiceUrl(app),
            app.session.GetWorldState(),
            BuildConversationJson(app),
            prompt,
            agentResponse,
            requestError)) {
        app.errorText = requestError;
        app.statusText = "Agent request failed.";
        return;
    }

    app.patchDraft = agentResponse.patchJson;
    app.session.AddMessage(
        Moon::Tooling::ChatRole::Assistant,
        agentResponse.summary.empty() ? "Agent returned a patch." : agentResponse.summary);
    app.statusText = "Agent returned a patch. Applying it locally now.";
    app.errorText.clear();
    app.promptDraft.clear();

    ApplyPatchDraft(app);
}

void ApplyPatchDraft(AppState& app) {
    std::string error;
    if (!app.session.ApplyPatchJson(app.patchDraft, error)) {
        app.errorText = error;
        return;
    }

    app.session.AddMessage(Moon::Tooling::ChatRole::Assistant, "Applied a local structured patch to the world state.");
    UpdateDraftsFromSession(app);
    RefreshPreviewFromSession(app, "Patch applied and preview rebuilt.");
}

void ApplyObjectJsonDraft(AppState& app) {
    std::string error;
    if (!app.session.SetObjectBlueprintFromJson(app.objectJsonDraft, error)) {
        app.errorText = error;
        return;
    }

    RefreshPreviewFromSession(app, "Object blueprint updated and preview rebuilt.");
}

void ResetSession(AppState& app) {
    app.session.ResetToDefaultObject();
    app.session.AddMessage(Moon::Tooling::ChatRole::System, "Session reset to the default prototype object.");
    app.promptDraft.clear();
    app.patchDraft = MakeDefaultPatchDraft();
    UpdateDraftsFromSession(app);
    RefreshPreviewFromSession(app, "Session reset and preview rebuilt.");
}

void DrawLeftPanel(AppState& app) {
    ImGui::TextUnformatted("Object Copilot");
    ImGui::TextWrapped("这里就是主对话区。你直接在下面输入一句话，比如“做一个金属圆柱，稍微大一点”，右边会实时更新预览。");
    ImGui::Separator();

    ImGui::TextUnformatted("Conversation");
    ImGui::BeginChild("ConversationHistory", ImVec2(0.0f, 180.0f), true);
    for (const auto& message : app.session.GetMessages()) {
        ImGui::Text("%s:", ChatRoleLabel(message.role).c_str());
        ImGui::PushTextWrapPos();
        ImGui::TextUnformatted(message.text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::TextUnformatted("Ask");
    char promptBuffer[4096];
    CopyStringToBuffer(app.promptDraft, promptBuffer, sizeof(promptBuffer));
    if (ImGui::InputTextMultiline("##ChatInput", promptBuffer, sizeof(promptBuffer), ImVec2(-1.0f, 110.0f))) {
        app.promptDraft = promptBuffer;
    }

    if (ImGui::Button("Send", ImVec2(150.0f, 0.0f))) {
        SubmitPrompt(app);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(120.0f, 0.0f))) {
        ResetSession(app);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Advanced", &app.showAdvanced);

    ImGui::Separator();
    if (!app.errorText.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", app.errorText.c_str());
    } else if (!app.statusText.empty()) {
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.65f, 1.0f), "%s", app.statusText.c_str());
    }
}

void DrawAdvancedPanel(AppState& app) {
    ImGui::TextUnformatted("Advanced Tools");
    ImGui::TextWrapped("这些是开发和调试用的，不是主要聊天入口。主交互在左边的 Ask 输入框。");
    ImGui::Separator();

    if (ImGui::BeginTabBar("AdvancedTabs")) {
        if (ImGui::BeginTabItem("Patch")) {
            std::vector<char> patchBuffer(kLargeTextBufferSize);
            CopyStringToBuffer(app.patchDraft, patchBuffer.data(), patchBuffer.size());
            if (ImGui::InputTextMultiline("Patch JSON", patchBuffer.data(), patchBuffer.size(), ImVec2(-1.0f, 320.0f))) {
                app.patchDraft = patchBuffer.data();
            }
            if (ImGui::Button("Apply Patch", ImVec2(150.0f, 0.0f))) {
                ApplyPatchDraft(app);
            }
            ImGui::SameLine();
            if (ImGui::Button("Rebuild Preview", ImVec2(150.0f, 0.0f))) {
                RefreshPreviewFromSession(app, "Preview rebuilt from current object state.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Object")) {
            std::vector<char> objectBuffer(kLargeTextBufferSize);
            CopyStringToBuffer(app.objectJsonDraft, objectBuffer.data(), objectBuffer.size());
            if (ImGui::InputTextMultiline("Object Blueprint", objectBuffer.data(), objectBuffer.size(), ImVec2(-1.0f, 320.0f))) {
                app.objectJsonDraft = objectBuffer.data();
            }
            if (ImGui::Button("Apply Object JSON", ImVec2(170.0f, 0.0f))) {
                ApplyObjectJsonDraft(app);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("World State")) {
            std::string worldStateJson = app.session.GetWorldStatePrettyJson();
            std::vector<char> worldStateBuffer(kLargeTextBufferSize);
            CopyStringToBuffer(worldStateJson, worldStateBuffer.data(), worldStateBuffer.size());
            ImGui::InputTextMultiline("World State", worldStateBuffer.data(), worldStateBuffer.size(), ImVec2(-1.0f, 360.0f), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void DrawPreviewPanel(AppState& app, Moon::FPSCounter& fpsCounter) {
    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();
    ImGui::TextDisabled("| FPS %.1f | %.2f ms", fpsCounter.GetFPS(), fpsCounter.GetFrameTimeMs());
    ImGui::Separator();
    ImGui::TextWrapped("This is the live preview texture rendered by Diligent into an offscreen target and shown inside ImGui.");
    const Moon::Tooling::ObjectCopilotPreview::PreviewStats& stats = app.preview->GetPreviewStats();
    if (stats.valid) {
        ImGui::Text("Size W/H/D: %.2f / %.2f / %.2f", stats.size.x, stats.size.y, stats.size.z);
        ImGui::Text("Min: %.2f, %.2f, %.2f", stats.min.x, stats.min.y, stats.min.z);
        ImGui::Text("Max: %.2f, %.2f, %.2f", stats.max.x, stats.max.y, stats.max.z);
        ImGui::TextUnformatted("Ground = Y 0, axis colors: X red / Y green / Z blue");
    }
    ImGui::Spacing();

    Diligent::ITextureView* previewSrv = app.renderer.GetPreviewSRV();
    ImVec2 imageSize = ImGui::GetContentRegionAvail();
    imageSize.x = std::max(1.0f, imageSize.x);
    imageSize.y = std::max(1.0f, imageSize.y);

    if (previewSrv) {
        ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(previewSrv)), imageSize);
    } else {
        ImGui::Dummy(imageSize);
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, IM_COL32(24, 28, 34, 255));
        drawList->AddRect(min, max, IM_COL32(90, 96, 110, 255));
        drawList->AddText(ImVec2(min.x + 12.0f, min.y + 12.0f), IM_COL32(220, 220, 220, 255), "Preview texture unavailable");
    }
}

void RenderUi(AppState& app, Moon::FPSCounter& fpsCounter, float displayWidth, float displayHeight) {
    app.imgui->NewFrame(static_cast<Diligent::Uint32>(displayWidth),
                        static_cast<Diligent::Uint32>(displayHeight),
                        Diligent::SURFACE_TRANSFORM_OPTIMAL);
    ImGuizmo::BeginFrame();

    const UiLayout layout = BuildUiLayout(displayWidth, displayHeight);
    app.previewViewport = MakePreviewViewportRect(layout);

    const ImGuiWindowFlags fixedWindowFlags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    ImGui::SetNextWindowPos(layout.leftPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.leftSize, ImGuiCond_Always);
    if (ImGui::Begin("Chat", nullptr, fixedWindowFlags)) {
        DrawLeftPanel(app);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(layout.previewPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.previewSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.08f);
    if (ImGui::Begin("Viewport", nullptr, fixedWindowFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        DrawPreviewPanel(app, fpsCounter);
    }
    ImGui::End();

    if (app.showAdvanced) {
        ImGui::SetNextWindowPos(layout.advancedPos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(layout.advancedSize, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Advanced", &app.showAdvanced)) {
            DrawAdvancedPanel(app);
        }
        ImGui::End();
    }

    ImGui::Render();
    app.imgui->Render(app.renderer.GetContext());
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_app && g_app->imgui && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return 1;
    }

    switch (msg) {
    case WM_SIZE:
        if (g_app && wParam != SIZE_MINIMIZED) {
            const UINT width = LOWORD(lParam);
            const UINT height = HIWORD(lParam);
            g_app->renderer.Resize(width, height);
            if (g_app->engine.GetCamera() && width > 0 && height > 0) {
                const UiLayout layout = BuildUiLayout(static_cast<float>(width), static_cast<float>(height));
                g_app->previewViewport = MakePreviewViewportRect(layout);
                const float previewWidth = std::max(1.0f, layout.previewSize.x);
                const float previewHeight = std::max(1.0f, layout.previewSize.y);
                g_app->engine.GetCamera()->SetAspectRatio(previewWidth / previewHeight);
            }
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
        SetCurrentDirectoryW(exePath);
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClassName;
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, 1480, 900 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        L"Moon Object Copilot",
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

    AppState app;
    g_app = &app;
    app.patchDraft = MakeDefaultPatchDraft();
    app.statusText = "Prototype ready. Record a prompt, apply a patch, or edit the current object blueprint.";
    app.engine.Initialize();

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

    Moon::InputSystem* inputSystem = app.engine.GetInputSystem();
    inputSystem->SetWindowHandle(hwnd);
    {
        const UiLayout initialLayout = BuildUiLayout(
            static_cast<float>(clientRect.right - clientRect.left),
            static_cast<float>(clientRect.bottom - clientRect.top));
        app.previewViewport = MakePreviewViewportRect(initialLayout);
        app.engine.GetCamera()->SetAspectRatio(
            std::max(1.0f, initialLayout.previewSize.x) /
            std::max(1.0f, initialLayout.previewSize.y));
    }

    Moon::FPSCameraController cameraController(app.engine.GetCamera(), inputSystem);
    cameraController.SetMoveSpeed(8.0f);
    cameraController.SetSprintMultiplier(3.0f);
    cameraController.SetMouseSensitivity(20.0f);

    RenderInitParams initParams = {};
    initParams.windowHandle = hwnd;
    initParams.width = clientRect.right - clientRect.left;
    initParams.height = clientRect.bottom - clientRect.top;
    if (!app.renderer.Initialize(initParams)) {
        MessageBoxA(hwnd, "Failed to initialize renderer.", "Object Copilot", MB_OK | MB_ICONERROR);
        return -1;
    }

    Diligent::ImGuiDiligentCreateInfo imguiCreateInfo = {};
    imguiCreateInfo.pDevice = app.renderer.GetDevice();
    imguiCreateInfo.BackBufferFmt = app.renderer.GetSwapChain()->GetDesc().ColorBufferFormat;
    imguiCreateInfo.DepthBufferFmt = app.renderer.GetSwapChain()->GetDesc().DepthBufferFormat;
    app.imgui = new Diligent::ImGuiImplWin32(imguiCreateInfo, hwnd);
    ConfigureImGuiFonts();
    app.imgui->InvalidateDeviceObjects();
    app.imgui->CreateDeviceObjects();

    Moon::Tooling::ObjectCopilotPreview preview(&app.engine);
    app.preview = &preview;
    UpdateDraftsFromSession(app);
    RefreshPreviewFromSession(app, "Loaded the default object prototype.");

    MSG msg = {};
    bool running = true;
    auto previousTick = std::chrono::high_resolution_clock::now();
    Moon::FPSCounter fpsCounter(60);

    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running) {
            break;
        }

        const auto now = std::chrono::high_resolution_clock::now();
        const float dt = std::chrono::duration<float>(now - previousTick).count();
        previousTick = now;
        fpsCounter.Update(dt);

        app.engine.Tick(dt);
        cameraController.Update(dt);

        RECT drawRect;
        GetClientRect(hwnd, &drawRect);
        const float displayWidth = static_cast<float>(drawRect.right - drawRect.left);
        const float displayHeight = static_cast<float>(drawRect.bottom - drawRect.top);
        const UiLayout layout = BuildUiLayout(displayWidth, displayHeight);
        app.previewViewport = MakePreviewViewportRect(layout);

        const uint32_t previewWidth = static_cast<uint32_t>(std::max(1.0f, layout.previewSize.x - 16.0f));
        const uint32_t previewHeight = static_cast<uint32_t>(std::max(1.0f, layout.previewSize.y - 64.0f));
        app.engine.GetCamera()->SetAspectRatio(
            static_cast<float>(previewWidth) /
            static_cast<float>(previewHeight));
        if (app.renderer.BeginPreviewPass(previewWidth, previewHeight)) {
            Moon::SceneRendererUtils::RenderScene(&app.renderer, app.engine.GetScene(), app.engine.GetCamera());
            app.renderer.EndPreviewPass();
        }

        app.renderer.BeginFrame();

        RenderUi(app,
                 fpsCounter,
                 displayWidth,
                 displayHeight);
        app.renderer.EndFrame();
    }

    preview.Clear();
    delete app.imgui;
    app.imgui = nullptr;

    app.renderer.Shutdown();
    app.engine.Shutdown();
    Moon::Core::Logger::Shutdown();
    g_app = nullptr;
    return 0;
}
