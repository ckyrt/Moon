// EditorApp.cpp - Moon Engine 编辑器主程序
// 集成 CEF 浏览器，显示 React 编辑器界面

#include "EditorBridge.h"
#include <Windows.h>
#include <iostream>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    // 重定向输出到控制台（方便调试）
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

    // 创建编辑器窗口
    // 注意：这里先加载一个测试 HTML，后续会改为加载 React 应用
    std::string url = "data:text/html,<html><body><h1>Hello from CEF!</h1><p>Moon Engine Editor is running.</p></body></html>";
    
    if (!bridge.CreateEditorWindow(url)) {
        std::cerr << "Failed to create editor window!" << std::endl;
        return -1;
    }

    std::cout << "Editor window created, entering message loop..." << std::endl;

    // 主消息循环
    MSG msg;
    while (true) {
        // 处理 Windows 消息
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                goto cleanup;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // 处理 CEF 消息（非阻塞）
        bridge.DoMessageLoopWork();

        // 检查是否正在关闭
        if (bridge.IsClosing()) {
            break;
        }

        // 避免 CPU 占用过高
        Sleep(1);
    }

cleanup:
    std::cout << "Shutting down..." << std::endl;
    bridge.Shutdown();
    
    FreeConsole();
    return 0;
}
