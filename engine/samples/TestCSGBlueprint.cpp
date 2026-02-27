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
    MOON_LOG_INFO("CSGBlueprint", "=== Testing Blueprints from index.json ===");

    Moon::Scene* scene = engine->GetScene();
    Moon::CSG::BlueprintDatabase database;
    std::string error;

    // 从 index.json 加载全部蓝图 —— 新增/删除物体只需编辑 index.json
    if (!database.LoadIndex("assets/csg/index.json", error)) {
        MOON_LOG_ERROR("CSGBlueprint", "Failed to load index: %s", error.c_str());
        return;
    }

    Moon::CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(&database);

    // 物体排列：从左到右间隔 1.5米，放在摄像机前方
    float xOffset = -2.0f;
    float zPos = 3.0f;

    for (const auto& id : database.GetAllIds()) {
        const Moon::CSG::Blueprint* blueprint = database.GetBlueprint(id);
        const std::string& name = id;
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
