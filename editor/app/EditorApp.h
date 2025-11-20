// ============================================================================
// EditorApp.h - Moon Engine 编辑器主程序头文件
// ============================================================================
#pragma once

#include <Windows.h>
#include <string>

// Forward declarations
class EngineCore;
class DiligentRenderer;
class EditorBridge;
namespace Diligent { class ImGuiImplWin32; }
namespace Moon { 
    class FPSCameraController; 
    class SceneNode;
    class Quaternion;
    class Matrix4x4;
    class Vector3;
    class PhysicsSystem;
}
namespace ImGuizmo {
    enum OPERATION : int;
    enum MODE : int;
}

// ============================================================================
// 全局变量声明（定义在 EditorApp.cpp）
// ============================================================================
extern EngineCore* g_Engine;
extern DiligentRenderer* g_Renderer;
extern Moon::PhysicsSystem* g_PhysicsSystem;
extern Moon::FPSCameraController* g_CameraController;
extern Diligent::ImGuiImplWin32* g_ImGuiWin32;
extern HWND g_EngineWindow;
extern Moon::SceneNode* g_SelectedObject;
extern EditorBridge* g_EditorBridge;

// Gizmo 状态
extern ImGuizmo::OPERATION g_GizmoOperation;
extern ImGuizmo::MODE g_GizmoMode;
extern bool g_WasUsingGizmo;
extern Moon::Quaternion g_LastRotation;
extern Moon::Matrix4x4 g_GizmoMatrix;

// Viewport 矩形
struct ViewportRect {
    int x = 0, y = 0;
    int width = 800, height = 600;
    bool updated = false;
};
extern ViewportRect g_ViewportRect;

// ============================================================================
// 公共接口函数
// ============================================================================

// 选中对象访问
void SetSelectedObject(Moon::SceneNode* node);
Moon::SceneNode* GetSelectedObject();

// Gizmo 模式设置
void SetGizmoOperation(const std::string& mode);
void SetGizmoMode(const std::string& mode);

// ============================================================================
// 模块函数声明（在各自的 .cpp 文件中实现）
// ============================================================================

// EditorApp_Init.cpp
void InitEngine(EngineCore*& enginePtr);
HWND InitCEF(HINSTANCE hInstance, EditorBridge& bridge);
bool InitEngineWindow(HINSTANCE hInstance);
bool InitRenderer();
void InitImGui();
void InitSceneObjects(EngineCore* engine);

// EditorApp_WndProc.cpp
LRESULT CALLBACK EngineWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// EditorApp_Gizmo.cpp
void RenderAndApplyGizmo(EngineCore* engine, EditorBridge& bridge);

// EditorApp_Render.cpp
void RenderScene(EngineCore* engine, DiligentRenderer* renderer);

// EditorApp_Utils.cpp
HWND FindCefHtmlRenderWindow(HWND cefWindow);
Moon::Vector3 ExtractScale(const Moon::Matrix4x4& matrix);
Moon::Matrix4x4 RemoveScale(const Moon::Matrix4x4& matrix, const Moon::Vector3& scale);
Moon::Quaternion StabilizeQuaternion(const Moon::Quaternion& current, const Moon::Quaternion& previous);

// EditorApp_Cleanup.cpp
void CleanupResources();
