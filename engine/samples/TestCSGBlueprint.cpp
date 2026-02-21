#include "TestScenes.h"
#include <Logging/Logger.h>
#include <CSG/Blueprint.h>
#include <CSG/BlueprintLoader.h>
#include <CSG/CSGBuilder.h>
#include <EngineCore.h>
#include <Scene/MeshRenderer.h>
#include <Scene/Material.h>

namespace TestScenes {

void TestCSGBlueprint(EngineCore* engine)
{
    MOON_LOG_INFO("CSGBlueprint", "=== Testing CSG Blueprint System ===");

    Moon::Scene* scene = engine->GetScene();

    // 创建 Blueprint 数据库
    Moon::CSG::BlueprintDatabase database;
    std::string error;

    // 加载多个 Blueprint 组件
    std::vector<std::string> blueprintFiles = {
        "assets/csg/components/beam_v1.json",
        "assets/csg/components/plank_v1.json",
        "assets/csg/components/pipe_v1.json",
        "assets/csg/components/disk_v1.json",
        "assets/csg/components/rod_v1.json",
        "assets/csg/components/cup_body_v1.json",
        "assets/csg/components/bowl_v1.json",
        "assets/csg/components/bottle_body_v1.json",
        "assets/csg/components/simple_table_v1.json"
    };

    for (const auto& path : blueprintFiles) {
        if (!database.LoadBlueprint(path, error)) {
            MOON_LOG_ERROR("CSGBlueprint", "Failed to load blueprint %s: %s", path.c_str(), error.c_str());
        } else {
            MOON_LOG_INFO("CSGBlueprint", "Loaded blueprint: %s", path.c_str());
        }
    }

    // 定义要显示的物体及其位置
    struct ObjectToShow {
        std::string blueprintId;
        Moon::Vector3 position;
        float scale;
    };

    std::vector<ObjectToShow> objects = {
        {"beam_v1", Moon::Vector3(-140.0f, 0.0f, 0.0f), 0.5f},
        {"plank_v1", Moon::Vector3(-110.0f, 0.0f, 0.0f), 0.5f},
        {"pipe_v1", Moon::Vector3(-80.0f, 0.0f, 0.0f), 0.5f},
        {"disk_v1", Moon::Vector3(-50.0f, 0.0f, 0.0f), 1.0f},
        {"rod_v1", Moon::Vector3(-20.0f, 0.0f, 0.0f), 0.5f},
        {"cup_body_v1", Moon::Vector3(10.0f, 0.0f, 0.0f), 1.0f},
        {"bowl_v1", Moon::Vector3(40.0f, 0.0f, 0.0f), 1.0f},
        {"bottle_body_v1", Moon::Vector3(70.0f, 0.0f, 0.0f), 1.0f},
        {"simple_table_v1", Moon::Vector3(110.0f, 0.0f, 0.0f), 0.5f}
    };

    // 创建 Builder
    Moon::CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(&database);

    // 构建并显示所有物体
    for (size_t objIdx = 0; objIdx < objects.size(); ++objIdx) {
        const auto& obj = objects[objIdx];
        
        const Moon::CSG::Blueprint* blueprint = database.GetBlueprint(obj.blueprintId);
        if (!blueprint) {
            MOON_LOG_ERROR("CSGBlueprint", "Blueprint not found: %s", obj.blueprintId.c_str());
            continue;
        }

        // 构建 Mesh
        std::unordered_map<std::string, float> params;
        Moon::CSG::BuildResult result = builder.Build(blueprint, params, error);

        if (result.meshes.empty()) {
            MOON_LOG_ERROR("CSGBlueprint", "Build failed for %s: %s", obj.blueprintId.c_str(), error.c_str());
            continue;
        }

        MOON_LOG_INFO("CSGBlueprint", "Built %s: %zu mesh(es)", obj.blueprintId.c_str(), result.meshes.size());

        // 将生成的 Mesh 添加到场景
        for (size_t i = 0; i < result.meshes.size(); ++i) {
            const auto& meshItem = result.meshes[i];

            std::string nodeName = obj.blueprintId + "_" + std::to_string(i);
            Moon::SceneNode* node = scene->CreateNode(nodeName);
            
            // 设置位置和缩放
            node->GetTransform()->SetLocalPosition(obj.position);
            node->GetTransform()->SetLocalScale(Moon::Vector3(obj.scale, obj.scale, obj.scale));

            // 添加 MeshRenderer
            Moon::MeshRenderer* renderer = node->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(meshItem.mesh);

            // 使用默认材质（白色）

            MOON_LOG_INFO("CSGBlueprint", "Added %s mesh %zu: %zu vertices, %zu triangles",
                obj.blueprintId.c_str(), i, meshItem.mesh->GetVertexCount(), meshItem.mesh->GetTriangleCount());
        }
    }

    MOON_LOG_INFO("CSGBlueprint", "=== CSG Blueprint Test Complete ===");
}

} // namespace TestScenes
