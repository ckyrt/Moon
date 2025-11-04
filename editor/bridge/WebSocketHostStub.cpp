#include "NativeHost.h"
#include <cstdio>

// Placeholder stub. Replace with a real WebSocket server (uWebSockets/httplib/etc.).
class WebSocketHostStub : public NativeHost {
public:
    void Start(MessageHandler handler) override {
        (void)handler;
        std::puts("[EditorBridge] WebSocketHostStub started (no real IPC).");
    }
    void SendToUI(const std::string& msg) override {
        std::printf("[EditorBridge] SendToUI (stub): %s\n", msg.c_str());
    }
};
// (Factory or registration could be added later.)
