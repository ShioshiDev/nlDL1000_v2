#pragma once
#ifndef __MODBUS_MONITOR_SERVICE_H__
#define __MODBUS_MONITOR_SERVICE_H__

#include <Arduino.h>
#include <HardwareSerial.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include "ModbusClientRTU.h"
#include "services/baseService.h"
#include "definitions.h"

// ModBus configuration structure
struct ModbusConfig
{
    uint32_t baudRate = 9600;     // Default baud rate
    uint8_t slaveId = 0x0A;       // Default slave ID
    bool outputToSerial = true;   // Output debug to serial
    bool outputToFile = false;     // Output to file/SD
    bool outputToMQTT = false;     // Output to MQTT
};

// ModBus response data structure
struct ModbusResponseData
{
    uint8_t slaveId;
    uint8_t functionCode;
    std::vector<uint16_t> registers;
    unsigned long timestamp;
    bool hasError;
    uint8_t errorCode;
};

class ModbusMonitorService : public BaseService
{
public:
    ModbusMonitorService();
    ~ModbusMonitorService();

    // BaseService implementation
    void begin() override;
    void loop() override;
    void stop() override;
    void start() override;

    // ModBus monitor specific functions
    ModbusMonitorStatus getModbusStatus() const;
    void setModbusConfig(const ModbusConfig& config);
    ModbusConfig getModbusConfig() const;
    
    // Configuration setters
    void setBaudRate(uint32_t baudRate);
    void setSlaveId(uint8_t slaveId);
    void setOutputFlags(bool serial, bool file, bool mqtt);
    
    // Statistics
    unsigned long getFramesReceived() const;
    unsigned long getValidFrames() const;
    unsigned long getInvalidFrames() const;
    unsigned long getLastActivityTime() const;

    // Response data access
    bool getLastValidResponse(ModbusResponseData& response) const;

private:
    // eModbus client
    ModbusClientRTU* modbusClient;
    HardwareSerial* modbusSerial;
    bool clientInitialized;
    
    // Configuration
    ModbusConfig config;
    
    // Status tracking
    ModbusMonitorStatus modbusStatus;
    unsigned long lastActivityTime;
    unsigned long lastValidFrameTime;
    
    // Statistics
    unsigned long framesReceived;
    unsigned long validFrames;
    unsigned long invalidFrames;
    
    // Response data
    ModbusResponseData lastValidResponse;
    
    // Thread safety
    SemaphoreHandle_t statusMutex;
    SemaphoreHandle_t configMutex;
    SemaphoreHandle_t responseMutex;
    
    // Task handle for monitoring task
    TaskHandle_t monitorTaskHandle;
    
    // Periodic request timing
    unsigned long lastRegisterRequestTime;
    uint32_t nextToken;
    
    // Static instance pointer for callbacks
    static ModbusMonitorService* instance;
    
    // Internal methods
    bool initializeModbusClient();
    void deinitializeModbusClient();
    void updateStatus();
    void setModbusStatus(ModbusMonitorStatus status);
    void sendRegister6Request();
    
    // eModbus callback handlers
    void handleModbusData(ModbusMessage response, uint32_t token);
    void handleModbusError(Error error, uint32_t token);
    
    // Static callback wrappers for eModbus
    static void onModbusData(ModbusMessage response, uint32_t token);
    static void onModbusError(Error error, uint32_t token);
    
    // Static task function for FreeRTOS
    static void monitorTask(void* parameter);
    
    // Constants
    static const unsigned long ACTIVITY_TIMEOUT_MS = 5000;     // 5 seconds of no activity = inactive
    static const unsigned long FRAME_TIMEOUT_MS = 250;         // 250ms max frame time
    static const unsigned long STATUS_UPDATE_INTERVAL_MS = 1000; // Update status every second
    static const unsigned long REGISTER_REQUEST_INTERVAL_MS = 2500; // Request register 6 every 2.5 seconds
    static const size_t MAX_FRAME_SIZE = 256;
};

#endif // __MODBUS_MONITOR_SERVICE_H__
