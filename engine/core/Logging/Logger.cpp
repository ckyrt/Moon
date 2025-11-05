#include "Logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cstdarg>
#include <vector>
#include <cerrno>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace Moon {
namespace Core {

    // Static member definitions
    std::unique_ptr<Logger> Logger::s_instance = nullptr;
    std::mutex Logger::s_mutex;

    bool Logger::Init() {
        std::lock_guard<std::mutex> lock(s_mutex);
        
        if (s_instance && s_instance->m_initialized) {
            return true; // Already initialized
        }
        
        if (!s_instance) {
            s_instance = std::make_unique<Logger>();
        }
        
        try {
            // Set log directory to absolute path based on executable location
            char exePath[MAX_PATH];
            GetModuleFileNameA(NULL, exePath, MAX_PATH);
            std::string exeDir = std::string(exePath);
            size_t lastSlash = exeDir.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                exeDir = exeDir.substr(0, lastSlash);
            }
            s_instance->m_logDirectory = exeDir + "\\logs";
            
            // Create log directory
            if (!s_instance->CreateLogDirectory()) {
                std::cerr << "Failed to create log directory: " << s_instance->m_logDirectory << std::endl;
                return false;
            }
            
            // Setup console
#ifdef _WIN32
            s_instance->m_consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            
            // Enable ANSI escape sequence support (Windows 10+)
            DWORD mode = 0;
            GetConsoleMode(s_instance->m_consoleHandle, &mode);
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(s_instance->m_consoleHandle, mode);
#endif
            
            // Open log file
            if (!s_instance->OpenLogFile()) {
                std::cerr << "Failed to open log file" << std::endl;
                return false;
            }
            
            s_instance->m_initialized = true;
            
            // Write initialization log after everything is set up
            Write(LogLevel::Info, "Logger", "Logger system initialized successfully");
            
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Logger initialization failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    void Logger::Shutdown() {
        std::lock_guard<std::mutex> lock(s_mutex);
        
        if (s_instance && s_instance->m_initialized) {
            // Write shutdown message directly to avoid macro recursion
            try {
                if (s_instance->m_logFile.is_open()) {
                    std::string timestamp = s_instance->GetTimestampString();
                    s_instance->m_logFile << "[" << timestamp << "] [INFO] [Logger] Logger system shutting down" << std::endl;
                    s_instance->m_logFile.flush();
                }
            }
            catch (...) {
                // Ignore any errors during shutdown logging
            }
            
            // Close file
            if (s_instance->m_logFile.is_open()) {
                s_instance->m_logFile.close();
            }
            
            s_instance->m_initialized = false;
            
            // Reset the unique_ptr to properly destroy the instance
            s_instance.reset();
        }
    }
    
    void Logger::Write(LogLevel level, const char* module, const char* format, ...) {
        std::lock_guard<std::mutex> lock(s_mutex);
        
        if (!s_instance || !s_instance->m_initialized) {
            return; // Logger not initialized
        }
        
        va_list args;
        va_start(args, format);
        s_instance->WriteInternal(level, module, format, args);
        va_end(args);
    }
    
    bool Logger::IsInitialized() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_instance && s_instance->m_initialized;
    }
    
    Logger& Logger::GetInstance() {
        return *s_instance;
    }
    
    void Logger::WriteInternal(LogLevel level, const char* module, const char* format, va_list args) {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        
        // Check log rotation
        CheckLogRotation();
        
        // Format message
        std::string message = FormatLogMessage(format, args);
        
        // Get timestamp
        std::string timestamp = GetTimestampString();
        
        // Format complete log line
        std::stringstream logLine;
        logLine << "[" << timestamp << "] "
                << "[" << GetLogLevelString(level) << "] "
                << "[" << module << "] "
                << message;
        
        std::string fullMessage = logLine.str();
        
        // Write to file
        if (m_logFile.is_open()) {
            m_logFile << fullMessage << std::endl;
            m_logFile.flush(); // Ensure immediate write
        }
        
        // Console output
#ifdef _DEBUG
        // Debug mode: output to console and file
        std::cout << GetConsoleColor(level) << fullMessage;
        ResetConsoleColor();
        std::cout << std::endl;
#endif
        // Release mode: only output to file
    }
    
