#include "TestScenes.h"
#include <Logging/Logger.h>
#include <CSG/Blueprint.h>
#include <CSG/BlueprintLoader.h>
#include <CSG/CSGBuilder.h>
#include <CSG/CSGOperations.h>
#include <EngineCore.h>
#include <Scene/MeshRenderer.h>
#include <Scene/Material.h>

namespace TestScenes {

void TestCSGBlueprint(EngineCore* engine)
{
    MOON_LOG_INFO("CSGBlueprint", "=== Testing Fixed Blueprints ===");

    Moon::Scene* scene = engine->GetScene();
    Moon::CSG::BlueprintDatabase database;
    std::string error;
    
    // 要测试的蓝图列表
    const char* blueprints[] = {
        // 基础结构件
        "assets/csg/components/beam_v1.json",
        "assets/csg/components/plank_v1.json",
        "assets/csg/components/pipe_v1.json",
        "assets/csg/components/disk_v1.json",
        "assets/csg/components/rod_v1.json",
        // 半球/碗
        "assets/csg/components/hemisphere_down_v1.json",
        "assets/csg/components/bowl_v1.json",
        // 容器
        "assets/csg/components/cup_body_v1.json",
        "assets/csg/components/bottle_body_v1.json",
        // 机械
        "assets/csg/components/wheel_v1.json",
        // 家具
        "assets/csg/components/table_leg_v1.json",
        "assets/csg/components/table_top_v1.json",
        "assets/csg/components/simple_table_v1.json",
        "assets/csg/components/table_v1.json",
        "assets/csg/components/chair_v1.json",
        // 门
        "assets/csg/components/door_v1.json",
        "assets/csg/components/door_frame_v1.json",
        "assets/csg/components/complete_door_v1.json",
        // 灯具（使用 rotation_euler）
        "assets/csg/components/desk_lamp_v1.json",
    };
    
    Moon::CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(&database);
    
    // 物体排列：从左到右间隔1.5米，放在摄像机前方5米处
    float xOffset = -2.0f;  // 从左侧开始
    float zPos = 3.0f;       // 摄像机在Z=-5朝向+Z，物体放在Z=3
    
    for (const char* blueprintPath : blueprints) {
        // 加载蓝图
        if (!database.LoadBlueprint(blueprintPath, error)) {
            MOON_LOG_ERROR("CSGBlueprint", "Failed to load %s: %s", blueprintPath, error.c_str());
            continue;
        }
        
        // 从路径提取名称
        std::string path(blueprintPath);
        size_t lastSlash = path.find_last_of("/\\");
        size_t lastDot = path.find_last_of(".");
        std::string name = path.substr(lastSlash + 1, lastDot - lastSlash - 1);
        
        const Moon::CSG::Blueprint* blueprint = database.GetBlueprint(name);
        if (!blueprint) {
            MOON_LOG_ERROR("CSGBlueprint", "%s not found!", name.c_str());
            continue;
        }
        
        // 构建
        std::unordered_map<std::string, float> params;
        Moon::CSG::BuildResult result = builder.Build(blueprint, params, error);
        
        if (result.meshes.empty()) {
            MOON_LOG_ERROR("CSGBlueprint", "Failed to build %s: %s", name.c_str(), error.c_str());
            continue;
        }
        
        MOON_LOG_INFO("CSGBlueprint", "=== %s: %d meshes ===", name.c_str(), (int)result.meshes.size());
        
        // 创建所有部件
        for (size_t i = 0; i < result.meshes.size(); ++i) {
            const auto& item = result.meshes[i];
            if (item.mesh && item.mesh->IsValid()) {
                auto node = scene->CreateNode(name + "_part_" + std::to_string(i));
                
                // 添加X偏移以便并排显示，Z位置放在摄像机前方
                Moon::Vector3 pos = item.worldTransform.position;
                pos.x += xOffset;
                pos.z += zPos;
                node->GetTransform()->SetLocalPosition(pos);
                node->GetTransform()->SetLocalRotation(item.worldTransform.rotation);
                node->GetTransform()->SetLocalScale(item.worldTransform.scale);
                
                auto renderer = node->AddComponent<Moon::MeshRenderer>();
                renderer->SetMesh(item.mesh);
                
                // 计算边界
                const auto& verts = item.mesh->GetVertices();
                Moon::Vector3 minB(1e9f, 1e9f, 1e9f), maxB(-1e9f, -1e9f, -1e9f);
                for (const auto& v : verts) {
                    minB.x = std::min(minB.x, v.position.x);
                    minB.y = std::min(minB.y, v.position.y);
                    minB.z = std::min(minB.z, v.position.z);
                    maxB.x = std::max(maxB.x, v.position.x);
                    maxB.y = std::max(maxB.y, v.position.y);
                    maxB.z = std::max(maxB.z, v.position.z);
                }
                
                MOON_LOG_INFO("CSGBlueprint", "  Part %d: Pos(%.2f, %.2f, %.2f) Local Y[%.3f, %.3f]", 
                    (int)i, pos.x, pos.y, pos.z, minB.y, maxB.y);
            }
        }
        
        xOffset += 1.5f; // 下一个物体向右偏移1.5米
    }

    MOON_LOG_INFO("CSGBlueprint", "=== All blueprints tested ===");
}

} // namespace TestScenes
