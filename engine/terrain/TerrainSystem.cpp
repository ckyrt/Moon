#include "TerrainSystem.h"

#include <algorithm>

namespace Moon {

void TerrainSystem::SetEnabled(bool enabled) {
    m_runtimeState.enabled = enabled;
}

bool TerrainSystem::IsEnabled() const {
    return m_runtimeState.enabled;
}

void TerrainSystem::SetProfile(const TerrainProfile& profile) {
    m_profile = profile;
    RebuildChunkLayout();
}

const TerrainProfile& TerrainSystem::GetProfile() const {
    return m_profile;
}

void TerrainSystem::SetData(const TerrainData& data) {
    m_data = data;
    ++m_runtimeState.heightRevision;
    RebuildChunkLayout();
}

const TerrainData& TerrainSystem::GetData() const {
    return m_data;
}

void TerrainSystem::ResizeHeightmap(uint32_t width, uint32_t height, float fillValue) {
    m_data.heightmap.Resize(width, height, fillValue);
    ++m_runtimeState.heightRevision;
    RebuildChunkLayout();
}

float TerrainSystem::GetHeightSample(uint32_t x, uint32_t y) const {
    return m_data.heightmap.GetSample(x, y);
}

bool TerrainSystem::SetHeightSample(uint32_t x, uint32_t y, float value) {
    if (!m_data.heightmap.SetSample(x, y, value)) {
        return false;
    }

    ++m_runtimeState.heightRevision;
    const uint32_t chunkIndex = GetChunkIndexForSample(x, y);
    MarkChunkDirty(chunkIndex);
    RefreshRuntimeState();
    return true;
}

const TerrainRuntimeState& TerrainSystem::GetRuntimeState() const {
    return m_runtimeState;
}

const std::vector<TerrainChunkState>& TerrainSystem::GetChunks() const {
    return m_chunks;
}

void TerrainSystem::Update(float) {
    if (!m_runtimeState.enabled) {
        return;
    }

    RefreshRuntimeState();
}

void TerrainSystem::RebuildChunkLayout() {
    m_chunks.clear();

    const uint32_t width = m_data.heightmap.GetWidth();
    const uint32_t height = m_data.heightmap.GetHeight();
    const uint32_t chunkCountX = ComputeChunkCount(width, m_profile.chunkResolutionQuads);
    const uint32_t chunkCountZ = ComputeChunkCount(height, m_profile.chunkResolutionQuads);

    m_chunks.reserve(static_cast<size_t>(chunkCountX) * static_cast<size_t>(chunkCountZ));
    for (uint32_t z = 0; z < chunkCountZ; ++z) {
        for (uint32_t x = 0; x < chunkCountX; ++x) {
            TerrainChunkState chunk;
            chunk.coord.x = static_cast<int>(x);
            chunk.coord.z = static_cast<int>(z);
            m_chunks.push_back(chunk);
        }
    }

    MarkAllChunksDirty();
    RefreshRuntimeState();
}

void TerrainSystem::MarkAllChunksDirty() {
    for (TerrainChunkState& chunk : m_chunks) {
        chunk.heightDirty = true;
        chunk.materialDirty = true;
        chunk.vegetationDirty = true;
        ++chunk.revision;
    }
}

void TerrainSystem::MarkChunkDirty(uint32_t chunkIndex) {
    if (chunkIndex >= m_chunks.size()) {
        return;
    }

    TerrainChunkState& chunk = m_chunks[chunkIndex];
    chunk.heightDirty = true;
    chunk.materialDirty = true;
    ++chunk.revision;
}

void TerrainSystem::RefreshRuntimeState() {
    m_runtimeState.chunkCountX = ComputeChunkCount(m_data.heightmap.GetWidth(), m_profile.chunkResolutionQuads);
    m_runtimeState.chunkCountZ = ComputeChunkCount(m_data.heightmap.GetHeight(), m_profile.chunkResolutionQuads);
    m_runtimeState.dirtyChunkCount = 0;

    for (const TerrainChunkState& chunk : m_chunks) {
        if (chunk.heightDirty || chunk.materialDirty || chunk.vegetationDirty) {
            ++m_runtimeState.dirtyChunkCount;
        }
    }
}

uint32_t TerrainSystem::GetChunkIndexForSample(uint32_t x, uint32_t y) const {
    if (m_chunks.empty()) {
        return 0;
    }

    const uint32_t quadsPerChunk = std::max(1u, m_profile.chunkResolutionQuads);
    const uint32_t chunkX = x / quadsPerChunk;
    const uint32_t chunkZ = y / quadsPerChunk;
    const uint32_t chunkCountX = std::max(1u, m_runtimeState.chunkCountX);
    return std::min(chunkZ * chunkCountX + chunkX, static_cast<uint32_t>(m_chunks.size() - 1));
}

uint32_t TerrainSystem::ComputeChunkCount(uint32_t sampleCount, uint32_t chunkResolutionQuads) {
    if (sampleCount <= 1) {
        return 0;
    }

    const uint32_t quads = sampleCount - 1;
    const uint32_t chunkQuads = std::max(1u, chunkResolutionQuads);
    return (quads + chunkQuads - 1) / chunkQuads;
}

} // namespace Moon
