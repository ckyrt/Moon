#pragma once
#include <string>
#include "../../engine/core/Scene/Scene.h"
#include "../../engine/core/Scene/SceneNode.h"

// Forward declarations
class EngineCore;

/**
 * @brief 场景序列化器 - 将引擎场景数据序列化为 JSON
 * 
 * 负责将 C++ 的 Scene 和 SceneNode 数据结构转换为 JSON 格式，
 * 供 WebUI (JavaScript) 使用。
 */
class SceneSerializer {
public:
    /**
     * @brief 获取场景层级结构（完整场景数据）
     * @param scene 场景指针
     * @return JSON 字符串，包含所有节点的 ID、名称、父子关系、Transform、Components
     * 
     * 返回格式：
     * {
     *   "rootNodes": [1, 2, 3],
     *   "allNodes": {
     *     "1": {
     *       "id": 1,
     *       "name": "Cube",
     *       "active": true,
     *       "parentId": null,
     *       "children": [],
     *       "transform": {
     *         "position": {"x": 0, "y": 0, "z": 0},
     *         "rotation": {"x": 0, "y": 0, "z": 0},
     *         "scale": {"x": 1, "y": 1, "z": 1}
     *       },
     *       "components": [...]
     *     }
     *   }
     * }
     */
    static std::string GetSceneHierarchy(Moon::Scene* scene);

    /**
     * @brief 获取单个节点的详细信息
     * @param scene 场景指针
     * @param nodeId 节点 ID
     * @return JSON 字符串，包含节点的完整信息
     * 
     * 返回格式：与 GetSceneHierarchy 中的单个节点格式相同
     */
    static std::string GetNodeDetails(Moon::Scene* scene, uint32_t nodeId);

    /**
     * @brief 序列化单个节点为 JSON 对象（内部辅助函数）
     * @param node 节点指针
     * @return nlohmann::json 对象
     */
    static void SerializeNode(Moon::SceneNode* node, void* jsonObject);

private:
    /**
     * @brief 序列化 Transform 数据
     */
    static void SerializeTransform(Moon::Transform* transform, void* jsonObject);

    /**
     * @brief 序列化 Components 列表
     */
    static void SerializeComponents(Moon::SceneNode* node, void* jsonArray);
};
