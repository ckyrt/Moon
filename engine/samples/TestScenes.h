#pragma once

class EngineCore;

// 你所有“临时效果测试”都从这里进
namespace TestScenes {

    void TestMaterial(EngineCore* engine);
    void TestCSG(EngineCore* engine);
    void TestCSGBlueprint(EngineCore* engine);  // 新增：测试 JSON Blueprint 系统
    void TestBuildingSample(EngineCore* engine);
    void TestZipLoad(EngineCore* engine);
    // void RunBuildingTest(...);
    // void RunUGCTest(...);

}
