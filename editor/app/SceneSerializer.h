#pragma once
#include <string>

// Forward declarations
namespace Moon {
    class Scene;
    class SceneNode;
    class Transform;
}
class EngineCore;

namespace Moon {

/**
 * @brief 场景序列化器 - 编辑器序列化系统
 * 
 * 职责：
 * - 将 Scene 和 SceneNode 序列化为 JSON
 * - 从 JSON 反序列化重建 Scene 和 SceneNode
 * - 支持完整场景的 Save/Load
 * - 支持单个节点的序列化（用于 Undo/Redo、网络同步等）
 * - 文件 I/O、格式选择、版本管理
 * 
 * 使用场景：
 * 1. 编辑器 Undo/Redo
 * 2. 场景文件保存/加载
 * 3. 编辑器 UI 数据
 * 4. 关卡数据导入/导出
 */
class SceneSerializer {
public:
    // ========================================================================
    // 完整场景序列化（用于 Save/Load）
    // ========================================================================
    
    /**
     * @brief 保存场景到文件
     * @param scene 场景指针
     * @param filePath 文件路径
     * @return 成功返回 true
     */
    static bool SaveSceneToFile(Scene* scene, const std::string& filePath);
    
    /**
     * @brief 从文件加载场景
     * @param scene 场景指针（会清空现有内容）
     * @param engine 引擎核心（用于创建 Mesh 等资源）
     * @param filePath 文件路径
     * @return 成功返回 true
     */
    static bool LoadSceneFromFile(Scene* scene, EngineCore* engine, const std::string& filePath);

    // ========================================================================
    // 场景数据获取（用于编辑器 UI）
    // ========================================================================
    
    /**
     * @brief 获取场景层级结构（用于编辑器 Hierarchy 面板）
     * @param scene 场景指针
     * @return JSON 字符串，包含所有节点的基本信息
     * 
     * 返回格式：
     * {
     *   "name": "MyScene",
     *   "rootNodes": [1, 2, 3],
     *   "allNodes": {
     *     "1": { "id": 1, "name": "Cube", ... }
     *   }
     * }
     */
    static std::string GetSceneHierarchy(Scene* scene);

    /**
     * @brief 获取单个节点的详细信息（用于编辑器 Inspector 面板）
     * @param scene 场景指针
     * @param nodeId 节点 ID
     * @return JSON 字符串，包含节点的详细信息
     */
    static std::string GetNodeDetails(Scene* scene, uint32_t nodeId);

    // ========================================================================
    // 🎯 单节点序列化（用于 Undo/Redo、网络同步）
    // ========================================================================
    
    /**
     * @brief 序列化单个节点（完整数据，包含所有 Components）
     * @param scene 场景指针
     * @param nodeId 节点 ID
     * @return JSON 字符串
     * 
     * 用途：
     * - Undo/Redo: 删除节点前保存数据（包含整个子树）
     * - 网络同步: 传输节点数据
     * - 预制体系统: 保存节点模板
     * 
     * ⚠️ 注意：会递归序列化所有子节点
     */
    static std::string SerializeNode(Scene* scene, uint32_t nodeId);

    /**
     * @brief 从序列化数据重建节点（完整恢复，包含所有 Components）
     * @param scene 场景指针
     * @param engine 引擎核心（用于创建 Mesh 等资源）
     * @param serializedData JSON 字符串
     * @return 创建的节点指针，失败返回 nullptr
     * 
     * 用途：
     * - Undo/Redo: 恢复被删除的节点（包含整个子树）
     * - 网络同步: 接收远程节点数据
     * - 预制体系统: 实例化节点模板
     * 
     * ⚠️ 注意：会递归反序列化所有子节点
     */
    static SceneNode* DeserializeNode(Scene* scene, EngineCore* engine, const std::string& serializedData);

private:
    // ========================================================================
    // 内部辅助函数
    // ========================================================================
    
    /**
     * @brief 序列化单个节点为 JSON 对象（基础版本，用于 UI）
     */
    static void SerializeNodeBasic(SceneNode* node, void* jsonObject);
    
    /**
     * @brief 序列化单个节点为 JSON 对象（完整版本，用于 Save/Load/Undo）
     */
    static void SerializeNodeFull(SceneNode* node, void* jsonObject);

    /**
     * @brief 序列化 Transform 数据
     */
    static void SerializeTransform(Transform* transform, void* jsonObject);

    /**
     * @brief 序列化 Components 列表（基础版本）
     */
    static void SerializeComponents(SceneNode* node, void* jsonArray);
    
    /**
     * @brief 序列化 Components 列表（完整版本，包含所有属性）
     */
    static void SerializeComponentsFull(SceneNode* node, void* jsonArray);

    /**
     * @brief 反序列化 Components 并添加到节点
     */
    static void DeserializeComponents(SceneNode* node, EngineCore* engine, void* jsonArray);
};

} // namespace Moon
