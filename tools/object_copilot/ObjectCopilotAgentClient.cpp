#include "ObjectCopilotAgentClient.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace Moon {
namespace Tooling {

namespace {

struct ParsedUrl {
    std::wstring host;
    INTERNET_PORT port = 0;
    std::wstring path;
    bool secure = false;
};

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

bool ParseHttpUrl(const std::string& url, const std::string& endpointPath, ParsedUrl& outUrl, std::string& outError) {
    URL_COMPONENTSW components = {};
    components.dwStructSize = sizeof(components);

    wchar_t hostBuffer[256];
    wchar_t pathBuffer[1024];
    components.lpszHostName = hostBuffer;
    components.dwHostNameLength = static_cast<DWORD>(std::size(hostBuffer));
    components.lpszUrlPath = pathBuffer;
    components.dwUrlPathLength = static_cast<DWORD>(std::size(pathBuffer));

    std::wstring wideUrl = Utf8ToWide(url);
    if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &components)) {
        outError = "Failed to parse agent service URL";
        return false;
    }

    outUrl.host.assign(components.lpszHostName, components.dwHostNameLength);
    outUrl.port = components.nPort;
    outUrl.secure = components.nScheme == INTERNET_SCHEME_HTTPS;

    std::wstring basePath(components.lpszUrlPath, components.dwUrlPathLength);
    if (basePath.empty()) {
        basePath = L"";
    }

    std::wstring endpoint = Utf8ToWide(endpointPath);
    if (!basePath.empty() && basePath.back() == L'/') {
        basePath.pop_back();
    }
    outUrl.path = basePath + endpoint;
    return true;
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

} // namespace

bool ObjectCopilotAgentClient::RequestPatch(const std::string& serviceUrl,
                                            const std::string& currentObjectJson,
                                            const nlohmann::json& conversation,
                                            const std::string& userPrompt,
                                            AgentPatchResponse& outResponse,
                                            std::string& outError) {
    ParsedUrl parsedUrl;
    if (!ParseHttpUrl(serviceUrl, "/agent/patch", parsedUrl, outError)) {
        return false;
    }

    const nlohmann::json requestJson = {
        {"current_object_json", currentObjectJson},
        {"conversation", conversation},
        {"user_prompt", userPrompt}
    };
    const std::string requestBody = requestJson.dump();

    HINTERNET sessionHandle = WinHttpOpen(L"MoonObjectCopilot/0.1",
                                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                          WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS,
                                          0);
    if (!sessionHandle) {
        outError = "WinHttpOpen failed";
        return false;
    }

    HINTERNET connectionHandle = WinHttpConnect(sessionHandle,
                                                parsedUrl.host.c_str(),
                                                parsedUrl.port,
                                                0);
    if (!connectionHandle) {
        outError = "WinHttpConnect failed";
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    const DWORD requestFlags = parsedUrl.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET requestHandle = WinHttpOpenRequest(connectionHandle,
                                                 L"POST",
                                                 parsedUrl.path.c_str(),
                                                 nullptr,
                                                 WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                 requestFlags);
    if (!requestHandle) {
        outError = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(sessionHandle);
        return false;
    }

    const wchar_t* headers = L"Content-Type: application/json\r\n";
    const BOOL sendOk = WinHttpSendRequest(requestHandle,
                                           headers,
                                           static_cast<DWORD>(-1L),
                                           const_cast<char*>(requestBody.data()),
                                           static_cast<DWORD>(requestBody.size()),
                                           static_cast<DWORD>(requestBody.size()),
                                           0);
    if (!sendOk) {
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

    if (statusCode < 200 || statusCode >= 300) {
        outError = "Agent service returned HTTP " + std::to_string(statusCode) + ": " + responseBody;
        return false;
    }

    nlohmann::json parsedResponse = nlohmann::json::parse(responseBody, nullptr, false);
    if (parsedResponse.is_discarded() || !parsedResponse.is_object()) {
        outError = "Agent response was not valid JSON";
        return false;
    }

    outResponse.summary = parsedResponse.value("summary", std::string());
    outResponse.rawResponseJson = parsedResponse.dump(2);
    if (!parsedResponse.contains("updated_object_json") || !parsedResponse["updated_object_json"].is_string()) {
        outError = "Agent response did not include updated_object_json";
        return false;
    }
    outResponse.updatedObjectJson = parsedResponse["updated_object_json"].get<std::string>();
    return true;
}

} // namespace Tooling
} // namespace Moon