    std::string Logger::GetCurrentDateString() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        struct tm timeinfo;
#ifdef _WIN32
        localtime_s(&timeinfo, &time_t);
#else
        localtime_r(&time_t, &timeinfo);
#endif
        
        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%Y-%m-%d");
        return ss.str();
    }
    
    std::string Logger::GetTimestampString() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        struct tm timeinfo;
#ifdef _WIN32
        localtime_s(&timeinfo, &time_t);
#else
        localtime_r(&time_t, &timeinfo);
#endif
        
        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
    
    const char* Logger::GetLogLevelString(LogLevel level) const {
        switch (level) {
            case LogLevel::Info:  return "INFO";
            case LogLevel::Warn:  return "WARN";
            case LogLevel::Error: return "ERROR";
            default:              return "UNKNOWN";
        }
    }
    
    const char* Logger::GetConsoleColor(LogLevel level) const {
#ifdef _WIN32
        switch (level) {
            case LogLevel::Info:  return "\033[37m";    // White
            case LogLevel::Warn:  return "\033[33m";    // Yellow
            case LogLevel::Error: return "\033[31m";    // Red
            default:              return "\033[37m";    // Default white
        }
#else
        switch (level) {
            case LogLevel::Info:  return "\033[0;37m";  // White
            case LogLevel::Warn:  return "\033[0;33m";  // Yellow
            case LogLevel::Error: return "\033[0;31m";  // Red
            default:              return "\033[0;37m";  // Default white
        }
#endif
    }
    
    void Logger::ResetConsoleColor() const {
#ifdef _WIN32
        std::cout << "\033[0m";
#else
        std::cout << "\033[0m";
#endif
    }
    
    bool Logger::CreateLogDirectory() {
        try {
#ifdef _WIN32
            if (_mkdir(m_logDirectory.c_str()) == 0 || errno == EEXIST) {
                return true;
            }
#else
            if (mkdir(m_logDirectory.c_str(), 0755) == 0 || errno == EEXIST) {
                return true;
            }
#endif
            return false;
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to create log directory: " << e.what() << std::endl;
            return false;
        }
    }
    
    void Logger::CheckLogRotation() {
        std::string currentDate = GetCurrentDateString();
        
        if (m_currentLogDate != currentDate) {
            // Need to rotate to new file
            if (m_logFile.is_open()) {
                m_logFile.close();
            }
            
            m_currentLogDate = currentDate;
            OpenLogFile();
        }
    }
    
    bool Logger::OpenLogFile() {
        std::string currentDate = GetCurrentDateString();
#ifdef _WIN32
        std::string logFileName = m_logDirectory + "\\" + currentDate + ".log";
#else
        std::string logFileName = m_logDirectory + "/" + currentDate + ".log";
#endif
        
        m_logFile.open(logFileName, std::ios::app);
        
        if (!m_logFile.is_open()) {
            std::cerr << "Failed to open log file: " << logFileName << std::endl;
            return false;
        }
        
        m_currentLogDate = currentDate;
        return true;
    }
    
    std::string Logger::FormatLogMessage(const char* format, va_list args) const {
        // First try with small buffer
        char buffer[1024];
        va_list argsCopy;
        
        va_copy(argsCopy, args);
        int size = vsnprintf(buffer, sizeof(buffer), format, argsCopy);
        va_end(argsCopy);
        
        if (size < 0) {
            return "Format error";
        }
        
        if (size < sizeof(buffer)) {
            return std::string(buffer, size);
        }
        
        // Need larger buffer
        std::vector<char> largeBuffer(size + 1);
        va_copy(argsCopy, args);
        vsnprintf(largeBuffer.data(), largeBuffer.size(), format, argsCopy);
        va_end(argsCopy);
        
        return std::string(largeBuffer.data(), size);
    }

} // namespace Core
} // namespace Moon