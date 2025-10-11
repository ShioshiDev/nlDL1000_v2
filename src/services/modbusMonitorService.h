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
#include "modbusData.h"

// ModBus configuration structure
struct ModbusConfig
{
    uint32_t baudRate = 115200;     // Default baud rate
    uint8_t slaveId = 0x0A;       // Default slave ID (DSE controller)
    bool outputToSerial = true;   // Output debug to serial
    bool outputToFile = false;     // Output to file/SD
    bool outputToMQTT = false;     // Output to MQTT
};

// DSE Data storage structure
struct DSEData
{
    DSEPage4_BasicInstrumentation page4;
    DSEPage5_ExtendedInstrumentation page5;
    DSEPage6_DerivedInstrumentation page6;
    DSEPage7_AccumulatedInstrumentation page7;
    
    bool page4Valid = false;
    bool page5Valid = false;
    bool page6Valid = false;
    bool page7Valid = false;
    
    unsigned long lastUpdateTime = 0;
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

    // DSE Data access
    bool getDSEData(DSEData& data) const;
    float getGeneratorTotalWatts() const;
    float getGeneratorL1NVoltage() const;

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
    
    // DSE Data storage
    DSEData dseData;
    
    // Thread safety
    SemaphoreHandle_t statusMutex;
    SemaphoreHandle_t configMutex;
    SemaphoreHandle_t dataMutex;
    
    // Periodic request timing
    unsigned long lastRequestTime;
    uint8_t currentPage;           // Current page being requested (4-7)
    uint32_t nextToken;
    
    // Static instance pointer for callbacks
    static ModbusMonitorService* instance;
    
    // Internal methods
    bool initializeModbusClient();
    void deinitializeModbusClient();
    void updateStatus();
    void setModbusStatus(ModbusMonitorStatus status);
    void requestNextPage();
    void processPageResponse(ModbusMessage response, uint8_t pageNum);
    
    // eModbus callback handlers
    void handleModbusData(ModbusMessage response, uint32_t token);
    void handleModbusError(Error error, uint32_t token);
    
    // Static callback wrappers for eModbus
    static void onModbusData(ModbusMessage response, uint32_t token);
    static void onModbusError(Error error, uint32_t token);
    
    // Constants
    static const unsigned long ACTIVITY_TIMEOUT_MS = 15000;    // 15 seconds of no activity = inactive
    static const unsigned long REQUEST_INTERVAL_MS = 10000;    // Request data every 10 seconds
    static const unsigned long STATUS_UPDATE_INTERVAL_MS = 1000; // Update status every second
};

#endif // __MODBUS_MONITOR_SERVICE_H__