// EditorApp.cpp - Moon Engine 编辑器主程序
// 集成 CEF 浏览器，显示 React 编辑器界面

#include "EditorBridge.h"
#include "cef/CefApp.h"     // ✅ 必须包含这个（用于 CefAppHandler）
#include <Windows.h>
#include <iostream>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    // ✅【必须 1】让 CEF 处理子进程，否则会无限创建 EditorApp.exe
    CefMainArgs main_args(hInstance);
    CefRefPtr<CefAppHandler> app(new CefAppHandler());
    int exit_code = CefExecuteProcess(main_args, app.get(), nullptr);
    if (exit_code >= 0) {
        return exit_code;   // ✅ 子进程执行完毕后直接退出，不进入主程序逻辑
    }

    // ✅【必须 2】主进程继续往下运行
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    std::cout << "========================================" << std::endl;
    std::cout << "  Moon Engine Editor - CEF Version" << std::endl;
    std::cout << "========================================" << std::endl;

    // 创建编辑器桥接
    EditorBridge bridge;

    // 初始化 CEF
    if (!bridge.Initialize(hInstance)) {
        std::cerr << "Failed to initialize EditorBridge!" << std::endl;
        MessageBoxA(nullptr, "Failed to initialize CEF!", "Error", MB_ICONERROR);
        return -1;
    }

    if (!bridge.CreateEditorWindow("")) {
        std::cerr << "Failed to create editor window!" << std::endl;
        return -1;
    }

    std::cout << "Editor window created, entering message loop..." << std::endl;

    // 主消息循环
    MSG msg;
    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                goto cleanup;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        bridge.DoMessageLoopWork();

        if (bridge.IsClosing()) {
            break;
        }

        Sleep(1);
    }

cleanup:
    std::cout << "Shutting down..." << std::endl;
    bridge.Shutdown();

    FreeConsole();
    return 0;
}
