# EngineBuilding 单元测试

## 概述

本目录包含 Moon 游戏引擎 Building 系统的单元测试和集成测试。使用 Google Test 框架编写。

## 测试覆盖

### 1. SchemaValidator 测试 (SchemaValidatorTests.cpp)

测试 JSON 模式验证和解析功能：

**正向测试:**
- ✅ 最小有效建筑
- ✅ 简单房间
- ✅ 多层建筑
- ✅ 网格对齐验证

**负向测试:**
- ❌ 缺失必需字段（grid）
- ❌ 错误的网格大小
- ❌ 非对齐坐标
- ❌ 格式错误的 JSON

### 2. LayoutValidator 测试 (LayoutValidatorTests.cpp)

测试布局合理性验证：

**正向测试:**
- ✅ 有效的最小建筑
- ✅ 有效的房间布局
- ✅ 多层建筑布局

**负向测试:**
- ❌ 空间重叠
- ❌ 空间超出边界
- ❌ 房间太小
- ❌ 无效的 mass 引用

**配置测试:**
- 自定义最小房间尺寸
- 自定义墙壁厚度

### 3. BuildingPipeline 集成测试 (BuildingPipelineTests.cpp)

测试完整的建筑生成管道：

**集成测试:**
- ✅ 处理最小建筑
- ✅ 处理简单房间
- ✅ 处理多层建筑
- ❌ 处理无效输入

**验证测试:**
- ValidateOnly 模式测试
- 错误处理测试

## 构建和运行测试

### 构建测试

使用 MSBuild 构建测试项目：

```powershell
# 构建所有项目（包括测试）
MSBuild Moon.sln /p:Configuration=Debug /p:Platform=x64

# 只构建测试项目
MSBuild Moon.sln /t:EngineBuildingTests /p:Configuration=Debug /p:Platform=x64
```

### 运行测试

```powershell
# 运行所有测试
.\bin\x64\Debug\EngineBuildingTests.exe

# 运行特定测试套件
.\bin\x64\Debug\EngineBuildingTests.exe --gtest_filter=SchemaValidatorTest.*

# 运行特定测试
.\bin\x64\Debug\EngineBuildingTests.exe --gtest_filter=SchemaValidatorTest.ValidateMinimalBuilding_Success

# 显示详细输出
.\bin\x64\Debug\EngineBuildingTests.exe --gtest_verbose

# 生成 XML 报告
.\bin\x64\Debug\EngineBuildingTests.exe --gtest_output=xml:test_results.xml
```

### 常用 Google Test 参数

- `--gtest_list_tests` - 列出所有测试
- `--gtest_filter=<pattern>` - 运行匹配模式的测试
- `--gtest_repeat=<count>` - 重复运行测试
- `--gtest_shuffle` - 随机顺序运行测试
- `--gtest_break_on_failure` - 失败时中断（用于调试）

## 测试辅助工具

### TestHelpers 类

提供创建测试数据的便捷方法：

```cpp
#include "TestHelpers.h"

// 创建最小有效建筑 JSON
std::string json = TestHelpers::CreateMinimalValidBuilding();

// 创建简单房间
std::string json = TestHelpers::CreateSimpleRoom();

// 创建多层建筑
std::string json = TestHelpers::CreateMultiFloorBuilding();

// 创建无效的 JSON（用于负向测试）
std::string json = TestHelpers::CreateInvalidJSON_MissingGrid();
```

## 添加新测试

### 1. 添加新测试用例

```cpp
TEST_F(SchemaValidatorTest, YourNewTest_Success) {
    // Arrange
    std::string json = "...";
    
    // Act
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_EQ(definition.masses.size(), 1);
}
```

### 2. 添加新测试文件

1. 创建 `YourModuleTests.cpp`
2. 在 `EngineBuildingTests.vcxproj` 中添加文件引用
3. 包含必要的头文件和测试 fixture

### 3. 测试命名约定

- 测试类: `<ModuleName>Test`
- 正向测试: `<Function>_<Scenario>_Success`
- 负向测试: `<Function>_<Scenario>_Fails`
- 边界测试: `<Function>_<EdgeCase>`

## 测试最佳实践

### ✅ 好的做法

1. **每个测试只测试一件事**
2. **使用描述性的测试名称**
3. **使用 Arrange-Act-Assert 模式**
4. **测试边界条件和错误情况**
5. **保持测试独立（不依赖其他测试）**
6. **使用 TestHelpers 创建测试数据**

### ❌ 避免的做法

1. ❌ 测试多个不相关的功能
2. ❌ 依赖测试执行顺序
3. ❌ 使用硬编码的绝对路径
4. ❌ 忽略错误消息验证
5. ❌ 创建过于复杂的测试

## 持续集成

测试可以集成到 CI/CD 流程中：

```powershell
# CI 脚本示例
MSBuild Moon.sln /t:EngineBuildingTests /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -eq 0) {
    .\bin\x64\Release\EngineBuildingTests.exe --gtest_output=xml:test_results.xml
    exit $LASTEXITCODE
}
```

## 测试覆盖率目标

| 模块 | 目标覆盖率 | 当前状态 |
|------|-----------|---------|
| SchemaValidator | 90%+ | ✅ 初步完成 |
| LayoutValidator | 90%+ | ✅ 初步完成 |
| BuildingPipeline | 80%+ | ✅ 初步完成 |
| SpaceGraphBuilder | 80%+ | ⏳ 待实现 |
| WallGenerator | 80%+ | ⏳ 待实现 |
| DoorGenerator | 80%+ | ⏳ 待实现 |
| StairGenerator | 80%+ | ⏳ 待实现 |
| FacadeGenerator | 80%+ | ⏳ 待实现 |

## 下一步计划

1. ✅ 完成 SchemaValidator 和 LayoutValidator 核心测试
2. ⏳ 为各个 Generator 添加单元测试
3. ⏳ 添加性能测试
4. ⏳ 添加压力测试（大型建筑）
5. ⏳ 集成代码覆盖率工具

## 问题反馈

如果发现测试问题或需要添加新的测试用例，请在项目中提出。

## 参考资料

- [Google Test 文档](https://google.github.io/googletest/)
- [Building System 规范](../../../assets/building/README.md)
- [Building System README](../README.md)
