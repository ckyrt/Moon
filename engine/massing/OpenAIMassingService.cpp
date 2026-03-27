#include "OpenAIMassingService.h"

#include "../../external/nlohmann/json.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace Moon {
namespace Massing {

using json = nlohmann::json;

namespace {

constexpr wchar_t kOpenAIHost[] = L"api.openai.com";
constexpr wchar_t kResponsesPath[] = L"/v1/responses";

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return std::wstring();
    }

    std::wstring result(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &result[0], required);
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return std::string();
    }

    std::string result(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &result[0], required, nullptr, nullptr);
    return result;
}

std::string GetEnvironmentVariableUtf8(const char* name) {
    char* buffer = nullptr;
    size_t envLength = 0;
    if (_dupenv_s(&buffer, &envLength, name) == 0 && buffer != nullptr && envLength > 1) {
        std::string result(buffer);
        free(buffer);
        return result;
    }
    if (buffer != nullptr) {
        free(buffer);
    }

    wchar_t wideBuffer[8192];
    const DWORD wideLength = ::GetEnvironmentVariableW(Utf8ToWide(name).c_str(), wideBuffer, static_cast<DWORD>(std::size(wideBuffer)));
    if (wideLength == 0 || wideLength >= std::size(wideBuffer)) {
        return std::string();
    }
    return WideToUtf8(std::wstring(wideBuffer, wideBuffer + wideLength));
}

bool ReadAllResponseBytes(HINTERNET requestHandle, std::string& outResponseBody, std::string& outError) {
    outResponseBody.clear();

    for (;;) {
        DWORD availableBytes = 0;
        if (!WinHttpQueryDataAvailable(requestHandle, &availableBytes)) {
            outError = "WinHttpQueryDataAvailable failed";
            return false;
        }

        if (availableBytes == 0) {
            return true;
        }

        std::string chunk(static_cast<size_t>(availableBytes), '\0');
        DWORD bytesRead = 0;
        if (!WinHttpReadData(requestHandle, &chunk[0], availableBytes, &bytesRead)) {
            outError = "WinHttpReadData failed";
            return false;
        }

        chunk.resize(bytesRead);
        outResponseBody += chunk;
    }
}

bool ExtractResponseText(const json& responseJson, std::string& outText) {
    if (responseJson.contains("output_text") && responseJson["output_text"].is_string()) {
        outText = responseJson["output_text"].get<std::string>();
        return !outText.empty();
    }

    if (!responseJson.contains("output") || !responseJson["output"].is_array()) {
        return false;
    }

    for (const json& outputItem : responseJson["output"]) {
        if (!outputItem.contains("content") || !outputItem["content"].is_array()) {
            continue;
        }

        for (const json& contentItem : outputItem["content"]) {
            if (contentItem.contains("text") && contentItem["text"].is_string()) {
                outText = contentItem["text"].get<std::string>();
                if (!outText.empty()) {
                    return true;
                }
            }
        }
    }

    return false;
}

} // namespace

bool OpenAIMassingService::IsConfigured() {
    return !GetEnvironmentVariableUtf8("OPENAI_API_KEY").empty();
}

