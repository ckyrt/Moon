#pragma once
#include "SceneNode.h"
#include <string>
#include <vector>
#include <functional>

namespace Moon {

/**
 * @brief 场景管理器 - 管理场景中的所有节点
 * 
 * Scene 负责管理场景中所有节点的生命周期、更新和遍历。
 * 维护根节点列表和所有节点的引用，提供查找和遍历功能。
 */
class Scene {
public:
    /**
     * @brief 构造函数
     * @param name 场景名称
     */
    explicit Scene(const std::string& name = "Untitled Scene");
    
    /**
     * @brief 析构函数
     */
    ~Scene();

    // === 场景管理 ===
    
    /**
     * @brief 获取场景名称
     */
    const std::string& GetName() const { return m_name; }
    
    /**
     * @brief 设置场景名称
     */
    void SetName(const std::string& name) { m_name = name; }

    // === 节点管理 ===
    
    /**
     * @brief 创建新节点
     * @param name 节点名称
     * @return 新创建的节点指针
     */
    SceneNode* CreateNode(const std::string& name = "GameObject");
    
    /**
     * @brief 销毁节点（延迟删除）
     * @param node 要销毁的节点
     * 
     * 节点会在当前帧结束时删除
     */
    void DestroyNode(SceneNode* node);
    
    /**
     * @brief 立即销毁节点
     * @param node 要销毁的节点
     */
    void DestroyNodeImmediate(SceneNode* node);
    
    /**
     * @brief 按名称查找节点
     * @param name 节点名称
     * @return 找到的第一个匹配节点，未找到返回 nullptr
     */
    SceneNode* FindNodeByName(const std::string& name) const;
    
    /**
     * @brief 按 ID 查找节点
     * @param id 节点 ID
     * @return 找到的节点，未找到返回 nullptr
     */
    SceneNode* FindNodeByID(uint32_t id) const;

    // === 根节点管理 ===
    
    /**
     * @brief 获取根节点数量
     */
    size_t GetRootNodeCount() const { return m_rootNodes.size(); }
    
    /**
     * @brief 获取指定索引的根节点
     * @param index 根节点索引
     * @return 根节点指针，索引越界返回 nullptr
     */
    SceneNode* GetRootNode(size_t index) const;
    
    /**
     * @brief 获取所有根节点
     */
    const std::vector<SceneNode*>& GetRootNodes() const { return m_rootNodes; }

    // === 更新 ===
    
    /**
     * @brief 更新场景中的所有节点
     * @param deltaTime 距离上一帧的时间（秒）
     */
    void Update(float deltaTime);

    // === 遍历 ===
    
    /**
     * @brief 遍历场景中的所有节点（深度优先）
     * @param callback 回调函数，接收每个节点作为参数
     */
    void Traverse(std::function<void(SceneNode*)> callback);
    
    /**
     * @brief 遍历场景中的所有激活节点
     * @param callback 回调函数
     */
    void TraverseActive(std::function<void(SceneNode*)> callback);

    // === 未来扩展：序列化 ===
    // bool SaveToFile(const std::string& path);
    // bool LoadFromFile(const std::string& path);

private:
    std::string m_name;                       ///< 场景名称
    std::vector<SceneNode*> m_rootNodes;      ///< 顶层节点列表
    std::vector<SceneNode*> m_allNodes;       ///< 所有节点列表
    std::vector<SceneNode*> m_pendingDelete;  ///< 待删除节点列表
    
    /**
     * @brief 添加根节点
     */
    void AddRootNode(SceneNode* node);
    
    /**
     * @brief 移除根节点
     */
    void RemoveRootNode(SceneNode* node);
    
    /**
     * @brief 处理待删除节点
     */
    void ProcessPendingDeletes();
    
    /**
     * @brief 递归遍历节点树
     */
    void TraverseNode(SceneNode* node, std::function<void(SceneNode*)> callback);
    
    // 供 SceneNode 使用的内部方法
    friend class SceneNode;
};

} // namespace Moon
