# Moon Building System V8

AI程序化建筑生成系统

## 概述

Moon Building System V8是一个基于JSON的程序化建筑生成系统。AI生成结构化的JSON文档，引擎将其转换为完整的3D建筑。

## 架构

### 管道流程

```
AI Prompt
    ↓
AI JSON Generation (external)
    ↓
Schema Validator
    ↓
Layout Validator
    ↓
Mass Processor
    ↓
Floor Generator
    ↓
Space Graph Builder
    ↓
Wall Generator
    ↓
Door Generator
    ↓
Stair Generator
    ↓
Facade Generator
    ↓
Mesh Builder (future)
    ↓
Renderer
```

### 核心原则

1. **JSON是唯一数据源** - 所有编辑通过JSON进行
2. **网格对齐** - 所有坐标必须对齐到0.5米网格
3. **AI生成结构，不生成几何体** - AI只生成建筑布局，不生成顶点/网格

## 使用方法

### 基本用法

```cpp
#include <Building/Building.h>

using namespace Moon::Building;

// 创建管道
BuildingPipeline pipeline;

// JSON输入（moon_building_v8格式）
std::string jsonInput = R"({
  "schema": "moon_building_v8",
  "grid": 0.5,
  "style": {
    "category": "modern",
    "facade": "glass_white",
    "roof": "flat",
    "window_style": "full_height",
    "material": "concrete_white"
  },
  "masses": [
    {
      "mass_id": "main",
      "origin": [0, 0],
      "size": [12, 10],
      "floors": 2
    }
  ],
  "floors": [
    {
      "level": 0,
      "mass_id": "main",
      "floor_height": 3.2,
      "spaces": [
        {
          "space_id": 1,
          "rects": [
            {
              "rect_id": "r1",
              "origin": [0, 0],
              "size": [6, 4]
            }
          ],
          "properties": {
            "usage_hint": "living",
            "is_outdoor": false,
            "stairs": false,
            "ceiling_height": 3.2
          },
          "anchors": []
        }
      ]
    }
  ]
})";

// 处理建筑
GeneratedBuilding building;
std::string error;

if (pipeline.ProcessBuilding(jsonInput, building, error)) {
    // 成功！使用生成的数据
    
    // 访问墙壁
    for (const auto& wall : building.walls) {
        // wall.start, wall.end, wall.type, wall.height
    }
    
    // 访问门
    for (const auto& door : building.doors) {
        // door.position, door.type, door.width
    }
    
    // 访问窗户
    for (const auto& window : building.windows) {
        // window.position, window.width, window.height
    }
    
    // 访问空间连接
    for (const auto& conn : building.connections) {
        // conn.spaceA, conn.spaceB, conn.hasDoor
    }
} else {
    // 错误处理
    std::cerr << "Building generation failed: " << error << std::endl;
}
```

### 仅验证（不生成）

```cpp
ValidationResult result;
if (pipeline.ValidateOnly(jsonInput, result)) {
    std::cout << "JSON is valid" << std::endl;
} else {
    std::cout << "Validation errors:" << std::endl;
    for (const auto& error : result.errors) {
        std::cout << "  - " << error << std::endl;
    }
}

// 检查警告
if (!result.warnings.empty()) {
    std::cout << "Warnings:" << std::endl;
    for (const auto& warning : result.warnings) {
        std::cout << "  - " << warning << std::endl;
    }
}
```

## 模块说明

### 1. BuildingTypes.h
定义所有数据结构：
- `BuildingDefinition` - 完整建筑定义
- `Mass` - 建筑体量
- `Floor` - 楼层
- `Space` - 房间/空间
- `WallSegment` - 墙壁段
- `Door`, `Window` - 门窗
- `ValidationResult` - 验证结果

### 2. SchemaValidator
- 解析JSON字符串
- 验证moon_building_v8格式
- 检查网格对齐
- 转换为BuildingDefinition结构

### 3. LayoutValidator
- 验证空间布局合理性
- 检查房间重叠
- 验证最小尺寸
- 检查边界约束
- 验证楼梯连接

