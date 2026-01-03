#pragma once

#include <string>

// 前向声明
namespace Moon {
    struct Matrix4x4;
    struct VertexAttributeDesc;
    struct Vertex;
}

namespace Diligent {
    struct LayoutElement;
    struct IBuffer;
    struct IDeviceContext;
    using Uint32 = uint32_t;
}

namespace DiligentRendererUtils {

// 获取可执行文件所在目录
std::string GetExecutableDirectory();

// 矩阵转置
Moon::Matrix4x4 Transpose(const Moon::Matrix4x4& a);

// 获取顶点布局
void GetVertexLayout(Diligent::LayoutElement* outLayout, Diligent::Uint32& outNumElements);

// 更新常量缓冲区（非模板版本，用于跨编译单元）
void UpdateConstantBuffer(Diligent::IBuffer* buf, Diligent::IDeviceContext* context, const void* data, size_t size);

} // namespace DiligentRendererUtils
