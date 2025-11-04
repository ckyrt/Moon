# Vehicle Module

## 职责 (Responsibilities)
- 轮式车辆物理模拟
- 悬挂系统 (Suspension)
- 油门、制动、转向控制
- 车辆AI和路径规划
- 车辆定制和配置

## 接口文件 (Interface Files)
- `IVehicle.h` - 车辆系统接口
- `Vehicle.h` - 车辆组件
- `Wheel.h` - 轮子组件
- `Engine.h` - 发动机组件
- `Suspension.h` - 悬挂组件

## 依赖 (Dependencies)
- engine/physics (车辆物理)
- engine/core (ECS系统)
- engine/terrain (道路交互)

## AI 生成指导
这个模块需要实现：
1. 车辆的物理模拟
2. 轮胎与地面的交互
3. 车辆控制输入系统
4. 简单的车辆AI行为