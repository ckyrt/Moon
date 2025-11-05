# Moon Game Engine Logging System

The Moon Engine logging system provides thread-safe, high-performance logging functionality with file output, colored console display, and daily log rotation.

## Features

- Thread-safe logging using `std::mutex`
- File logging with automatic date-based rotation (`./logs/YYYY-MM-DD.log`)
- Colored console output (Debug mode only)
- Multiple log levels: Info, Warn, Error
- Module-based logging
- Daily log file rotation
- Automatic directory creation
- C++17 compatible, no external dependencies

## Quick Start

### 1. Initialize Logging System

Call at application startup:

```cpp
#include "core/Logging/Logger.h"

int main() {
    // Initialize logging system
    if (!Moon::Core::Logger::Init()) {
        // Handle initialization failure
        return -1;
    }
    
    // Application logic...
    
    // Shutdown logging system
    Moon::Core::Logger::Shutdown();
    return 0;
}
```

### 2. Basic Logging

```cpp
// Basic usage - specify module name
MOON_LOG_INFO("ModuleName", "This is an info message");
MOON_LOG_WARN("ModuleName", "This is a warning message");
MOON_LOG_ERROR("ModuleName", "This is an error message");

// Formatted strings
MOON_LOG_INFO("Renderer", "Initializing with resolution %dx%d", width, height);
MOON_LOG_WARN("Network", "Connection timeout after %d seconds", timeout);
MOON_LOG_ERROR("FileSystem", "Failed to load file: %s", filename);

// Simplified version - uses default module name "General"
MOON_LOG_INFO_SIMPLE("Application started");
MOON_LOG_WARN_SIMPLE("Low memory warning");
MOON_LOG_ERROR_SIMPLE("Critical error occurred");
```

## Log Levels

### Info (Information)
- **Purpose**: General information logging
- **Color**: White
- **Examples**: System startup, feature initialization, state changes

### Warn (Warning)
- **Purpose**: Potential issues or unusual situations
- **Color**: Yellow
- **Examples**: Resource loading failure with fallback, performance warnings

### Error (Error)
- **Purpose**: Serious errors or failed operations
- **Color**: Red
- **Examples**: Initialization failure, file I/O errors, fatal exceptions

## Output Behavior

### Debug Mode
- Logs output to both **console** and **file**
- Console supports colored display
- Includes detailed debug information

### Release Mode
- Logs output to **file only**
- No console resource usage
- Optimized performance

## Log File Management

### File Location
```
./logs/
├── 2025-11-04.log
├── 2025-11-05.log
└── 2025-11-06.log
```

### File Format
```
[2025-11-05 14:32:15.123] [INFO] [Main] HelloEngine starting up
[2025-11-05 14:32:15.145] [INFO] [DiligentRenderer] Initializing DiligentRenderer...
[2025-11-05 14:32:15.167] [INFO] [DiligentRenderer] D3D11 device and context created
[2025-11-05 14:32:15.189] [WARN] [EngineCore] High frame time detected: 0.125s (8.0 FPS)
[2025-11-05 14:32:15.201] [ERROR] [FileSystem] Failed to load texture: missing.png
```

### Automatic Rotation
- New log file created automatically each day
- Filename format: `YYYY-MM-DD.log`
- Old log files are automatically preserved
- No manual file management required

## Integration Examples

### HelloEngine (Main Program)
```cpp
// Initialization
Moon::Core::Logger::Init();
MOON_LOG_INFO("Main", "HelloEngine starting up");

// Window creation
MOON_LOG_INFO("Main", "Creating window with size %dx%d", width, height);

// FPS monitoring
MOON_LOG_INFO("Main", "FPS: %.1f (frames: %d)", fps, frameCount);

// Shutdown
MOON_LOG_INFO("Main", "HelloEngine shutdown complete");
Moon::Core::Logger::Shutdown();
```

### EngineCore (Core Engine)
```cpp
void EngineCore::Initialize() {
    MOON_LOG_INFO("EngineCore", "Engine initialization starting");
    // Initialization logic...
    MOON_LOG_INFO("EngineCore", "Engine initialization completed successfully");
}

void EngineCore::Tick(double dt) {
    if (dt > 0.1) {
        MOON_LOG_WARN("EngineCore", "High frame time detected: %.3fs", dt);
    }
}
```

### DiligentRenderer (Renderer)
```cpp
bool DiligentRenderer::Initialize(const RenderInitParams& params) {
    MOON_LOG_INFO("DiligentRenderer", "Initializing DiligentRenderer...");
    
    try {
        // Create device...
        MOON_LOG_INFO("DiligentRenderer", "D3D11 device and context created");
        
        // Create swap chain...
        MOON_LOG_INFO("DiligentRenderer", "Swap chain created");
        
        MOON_LOG_INFO("DiligentRenderer", "DiligentRenderer initialized successfully!");
        return true;
    }
    catch (const std::exception& e) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to initialize: %s", e.what());
        return false;
    }
}

void DiligentRenderer::Resize(uint32_t w, uint32_t h) {
    MOON_LOG_INFO("DiligentRenderer", "Resizing viewport from %ux%u to %ux%u", 
                  m_Width, m_Height, w, h);
}
```

