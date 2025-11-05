#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <memory>

namespace Moon {
namespace Core {

    enum class LogLevel {
        Info,
        Warn,
        Error
    };

    class Logger {
    public:
        // Initialize logging system
        static bool Init();
        
        // Shutdown logging system
        static void Shutdown();
        
        // Write log message
        static void Write(LogLevel level, const char* module, const char* format, ...);
        
        // Check if initialized
        static bool IsInitialized();
        
        // Destructor needs to be public for unique_ptr access
        ~Logger() = default;
        
        // Constructor needs to be public for make_unique
        Logger() = default;
        
    private:
        // Get singleton instance
        static Logger& GetInstance();
        
        // Internal write method
        void WriteInternal(LogLevel level, const char* module, const char* format, va_list args);
        
        // Get current date string (YYYY-MM-DD)
        std::string GetCurrentDateString() const;
        
        // Get current timestamp string (YYYY-MM-DD HH:MM:SS)
        std::string GetTimestampString() const;
        
        // Get log level string
        const char* GetLogLevelString(LogLevel level) const;
        
        // Get console color code
        const char* GetConsoleColor(LogLevel level) const;
        
        // Reset console color
        void ResetConsoleColor() const;
        
        // Check and create log directory
        bool CreateLogDirectory();
        
        // Check if need to rotate log file (new day)
        void CheckLogRotation();
        
        // Open new log file
        bool OpenLogFile();
        
        // Format message
        std::string FormatLogMessage(const char* format, va_list args) const;
        
    private:
        static std::unique_ptr<Logger> s_instance;
        static std::mutex s_mutex;
        
        std::mutex m_writeMutex;
        std::ofstream m_logFile;
        std::string m_currentLogDate;
        std::string m_logDirectory;
        bool m_initialized = false;
        
        // Windows console handle
        void* m_consoleHandle = nullptr;
    };

} // namespace Core
} // namespace Moon

// Moon logging macros (use these to avoid conflicts)
#define MOON_LOG_INFO(module, format, ...)  Moon::Core::Logger::Write(Moon::Core::LogLevel::Info,  module, format, ##__VA_ARGS__)
#define MOON_LOG_WARN(module, format, ...)  Moon::Core::Logger::Write(Moon::Core::LogLevel::Warn,  module, format, ##__VA_ARGS__)
#define MOON_LOG_ERROR(module, format, ...) Moon::Core::Logger::Write(Moon::Core::LogLevel::Error, module, format, ##__VA_ARGS__)

// Simplified macros (no module name required)
#define MOON_LOG_INFO_SIMPLE(format, ...)   MOON_LOG_INFO("General", format, ##__VA_ARGS__)
#define MOON_LOG_WARN_SIMPLE(format, ...)   MOON_LOG_WARN("General", format, ##__VA_ARGS__)
#define MOON_LOG_ERROR_SIMPLE(format, ...)  MOON_LOG_ERROR("General", format, ##__VA_ARGS__)