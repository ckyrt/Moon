#pragma once

#include <chrono>

namespace Moon {

/**
 * @brief FPS计数器 - 使用滑动窗口平均算法
 * 
 * 用于测量和平滑帧率，避免FPS数值抖动
 */
class FPSCounter {
public:
    /**
     * @brief 构造函数
     * @param sampleCount 采样窗口大小（默认60帧）
     */
    explicit FPSCounter(int sampleCount = 60);
    
    /**
     * @brief 析构函数
     */
    ~FPSCounter();
    
    /**
     * @brief 更新FPS计数器（每帧调用一次）
     * @param deltaTime 当前帧时间（秒）
     */
    void Update(float deltaTime);
    
    /**
     * @brief 获取平均FPS
     * @return 平滑后的FPS值
     */
    float GetFPS() const { return m_fps; }
    
    /**
     * @brief 获取平均帧时间
     * @return 平滑后的帧时间（秒）
     */
    float GetFrameTime() const { return m_avgDeltaTime; }
    
    /**
     * @brief 获取平均帧时间（毫秒）
     * @return 平滑后的帧时间（毫秒）
     */
    float GetFrameTimeMs() const { return m_avgDeltaTime * 1000.0f; }
    
    /**
     * @brief 重置计数器
     */
    void Reset();

private:
    int m_sampleCount;           // 采样窗口大小
    float* m_history;            // 历史帧时间数组
    int m_index;                 // 当前写入索引
    int m_frameCount;            // 总帧数计数
    float m_avgDeltaTime;        // 平均帧时间
    float m_fps;                 // 当前FPS
};

} // namespace Moon
