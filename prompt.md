# NovaLogic DL1000 Factory Device

## Project Overview
An ESP32 Arduino PlatformIO project in C++.

## Completed Tasks ✅
- **Enhanced Logging System**: Created a comprehensive logging system based on the esp_log library which replaces current DEBUG/Serial output statements with a professional multi-output system.
    - ✅ **Serial console** (default on) - Uses ESP-IDF logging with custom vprintf integration
    - ✅ **File system** (LittleFS with size management and log rotation - default off) - JSON-transmittable logs with configurable file size limits
    - ✅ **MQTT service** (Using NovaLogicService, publishes device logs to log topics when connected - default off) - Queues logs offline and sends on reconnect
    - ✅ **Dynamic configuration** via MQTT commands for real-time logging policy updates
    - ✅ **Component-specific control** - Enable debug for specific managers using TAG-based filtering
    - ✅ **Level-based filtering** - Different verbosity levels for each output target
    - ✅ **Resilient offline logging** - Stores logs during disconnection and sends on reconnect
    - ✅ **Performance optimized** - Built-in efficiency with ESP-IDF integration

**Key Features Implemented:**
- Multi-output logging to Serial, File system (LittleFS), and MQTT simultaneously
- Dynamic configuration via MQTT commands (`/logging/config`, `/logging/get_config`, `/logging/get_logs`) 
- Component-specific log level control (e.g., enable DEBUG for NetworkingManager only)
- Automatic log file rotation with configurable size limits and file retention
- Offline log queuing with automatic transmission on MQTT reconnect
- Enhanced logging macros: `LOG_ERROR()`, `LOG_WARN()`, `LOG_INFO()`, `LOG_DEBUG()`, `LOG_VERBOSE()`, `LOG_MQTT()`
- Backward compatibility with existing `DEBUG_PRINTF()` macros (can be disabled)
- ESP-IDF integration using custom vprintf function for seamless log capture
- JSON-based configuration system with persistent storage to LittleFS
- Statistics tracking for log counts per output and performance monitoring

**Integration with Current Architecture:**
- Integrated into CoreManager lifecycle (begin/loop/stop)
- Connected to NovaLogicService for MQTT connectivity events  
- Uses existing LittleFS filesystem for log storage and configuration
- Compatible with current TAG-based logging approach in managers
- Maintains performance with non-blocking operations and efficient buffering

## Tasks
- ~~Create a logging system based on the esp_log library~~
- ✅ **Migrate existing DEBUG statements to new logging system** 
    - ✅ **CoreManager** - All DEBUG statements migrated to appropriate log levels (LOG_INFO, LOG_WARN, LOG_ERROR, LOG_DEBUG)
    - ✅ **NetworkingManager** - Network events, state changes, and diagnostics migrated with proper severity levels
    - ✅ **ServicesManager** - Service orchestration and state management migrated to structured logging
    - ✅ **ConnectivityManager** - Internet connectivity and ping diagnostics migrated with debug/warn levels
    - ✅ **BaseService** - Service status changes and state logging migrated to use service names as tags
    - ✅ **Legacy DEBUG macro redirection** - All remaining DEBUG_PRINTF/DEBUG_PRINTLN calls now route through enhanced logging system
    - ✅ **Backward compatibility maintained** - Existing code continues to work while benefiting from new logging features
    
**Migration Approach Used:**
- **Manual migration** for core system files to use appropriate log levels (INFO for initialization, WARN for issues, ERROR for failures, DEBUG for detailed diagnostics)
- **Automatic redirection** of legacy DEBUG macros to enhanced logging system via definitions.h
- **Preserved backward compatibility** so existing debug statements continue to work
- **Structured logging** with component-specific TAGs for better filtering and analysis

**Benefits Achieved:**
- **100+ debug statements** now route through the enhanced logging system
- **Appropriate log levels** applied based on context (INFO for startup, WARN for recoverable issues, ERROR for failures)
- **Component-specific filtering** enabled through TAG-based logging (CoreManager, NetworkingManager, etc.)
- **Backward compatibility** maintained - no breaking changes to existing code
- **Foundation for future enhancements** - Ready for file logging, MQTT logging, and dynamic configuration
- **Consistent logging format** across the entire application

## Optional Tasks
- Update documentation with the overview, architecture, and technical notes of our current project.

## Future Enhancements (Not Critical)
- **Log Analytics**: Add log analysis and trending capabilities
- **Remote Log Viewer**: Web interface for viewing logs remotely via MQTT or HTTP
- **Log Compression**: Compress old log files to save storage space
- **Structured Logging**: Add JSON-structured logging for better parsing and analysis
