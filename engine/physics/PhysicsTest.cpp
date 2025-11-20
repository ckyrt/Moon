// 临时测试文件 - 验证 Jolt 集成是否正确
#include "PhysicsSystem.h"
#include <iostream>

int main() {
    Moon::PhysicsSystem physics;
    physics.Init();
    
    std::cout << "Physics system initialized successfully!" << std::endl;
    
    physics.Shutdown();
    return 0;
}
