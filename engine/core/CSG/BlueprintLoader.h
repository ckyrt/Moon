#pragma once

#include "Blueprint.h"
#include "BlueprintTypes.h"
#include <string>
#include <memory>

namespace Moon {
namespace CSG {

/**
 * @brief Blueprint JSON 解析器
 */
class BlueprintLoader {
public:
    /**
     * @brief 从 JSON 文件加载 Blueprint
     * @param filepath JSON 文件路径
     * @param outError 错误信息
     * @return 加载的 Blueprint，失败返回 nullptr
     */
    static std::unique_ptr<Blueprint> LoadFromFile(const std::string& filepath, std::string& outError);

    /**
     * @brief 从 JSON 字符串解析 Blueprint
     * @param jsonContent JSON 内容
     * @param outError 错误信息
     * @return 解析的 Blueprint，失败返回 nullptr
     */
    static std::unique_ptr<Blueprint> ParseFromString(const std::string& jsonContent, std::string& outError);

private:
    // 内部解析辅助函数
    static bool ParseMetadata(const void* json, Blueprint* blueprint, std::string& outError);
    static bool ParseParameters(const void* json, Blueprint* blueprint, std::string& outError);
    static bool ParseAnchors(const void* json, Blueprint* blueprint, std::string& outError);
    static std::unique_ptr<Node> ParseNode(const void* jsonNode, std::string& outError);
    static bool ParseTransform(const void* jsonTransform, TransformTRS& transform, std::string& outError);
    static ValueExpr ParseValueExpr(const void* jsonValue, std::string& outError);
};

} // namespace CSG
} // namespace Moon
