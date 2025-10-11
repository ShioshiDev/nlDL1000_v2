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
    
    // DSE Data access
    bool getDSEData(DSEData& data) const;
    float getGeneratorTotalWatts() const;
    float getGeneratorL1NVoltage() const;
    
    // Service access
    ModbusMonitorService& getService() { return modbusService; }

    // Set callback for status change events
    void setStatusChangeCallback(std::function<void(ModbusMonitorStatus)> callback);

private:
    StatusViewModel &statusViewModel;
    ModbusMonitorService modbusService;
    
    ModbusMonitorStatus lastReportedStatus;
    std::function<void(ModbusMonitorStatus)> statusChangeCallback;
    
    void updateStatusViewModel();
};

#endif // __MODBUS_MONITOR_MANAGER_H__