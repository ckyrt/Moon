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

    // 加载 cup_body_v1 Blueprint - A meaningful example (hollow cylinder cup)
    std::string blueprintPath = "assets/csg/components/cup_body_v1.json";
    if (!database.LoadBlueprint(blueprintPath, error)) {
        MOON_LOG_ERROR("CSGBlueprint", "Failed to load blueprint: %s", error.c_str());
        return;
    }

    MOON_LOG_INFO("CSGBlueprint", "Successfully loaded blueprint from: %s", blueprintPath.c_str());

    // 获取 Blueprint
    const Moon::CSG::Blueprint* blueprint = database.GetBlueprint("cup_body_v1");
    if (!blueprint) {
        MOON_LOG_ERROR("CSGBlueprint", "Failed to get blueprint");
        return;
    }

    // 创建 Builder
    Moon::CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(&database);

    // 构建 Mesh
    std::unordered_map<std::string, float> params; // 无参数覆盖
    Moon::CSG::BuildResult result = builder.Build(blueprint, params, error);

    if (result.meshes.empty()) {
        MOON_LOG_ERROR("CSGBlueprint", "Build failed: %s", error.c_str());
        return;
    }

    MOON_LOG_INFO("CSGBlueprint", "Build succeeded! Generated %zu mesh(es)", result.meshes.size());

    // 将所有生成的 Mesh 添加到场景
    for (size_t i = 0; i < result.meshes.size(); ++i) {
        const auto& meshItem = result.meshes[i];

        std::string nodeName = "CSG_Blueprint_" + std::to_string(i);
        Moon::SceneNode* node = scene->CreateNode(nodeName);
        
        // 设置位置（演示位置）
        node->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 2.0f, 0.0f));

        // 添加 MeshRenderer
        Moon::MeshRenderer* renderer = node->AddComponent<Moon::MeshRenderer>();
        renderer->SetMesh(meshItem.mesh);

        // 添加材质
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetMaterialPreset(Moon::MaterialPreset::Metal);
        material->SetMappingMode(Moon::MappingMode::Triplanar);
        material->SetTriplanarTiling(0.5f);
        material->SetTriplanarBlend(4.0f);

        MOON_LOG_INFO("CSGBlueprint", "Added mesh %zu to scene: %zu vertices, %zu triangles",
            i, meshItem.mesh->GetVertexCount(), meshItem.mesh->GetTriangleCount());
    }

    MOON_LOG_INFO("CSGBlueprint", "=== CSG Blueprint Test Complete ===");
}

} // namespace TestScenes
