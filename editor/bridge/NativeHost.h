#pragma once
#include <string>
#include <functional>

class NativeHost {
public:
    using MessageHandler = std::function<void(const std::string&)>;
    virtual ~NativeHost() = default;
    virtual void Start(MessageHandler handler) = 0;
    virtual void SendToUI(const std::string& msg) = 0;
};
