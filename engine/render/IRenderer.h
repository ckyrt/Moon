#pragma once
#include <cstdint>

// Forward declarations
namespace Moon {
    struct Matrix4x4;
    class Mesh;
}

struct RenderInitParams {
    void* windowHandle = nullptr; // HWND on Windows
    uint32_t width = 1280;
    uint32_t height = 720;
};

/**
 * @brief 渲染器接口 - 抽象所有渲染后端的通用接口
 * 
 * 支持多种渲染后端：DiligentEngine, bgfx, Vulkan, DX12 等
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    // === 生命周期管理 ===
    
    /**
     * @brief 初始化渲染器
     * @param params 初始化参数
     * @return true 成功, false 失败
     */
    virtual bool Initialize(const RenderInitParams& params) = 0;
    
    /**
     * @brief 关闭渲染器，释放资源
     */
    virtual void Shutdown() = 0;
    
    /**
     * @brief 调整渲染目标大小
     * @param w 新宽度
     * @param h 新高度
     */
    virtual void Resize(uint32_t w, uint32_t h) = 0;
    
    // === 渲染流程控制 ===
    
    /**
     * @brief 开始渲染帧
     * 
     * 清空缓冲区，设置渲染状态
     */
    virtual void BeginFrame() = 0;
    
    /**
     * @brief 结束渲染帧
     * 
     * 提交渲染命令，呈现到屏幕
     */
    virtual void EndFrame() = 0;
    
    /**
     * @brief 渲染整个帧（遗留方法）
     * 
     * 等价于 BeginFrame() + 渲染内容 + EndFrame()
     * 用于向后兼容，未来可能移除
     */
    virtual void RenderFrame() = 0;
    
    // === 相机和变换 ===
    
    /**
     * @brief 设置相机的视图投影矩阵
     * @param viewProj16 4x4 矩阵的 16 个浮点数（行主序）
     */
    virtual void SetViewProjectionMatrix(const float* viewProj16) = 0;
    
    // === 绘制接口 ===
    
    /**
     * @brief 绘制网格（主要接口）
     * @param mesh 要绘制的 Mesh 数据
     * @param worldMatrix 世界变换矩阵
     * 
     * 渲染器负责：
     * 1. 从 Mesh 获取顶点/索引数据
     * 2. 创建/更新 GPU 缓冲区
     * 3. 设置渲染状态并绘制
     */
    virtual void DrawMesh(Moon::Mesh* mesh, const Moon::Matrix4x4& worldMatrix) = 0;
    
    /**
     * @brief 绘制立方体（便捷方法，已废弃）
     * @param worldMatrix 世界变换矩阵
     * @deprecated 请使用 DrawMesh() 替代，未来版本将移除
     */
    virtual void DrawCube(const Moon::Matrix4x4& worldMatrix) = 0;
};