bool OpenAIMassingService::GenerateRuleJson(const std::string& hiddenContext,
                                            const std::string& userPrompt,
                                            const std::string& currentRuleJson,
                                            OpenAIMassingResponse& outResponse,
                                            std::string& outError) {
    outResponse = OpenAIMassingResponse();

    const std::string apiKey = GetEnvironmentVariableUtf8("OPENAI_API_KEY");
    if (apiKey.empty()) {
        outError = "OPENAI_API_KEY is not configured";
        return false;
    }

    const std::string model = !GetEnvironmentVariableUtf8("OPENAI_MODEL").empty()
        ? GetEnvironmentVariableUtf8("OPENAI_MODEL")
        : "gpt-5-mini";

    std::ostringstream instructions;
    instructions
        << "You are generating massing rule JSON for a building design editor.\n"
        << "Return only data that fits the provided JSON schema.\n"
        << "The rule_json string must itself be valid massing rule JSON.\n"
        << "Use the hidden rule spec and building guide as authoritative.\n"
        << "Prefer realistic architectural massing over abstract sculpture unless explicitly requested.\n\n"
        << "Hidden context follows:\n"
        << hiddenContext;

    std::ostringstream userInput;
    userInput
        << "User prompt:\n" << userPrompt << "\n\n"
        << "Current rule JSON:\n";

    if (currentRuleJson.empty()) {
        userInput << "(none)\n";
    } else {
        userInput << currentRuleJson << "\n";
    }

    json requestBody = {
        {"model", model},
        {"store", false},
        {"input", json::array({
            {
                {"role", "user"},
                {"content", json::array({
                    {
                        {"type", "input_text"},
                        {"text", userInput.str()}
                    }
                })}
            }
        })},
        {"instructions", instructions.str()},
        {"text", {
            {"format", {
                {"type", "json_schema"},
                {"name", "massing_generation_result"},
                {"strict", true},
                {"schema", {
                    {"type", "object"},
                    {"properties", {
                        {"rule_json", {
                            {"type", "string"},
                            {"description", "Valid Moon massing rule JSON as a single string."}
                        }},
                        {"strategy", {
                            {"type", "string"},
                            {"description", "Short description of how the design was generated."}
                        }},
                        {"notes", {
                            {"type", "array"},
                            {"items", {{"type", "string"}}}
                        }}
                    }},
                    {"required", json::array({"rule_json", "strategy", "notes"})},
                    {"additionalProperties", false}
                }}
            }}
        }}
    };

    const std::string project = GetEnvironmentVariableUtf8("OPENAI_PROJECT");
    const std::string organization = GetEnvironmentVariableUtf8("OPENAI_ORGANIZATION");

    HINTERNET sessionHandle = WinHttpOpen(L"MoonEditor/1.0",
                                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                          WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS,
                                          0);
    if (!sessionHandle) {
        outError = "WinHttpOpen failed";
        return false;
    }

    WinHttpSetTimeouts(sessionHandle, 10000, 10000, 10000, 120000);

    HINTERNET connectionHandle = WinHttpConnect(sessionHandle, kOpenAIHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connectionHandle) {
        outError = "WinHttpConnect failed";
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    HINTERNET requestHandle = WinHttpOpenRequest(connectionHandle,
                                                 L"POST",
                                                 kResponsesPath,
                                                 nullptr,
                                                 WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                 WINHTTP_FLAG_SECURE);
    if (!requestHandle) {
        outError = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += Utf8ToWide("Authorization: Bearer " + apiKey) + L"\r\n";
    if (!project.empty()) {
        headers += Utf8ToWide("OpenAI-Project: " + project) + L"\r\n";
    }
    if (!organization.empty()) {
        headers += Utf8ToWide("OpenAI-Organization: " + organization) + L"\r\n";
    }

    const std::string requestJson = requestBody.dump();
    const BOOL sent = WinHttpSendRequest(requestHandle,
                                         headers.c_str(),
                                         static_cast<DWORD>(headers.size()),
                                         reinterpret_cast<LPVOID>(const_cast<char*>(requestJson.data())),
                                         static_cast<DWORD>(requestJson.size()),
                                         static_cast<DWORD>(requestJson.size()),
                                         0);
    if (!sent) {
        outError = "WinHttpSendRequest failed";
        WinHttpCloseHandle(requestHandle);
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    if (!WinHttpReceiveResponse(requestHandle, nullptr)) {
        outError = "WinHttpReceiveResponse failed";
        WinHttpCloseHandle(requestHandle);
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(requestHandle,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,
                             &statusCode,
                             &statusCodeSize,
                             WINHTTP_NO_HEADER_INDEX)) {
        outError = "WinHttpQueryHeaders failed";
        WinHttpCloseHandle(requestHandle);
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    std::string responseBody;
    if (!ReadAllResponseBytes(requestHandle, responseBody, outError)) {
        WinHttpCloseHandle(requestHandle);
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    WinHttpCloseHandle(requestHandle);
    WinHttpCloseHandle(connectionHandle);
    WinHttpCloseHandle(sessionHandle);

    json responseJson = json::parse(responseBody, nullptr, false);
    if (responseJson.is_discarded()) {
        outError = "OpenAI response was not valid JSON";
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (responseJson.contains("error") && responseJson["error"].contains("message")) {
            outError = responseJson["error"]["message"].get<std::string>();
        } else {
            outError = "OpenAI request failed with status " + std::to_string(statusCode);
        }
        return false;
    }

    std::string resultText;
    if (!ExtractResponseText(responseJson, resultText)) {
        outError = "OpenAI response did not contain output text";
        return false;
    }

    json resultJson = json::parse(resultText, nullptr, false);
    if (resultJson.is_discarded()) {
        outError = "Structured OpenAI result was not valid JSON";
        return false;
    }

    if (!resultJson.contains("rule_json") || !resultJson["rule_json"].is_string()) {
        outError = "Structured OpenAI result did not contain rule_json";
        return false;
    }

    outResponse.ruleJson = resultJson["rule_json"].get<std::string>();
    outResponse.strategy = resultJson.value("strategy", "openai_generated");
    outResponse.notes = resultJson.value("notes", std::vector<std::string>());
    outResponse.model = responseJson.value("model", model);
    outResponse.responseId = responseJson.value("id", std::string());
    return true;
}

} // namespace Massing
} // namespace Moon
