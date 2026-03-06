#include "TestScenes.h"
#include <Logging/Logger.h>
#include <CSG/Blueprint.h>
#include <CSG/BlueprintLoader.h>
#include <CSG/CSGBuilder.h>
#include <CSG/CSGOperations.h>
#include <EngineCore.h>
#include <Scene/MeshRenderer.h>
#include <Scene/Material.h>
#include <Scene/Light.h>

namespace TestScenes {

void TestCSGBlueprint(EngineCore* engine)
{
    MOON_LOG_INFO("CSGBlueprint", "=== Testing CSG Blueprint Scene ===");

    Moon::Scene* scene = engine->GetScene();
    Moon::CSG::BlueprintDatabase database;
    std::string error;

    // 加载 index.json - 注册所有组件（包括场景配置）
    if (!database.LoadIndex("assets/csg/index.json", error)) {
        MOON_LOG_ERROR("CSGBlueprint", "Failed to load index: %s", error.c_str());
        return;
    }

    // 从 index 中获取场景蓝图（懒加载）
    const Moon::CSG::Blueprint* sceneBlueprint = database.GetBlueprint("test_scene");
    if (!sceneBlueprint) {
        MOON_LOG_ERROR("CSGBlueprint", "Scene blueprint not found in index!");
        return;
    }

    Moon::CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(&database);

    // 构建场景
    std::unordered_map<std::string, float> params;
    Moon::CSG::BuildResult result = builder.Build(sceneBlueprint, params, error);

    if (result.meshes.empty() && result.lights.empty()) {
        MOON_LOG_ERROR("CSGBlueprint", "Failed to build scene: %s", error.c_str());
        return;
    }
    
    MOON_LOG_INFO("CSGBlueprint", "Scene built successfully: %d meshes", (int)result.meshes.size());
    
    // 创建所有场景物体
    for (size_t i = 0; i < result.meshes.size(); ++i) {
        const auto& item = result.meshes[i];
        if (item.mesh && item.mesh->IsValid()) {
            auto node = scene->CreateNode("scene_part_" + std::to_string(i));
            
            node->GetTransform()->SetLocalPosition(item.worldTransform.position);
            node->GetTransform()->SetLocalRotation(item.worldTransform.rotation);
            node->GetTransform()->SetLocalScale(item.worldTransform.scale);
            
            auto renderer = node->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(item.mesh);

            // 根据 blueprint 中的 material 字符串应用材质预设（25种材质）
            auto mat = node->AddComponent<Moon::Material>();
            const std::string& matName = item.material;
            
            bool materialApplied = false;
            
            // === 混凝土系列 ===
            if (matName == "concrete")              { mat->SetMaterialPreset(Moon::MaterialPreset::Concrete); materialApplied = true; }
            else if (matName == "concrete_floor")   { mat->SetMaterialPreset(Moon::MaterialPreset::ConcreteFloor); materialApplied = true; }
            else if (matName == "concrete_polished"){ mat->SetMaterialPreset(Moon::MaterialPreset::ConcretePolished); materialApplied = true; }
            
            // === 岩石/砖石系列 ===
            else if (matName == "rock" || matName == "stone") { mat->SetMaterialPreset(Moon::MaterialPreset::Rock); materialApplied = true; }
            else if (matName == "brick")            { mat->SetMaterialPreset(Moon::MaterialPreset::Brick); materialApplied = true; }
            else if (matName == "plaster")          { mat->SetMaterialPreset(Moon::MaterialPreset::Plaster); materialApplied = true; }
            else if (matName == "tile_ceramic")     { mat->SetMaterialPreset(Moon::MaterialPreset::TileCeramic); materialApplied = true; }
            
            // === 木材系列 ===
            else if (matName == "wood")             { mat->SetMaterialPreset(Moon::MaterialPreset::Wood); materialApplied = true; }
            else if (matName == "wood_floor")       { mat->SetMaterialPreset(Moon::MaterialPreset::WoodFloor); materialApplied = true; }
            else if (matName == "wood_polished")    { mat->SetMaterialPreset(Moon::MaterialPreset::WoodPolished); materialApplied = true; }
            else if (matName == "wood_painted")     { mat->SetMaterialPreset(Moon::MaterialPreset::WoodPainted); materialApplied = true; }
            
            // === 金属系列 ===
            else if (matName == "metal")            { mat->SetMaterialPreset(Moon::MaterialPreset::Metal); materialApplied = true; }
            else if (matName == "steel")            { mat->SetMaterialPreset(Moon::MaterialPreset::Steel); materialApplied = true; }
            else if (matName == "aluminum")         { mat->SetMaterialPreset(Moon::MaterialPreset::Aluminum); materialApplied = true; }
            else if (matName == "chrome")           { mat->SetMaterialPreset(Moon::MaterialPreset::Chrome); materialApplied = true; }
            else if (matName == "copper")           { mat->SetMaterialPreset(Moon::MaterialPreset::Copper); materialApplied = true; }
            
            // === 玻璃系列 ===
            else if (matName == "glass")            { mat->SetMaterialPreset(Moon::MaterialPreset::Glass); materialApplied = true; }
            else if (matName == "glass_frosted")    { mat->SetMaterialPreset(Moon::MaterialPreset::GlassFrosted); materialApplied = true; }
            else if (matName == "glass_tinted")     { mat->SetMaterialPreset(Moon::MaterialPreset::GlassTinted); materialApplied = true; }
            
            // === 软装系列 ===
            else if (matName == "fabric")           { mat->SetMaterialPreset(Moon::MaterialPreset::Fabric); materialApplied = true; }
            else if (matName == "leather")          { mat->SetMaterialPreset(Moon::MaterialPreset::Leather); materialApplied = true; }
            else if (matName == "carpet")           { mat->SetMaterialPreset(Moon::MaterialPreset::Carpet); materialApplied = true; }
            
            // === 塑料/橡胶系列 ===
            else if (matName == "plastic")          { mat->SetMaterialPreset(Moon::MaterialPreset::Plastic); materialApplied = true; }
            else if (matName == "rubber")           { mat->SetMaterialPreset(Moon::MaterialPreset::Rubber); materialApplied = true; }
            
            else if (!matName.empty()) {
                MOON_LOG_WARN("CSGBlueprint", "Unknown material '%s' - see index.json for allowed materials", matName.c_str());
            }
            
            mat->SetMappingMode(Moon::MappingMode::Triplanar);

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
            
            MOON_LOG_INFO("CSGBlueprint", "  Part %d: Pos(%.2f, %.2f, %.2f) Bounds Y[%.3f, %.3f]", 
                (int)i, item.worldTransform.position.x, item.worldTransform.position.y, item.worldTransform.position.z, minB.y, maxB.y);
        }
    }

    // 创建光源
    for (size_t i = 0; i < result.lights.size(); ++i) {
        const auto& item = result.lights[i];
        auto node = scene->CreateNode("scene_light_" + std::to_string(i));

        node->GetTransform()->SetLocalPosition(item.worldTransform.position);
        node->GetTransform()->SetLocalRotation(item.worldTransform.rotation);
        node->GetTransform()->SetLocalScale(item.worldTransform.scale);

        auto light = node->AddComponent<Moon::Light>();
        switch (item.type) {
            case Moon::CSG::LightItem::Type::Directional: light->SetType(Moon::Light::Type::Directional); break;
            case Moon::CSG::LightItem::Type::Point:       light->SetType(Moon::Light::Type::Point); break;
            case Moon::CSG::LightItem::Type::Spot:        light->SetType(Moon::Light::Type::Spot); break;
        }
        light->SetColor(item.color);
        light->SetIntensity(item.intensity);

        if (light->GetType() == Moon::Light::Type::Point || light->GetType() == Moon::Light::Type::Spot) {
            light->SetRange(item.range);
            light->SetAttenuation(item.attenuation.x, item.attenuation.y, item.attenuation.z);
        }
        if (light->GetType() == Moon::Light::Type::Spot) {
            light->SetSpotAngles(item.spotInnerConeAngle, item.spotOuterConeAngle);
        }

        light->SetCastShadows(item.castShadows);

        MOON_LOG_INFO("CSGBlueprint", "  Light %d: type=%d pos(%.2f, %.2f, %.2f) intensity=%.2f",
            (int)i, (int)light->GetType(),
            item.worldTransform.position.x, item.worldTransform.position.y, item.worldTransform.position.z,
            item.intensity);
    }

    MOON_LOG_INFO("CSGBlueprint", "=== Scene loaded successfully ===");
}

} // namespace TestScenes
