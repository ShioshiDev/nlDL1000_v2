#pragma once
#ifndef __LOGGINGMANAGER_H__
#define __LOGGINGMANAGER_H__

#include <Arduino.h>

// Forward declaration
class NovaLogicService;

// Simple logging manager for ESP32
class LoggingManager {
public:
    LoggingManager();
    ~LoggingManager();
    
    // Basic lifecycle
    bool begin();
    void loop();
    void stop();
    
    // Simple logging functions
    void logError(const char* tag, const char* format, ...);
    void logWarn(const char* tag, const char* format, ...);
    void logInfo(const char* tag, const char* format, ...);
    void logDebug(const char* tag, const char* format, ...);
    
    // MQTT connection events
    void onMQTTConnected();
    void onMQTTDisconnected();
    
    // State queries
    bool isMQTTConnected() const { return mqttConnected; }
    
    // Backward compatibility
    void debugPrintf(const char* format, ...);
    
private:
    bool initialized = false;
    bool mqttConnected = false;  // Track MQTT connectivity state
    NovaLogicService* mqttService = nullptr;
};

// Enhanced logging macros
#define LOG_ERROR(tag, format, ...)   do { if(globalLoggingManager) globalLoggingManager->logError(tag, format, ##__VA_ARGS__); } while(0)
#define LOG_WARN(tag, format, ...)    do { if(globalLoggingManager) globalLoggingManager->logWarn(tag, format, ##__VA_ARGS__); } while(0)
#define LOG_INFO(tag, format, ...)    do { if(globalLoggingManager) globalLoggingManager->logInfo(tag, format, ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(tag, format, ...)   do { if(globalLoggingManager) globalLoggingManager->logDebug(tag, format, ##__VA_ARGS__); } while(0)
#define LOG_VERBOSE(tag, format, ...) do { if(globalLoggingManager) globalLoggingManager->logDebug(tag, format, ##__VA_ARGS__); } while(0)

// Backward compatibility macros that redirect to enhanced logging
#define DEBUG_LOG_PRINTF(tag, format, ...) LOG_DEBUG(tag, format, ##__VA_ARGS__)
#define DEBUG_LOG_PRINTLN(tag, message) LOG_DEBUG(tag, "%s", message)

// Global instance
extern LoggingManager* globalLoggingManager;

#endif // __LOGGINGMANAGER_H__
