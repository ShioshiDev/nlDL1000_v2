#include "loggingManager.h"

// Global instance
LoggingManager* globalLoggingManager = nullptr;

LoggingManager::LoggingManager() {
    globalLoggingManager = this;
}

LoggingManager::~LoggingManager() {
    stop();
    if (globalLoggingManager == this) {
        globalLoggingManager = nullptr;
    }
}

bool LoggingManager::begin() {
    if (initialized) {
        return true;
    }
    
    initialized = true;
    Serial.printf("[LoggingManager] Enhanced logging system initialized\n");
    return true;
}

void LoggingManager::loop() {
    // Simple implementation - no periodic tasks yet
}

void LoggingManager::stop() {
    if (!initialized) {
        return;
    }
    
    initialized = false;
    Serial.printf("[LoggingManager] Logging system stopped\n");
}

void LoggingManager::logError(const char* tag, const char* format, ...) {
    if (!initialized) return;
    
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Serial.printf("[%lu][ERROR][%s] %s\n", millis(), tag, buffer);
}

void LoggingManager::logWarn(const char* tag, const char* format, ...) {
    if (!initialized) return;
    
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Serial.printf("[%lu][WARN][%s] %s\n", millis(), tag, buffer);
}

void LoggingManager::logInfo(const char* tag, const char* format, ...) {
    if (!initialized) return;
    
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Serial.printf("[%lu][INFO][%s] %s\n", millis(), tag, buffer);
}

void LoggingManager::logDebug(const char* tag, const char* format, ...) {
    if (!initialized) return;
    
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Serial.printf("[%lu][DEBUG][%s] %s\n", millis(), tag, buffer);
}

void LoggingManager::onMQTTConnected() {
    if (initialized) {
        mqttConnected = true;
        Serial.printf("[%lu][INFO][LoggingManager] MQTT connectivity established - enhanced logging features available\n", millis());
    }
}

void LoggingManager::onMQTTDisconnected() {
    if (initialized) {
        mqttConnected = false;
        Serial.printf("[%lu][WARN][LoggingManager] MQTT connectivity lost - falling back to serial-only logging\n", millis());
    }
}

void LoggingManager::updateSettings(bool logToFileEnabled, bool logToMQTTEnabled) {
    this->logToFileEnabled = logToFileEnabled;
    this->logToMQTTEnabled = logToMQTTEnabled;
    
    if (initialized) {
        Serial.printf("[%lu][INFO][LoggingManager] Settings updated - File logging: %s, MQTT logging: %s\n", 
                     millis(), 
                     logToFileEnabled ? "enabled" : "disabled",
                     logToMQTTEnabled ? "enabled" : "disabled");
    }
}

void LoggingManager::debugPrintf(const char* format, ...) {
    if (!initialized) return;
    
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Serial.printf("[%lu][DEBUG] %s", millis(), buffer);
}