### 4. SpaceGraphBuilder
- 分析空间邻接关系
- 构建连接图
- 计算共享边缘

### 5. WallGenerator
- 从空间边界生成墙壁
- 分类内墙/外墙
- 计算墙壁厚度和高度

### 6. DoorGenerator
- 在连接的空间之间放置门
- 支持门提示（door_hint）
- 根据空间类型选择门类型

### 7. StairGenerator
- 生成楼梯几何
- 计算台阶数量和尺寸
- 支持直梯、L型、U型、螺旋梯

### 8. FacadeGenerator
- 生成外立面元素
- 在外墙上放置窗户
- 支持阳台、栏杆

### 9. BuildingPipeline
- 主协调类
- 按顺序执行所有管道阶段
- 提供统一的API

## JSON Schema

### 必需字段

```json
{
  "schema": "moon_building_v8",
  "grid": 0.5,
  "style": { ... },
  "masses": [ ... ],
  "floors": [ ... ]
}
```

### Style 字段

```json
"style": {
  "category": "modern",      // modern, classical, industrial
  "facade": "glass_white",   // glass_white, brick, concrete
  "roof": "flat",            // flat, pitched, gable
  "window_style": "full_height", // full_height, standard, small
  "material": "concrete_white"
}
```

### Mass 定义

```json
{
  "mass_id": "main",
  "origin": [0, 0],    // [x, y] 网格坐标
  "size": [12, 10],    // [width, depth] 米
  "floors": 2          // 楼层数
}
```

### Floor 定义

```json
{
  "level": 0,
  "mass_id": "main",
  "floor_height": 3.2,
  "spaces": [ ... ]
}
```

### Space 定义

```json
{
  "space_id": 1,
  "rects": [
    {
      "rect_id": "r1",
      "origin": [0, 0],
      "size": [6, 4]
    }
  ],
  "properties": {
    "usage_hint": "living",
    "is_outdoor": false,
    "stairs": false,
    "ceiling_height": 3.2
  },
  "anchors": []
}
```

## 网格系统

所有坐标必须对齐到0.5米网格：

**有效坐标：** 0, 0.5, 1, 1.5, 2, 2.5, ...

**无效坐标：** 0.37, 1.28, 2.13, ...

这样做的好处：
- 提高AI可靠性
- 避免浮点精度问题
- 简化几何生成

## 扩展点

### 添加新的墙壁类型

在 `BuildingTypes.h` 中添加新的 `WallType` 枚举值，然后在 `WallGenerator.cpp` 中实现相应逻辑。

### 添加新的门类型

在 `BuildingTypes.h` 中添加新的 `DoorType` 枚举值，在 `DoorGenerator.cpp` 的 `DetermineDoorType` 中添加逻辑。

### 自定义验证规则

继承 `LayoutValidator` 并重写验证方法，或添加新的验证函数。

## 参考文档

详细规范请参阅：`docs/moon_ai_building_system_v8.md`

## 文件清单

```
engine/building/
├── Building.h              # 主头文件（包含所有）
├── BuildingTypes.h         # 数据结构定义
├── BuildingPipeline.h/cpp  # 主管道类
├── SchemaValidator.h/cpp   # JSON验证器
├── LayoutValidator.h/cpp   # 布局验证器
├── SpaceGraphBuilder.h/cpp # 空间图构建器
├── WallGenerator.h/cpp     # 墙壁生成器
├── DoorGenerator.h/cpp     # 门生成器
├── StairGenerator.h/cpp    # 楼梯生成器
├── FacadeGenerator.h/cpp   # 外立面生成器
└── README.md              # 本文件
```

## 依赖

- nlohmann/json - JSON解析
- engine/core/Math - 向量和数学运算

## 未来工作

- [ ] Mesh Builder - 从生成数据创建3D网格
- [ ] 更复杂的楼梯类型
- [ ] 屋顶生成器
- [ ] 地形集成
- [ ] 优化和修复算法
- [ ] 更多建筑样式

## 示例

参见完整示例JSON：`docs/moon_ai_building_system_v8.md` 第15节