## Extending to New Modules

### Step 1: Include Header
```cpp
#include "core/Logging/Logger.h"
```

### Step 2: Define Module Name
```cpp
// Recommended: define module name constant at file top
static const char* MODULE_NAME = "YourModuleName";
```

### Step 3: Use Logging Macros
```cpp
void YourClass::YourMethod() {
    MOON_LOG_INFO(MODULE_NAME, "Method started");
    
    if (someCondition) {
        MOON_LOG_WARN(MODULE_NAME, "Potential issue detected");
    }
    
    if (errorOccurred) {
        MOON_LOG_ERROR(MODULE_NAME, "Operation failed: %s", errorMessage);
        return;
    }
    
    MOON_LOG_INFO(MODULE_NAME, "Method completed successfully");
}
```

## Best Practices

### 1. Module Naming Conventions
- Use clear module names: `"Renderer"`, `"Physics"`, `"Audio"`, `"Network"`
- Sub-modules can use dot notation: `"Renderer.D3D11"`, `"Physics.Collision"`
- Keep names concise but descriptive

### 2. Log Level Selection
```cpp
// Correct usage
MOON_LOG_INFO("Module", "Normal operation info");          // General information
MOON_LOG_WARN("Module", "Unusual but recoverable situation"); // Recoverable issues
MOON_LOG_ERROR("Module", "Serious error requiring attention"); // Serious errors

// Avoid misuse
MOON_LOG_ERROR("Module", "User clicked button");           // This is not an error
MOON_LOG_INFO("Module", "Fatal crash occurred");          // This should be ERROR
```

### 3. Performance Considerations
```cpp
// Use logging sparingly in high-frequency operations
void TickFunction() {
    static int counter = 0;
    if (++counter % 100 == 0) {  // Log every 100 frames
        MOON_LOG_INFO("Engine", "Tick frame #%d", counter);
    }
}

// Avoid logging every frame
void RenderFrame() {
    MOON_LOG_INFO("Renderer", "Rendering frame");  // 60 FPS = 60 logs/second - BAD
}
```

### 4. Format Strings
```cpp
// Correct formatting
MOON_LOG_INFO("Module", "Loading %d assets from %s", count, path);
MOON_LOG_ERROR("Module", "Failed to allocate %zu bytes", size);

// Avoid string concatenation
std::string msg = "Loading " + std::to_string(count) + " assets";
MOON_LOG_INFO("Module", msg.c_str());  // Less efficient
```

## Troubleshooting

### Problem: Log files not created
**Cause**: Permission issues or insufficient disk space
**Solution**: Check write permissions in program directory

### Problem: No colored console output
**Cause**: Windows terminal doesn't support ANSI escape sequences
**Solution**: Use Windows Terminal or other supporting terminal

### Problem: Logger initialization failure
**Cause**: Cannot create log directory or file
**Solution**: Check `Logger::Init()` return value and handle errors

### Problem: No logs visible in Release mode
**Cause**: Release mode only outputs to file
**Solution**: Check `./logs/` directory for log files

## API Reference

### Core Methods
```cpp
namespace Moon::Core {
    class Logger {
    public:
        static bool Init();                    // Initialize logging system
        static void Shutdown();                // Shutdown logging system
        static bool IsInitialized();          // Check if initialized
        static void Write(LogLevel level, const char* module, 
                         const char* format, ...);  // Write log
    };
    
    enum class LogLevel {
        Info,   // Information level
        Warn,   // Warning level
        Error   // Error level
    };
}
```

### Convenience Macros
```cpp
// Basic macros (require module name)
MOON_LOG_INFO(module, format, ...)
MOON_LOG_WARN(module, format, ...)
MOON_LOG_ERROR(module, format, ...)

// Simplified macros (use default module name "General")
MOON_LOG_INFO_SIMPLE(format, ...)
MOON_LOG_WARN_SIMPLE(format, ...)
MOON_LOG_ERROR_SIMPLE(format, ...)

// Compatibility macros (if no conflicts)
LOG_INFO(module, format, ...)     // Available if LOG_INFO not defined elsewhere
LOG_WARN(module, format, ...)     // Available if LOG_WARN not defined elsewhere
LOG_ERROR(module, format, ...)    // Available if LOG_ERROR not defined elsewhere
```

## Version History

### v1.0.0 (2025-11-05)
- Basic logging system implementation
- Thread-safe mechanisms
- File and console output
- Daily log rotation
- Colored console support
- Debug/Release mode distinction
- Macro conflict resolution (MOON_ prefix)

---

**Moon Game Engine Logging System** - Reliable logging functionality for your game development.