#include "FPSCounter.h"
#include <cstring>

namespace Moon {

FPSCounter::FPSCounter(int sampleCount)
    : m_sampleCount(sampleCount)
    , m_index(0)
    , m_frameCount(0)
    , m_avgDeltaTime(0.0f)
    , m_fps(0.0f)
{
    m_history = new float[m_sampleCount];
    std::memset(m_history, 0, sizeof(float) * m_sampleCount);
}

FPSCounter::~FPSCounter()
{
    delete[] m_history;
}

void FPSCounter::Update(float deltaTime)
{
    // 记录当前帧时间
    m_history[m_index] = deltaTime;
    m_index = (m_index + 1) % m_sampleCount;
    m_frameCount++;
    
    // 计算平均帧时间（滑动窗口）
    int samples = (m_frameCount < m_sampleCount) ? m_frameCount : m_sampleCount;
    float sum = 0.0f;
    for (int i = 0; i < samples; i++) {
        sum += m_history[i];
    }
    m_avgDeltaTime = sum / samples;
    
    // 计算FPS
    m_fps = (m_avgDeltaTime > 0.0f) ? (1.0f / m_avgDeltaTime) : 0.0f;
}

void FPSCounter::Reset()
{
    m_index = 0;
    m_frameCount = 0;
    m_avgDeltaTime = 0.0f;
    m_fps = 0.0f;
    std::memset(m_history, 0, sizeof(float) * m_sampleCount);
}

} // namespace Moon
