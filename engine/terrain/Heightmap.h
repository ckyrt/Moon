#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Moon {

class Heightmap {
public:
    Heightmap() = default;
    Heightmap(uint32_t width, uint32_t height, float fillValue = 0.0f) {
        Resize(width, height, fillValue);
    }

    void Resize(uint32_t width, uint32_t height, float fillValue = 0.0f) {
        m_width = width;
        m_height = height;
        m_samples.assign(static_cast<size_t>(width) * static_cast<size_t>(height), fillValue);
    }

    void Clear(float fillValue = 0.0f) {
        std::fill(m_samples.begin(), m_samples.end(), fillValue);
    }

    bool IsEmpty() const {
        return m_width == 0 || m_height == 0 || m_samples.empty();
    }

    uint32_t GetWidth() const {
        return m_width;
    }

    uint32_t GetHeight() const {
        return m_height;
    }

    bool IsValidCoordinate(uint32_t x, uint32_t y) const {
        return x < m_width && y < m_height;
    }

    float GetSample(uint32_t x, uint32_t y) const {
        if (!IsValidCoordinate(x, y)) {
            return 0.0f;
        }
        return m_samples[ToIndex(x, y)];
    }

    bool SetSample(uint32_t x, uint32_t y, float value) {
        if (!IsValidCoordinate(x, y)) {
            return false;
        }
        m_samples[ToIndex(x, y)] = value;
        return true;
    }

    const std::vector<float>& GetSamples() const {
        return m_samples;
    }

    std::vector<float>& GetSamples() {
        return m_samples;
    }

private:
    size_t ToIndex(uint32_t x, uint32_t y) const {
        return static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x);
    }

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<float> m_samples;
};

} // namespace Moon
