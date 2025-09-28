#pragma once
#ifndef __MODBUS_MONITOR_MANAGER_H__
#define __MODBUS_MONITOR_MANAGER_H__

#include <functional>
#include "definitions.h"
#include "statusViewModel.h"
#include "services/modbusMonitorService.h"

// Forward declarations
class StatusViewModel;

class ModbusMonitorManager
{
public:
    ModbusMonitorManager(StatusViewModel &statusVM);
    ~ModbusMonitorManager();

    void begin();
    void loop();
    void stop();

    // Status access
    ModbusMonitorStatus getModbusStatus() const;
    bool isMonitoring() const;
    
    // Configuration management
    void setBaudRate(uint32_t baudRate);
    void setSlaveId(uint8_t slaveId);
    void setOutputFlags(bool serial, bool file, bool mqtt);
    ModbusConfig getConfiguration() const;
    
    // Statistics access
    unsigned long getFramesReceived() const;
    unsigned long getValidFrames() const;
    unsigned long getInvalidFrames() const;
    unsigned long getLastActivityTime() const;
    
    // Response data access
    bool getLastValidResponse(ModbusResponseData& response) const;
    
    // Configuration persistence
    void loadConfiguration();
    void saveConfiguration();
    
    // Service access
    ModbusMonitorService& getService() { return modbusService; }

    // Set callback for status change events
    void setStatusChangeCallback(std::function<void(ModbusMonitorStatus)> callback);

private:
    // Update status in view model
    void updateStatusViewModel();
    void onServiceStatusChange(ServiceStatus status);
    void onModbusStatusChange();
    
    // Dependencies
    StatusViewModel &statusViewModel;
    
    // Service
    ModbusMonitorService modbusService;
    
    // State management
    bool initialized;
    ModbusMonitorStatus lastModbusStatus;
    std::function<void(ModbusMonitorStatus)> statusChangeCallback;
    
    // Timing
    unsigned long lastStatusUpdate;
    
    // Constants
    static const unsigned long STATUS_UPDATE_INTERVAL_MS = 1000; // Update status every second
    
    // Configuration keys for NVS storage
    static const char* NVS_NAMESPACE;
    static const char* NVS_KEY_BAUD_RATE;
    static const char* NVS_KEY_SLAVE_ID;
    static const char* NVS_KEY_OUTPUT_SERIAL;
    static const char* NVS_KEY_OUTPUT_FILE;
    static const char* NVS_KEY_OUTPUT_MQTT;
};

#endif // __MODBUS_MONITOR_MANAGER_H__
