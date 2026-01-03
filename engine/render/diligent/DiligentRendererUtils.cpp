#include "DiligentRendererUtils.h"
#include "../../core/Math/Matrix4x4.h"
#include "../../core/Mesh/Mesh.h"
#include "../../core/Logging/Logger.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace DiligentRendererUtils {

// ======= 辅助函数：获取 exe 所在目录 =======
std::string GetExecutableDirectory()
{
#ifdef _WIN32
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string path(exePath);
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        return path.substr(0, lastSlash + 1);
    }
    return "";
#else
    // Linux/Mac 实现可以使用 readlink("/proc/self/exe") 等
    return "";
#endif
}

// ======= 工具函数 =======
Moon::Matrix4x4 Transpose(const Moon::Matrix4x4& a)
{
    Moon::Matrix4x4 t{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            t.m[i][j] = a.m[j][i];
    return t;
}

/**
 * @brief 从 Vertex 结构自动生成 Diligent Engine 的 InputLayout
 * 
 * 该函数根据 Vertex::GetLayoutDesc() 提供的布局信息，自动生成 LayoutElement 数组。
 * 这样当 Vertex 结构修改时（例如添加法线、UV），只需更新 Vertex::GetLayoutDesc()，
 * 所有 PSO 的 InputLayout 会自动同步更新。
 * 
 * 优势：
 * - ✅ 单一真实来源（Single Source of Truth）在 Vertex 结构内部
 * - ✅ 使用 offsetof() 自动计算偏移，无需手动维护
 * - ✅ 编译时 static_assert 验证布局正确性
 * 
 * @param[out] outLayout 指向至少能容纳 Vertex 属性数量的 LayoutElement 数组
 * @param[out] outNumElements 返回属性数量
 */
void GetVertexLayout(Diligent::LayoutElement* outLayout, Diligent::Uint32& outNumElements)
{
    int attrCount = 0;
    const Moon::VertexAttributeDesc* attrs = Moon::Vertex::GetLayoutDesc(attrCount);
    
    for (int i = 0; i < attrCount; ++i) {
        outLayout[i].InputIndex = i;                              // ATTRIB0, ATTRIB1, ... 的索引
        outLayout[i].BufferSlot = 0;                              // 使用第 0 个 vertex buffer
        outLayout[i].NumComponents = attrs[i].numComponents;      // 从 Vertex 读取分量数
        outLayout[i].ValueType = Diligent::VT_FLOAT32;           // 目前都是 float
        outLayout[i].IsNormalized = Diligent::False;             // 不归一化
        outLayout[i].RelativeOffset = attrs[i].offsetInBytes;     // 从 Vertex 读取偏移
    }
    
    outNumElements = static_cast<Diligent::Uint32>(attrCount);
}

void UpdateConstantBuffer(Diligent::IBuffer* buf, Diligent::IDeviceContext* context, const void* data, size_t size)
{
    void* p = nullptr;
    context->MapBuffer(buf, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, p);
    std::memcpy(p, data, size);
    context->UnmapBuffer(buf, Diligent::MAP_WRITE);
}

std::string LoadShaderSource(const char* filename)
{
    std::string shaderPath = GetExecutableDirectory() + "assets\\shaders\\" + filename;
    
    std::ifstream file(shaderPath);
    if (!file.is_open()) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to load shader: %s", shaderPath.c_str());
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    MOON_LOG_INFO("DiligentRenderer", "Loaded shader: %s (%zu bytes)", shaderPath.c_str(), content.size());
    return content;
}

} // namespace DiligentRendererUtils
