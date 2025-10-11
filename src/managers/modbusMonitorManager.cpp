#include "managers/modbusMonitorManager.h"
#include "managers/loggingManager.h"

static const char *TAG = "ModbusMonitorManager";

ModbusMonitorManager::ModbusMonitorManager(StatusViewModel &statusVM)
    : statusViewModel(statusVM),
      modbusService(),
      lastReportedStatus(MODBUS_INACTIVE),
      statusChangeCallback(nullptr)
{
    LOG_INFO(TAG, "ModbusMonitorManager initialized");
}

ModbusMonitorManager::~ModbusMonitorManager()
{
    stop();
    LOG_INFO(TAG, "ModbusMonitorManager destroyed");
}

void ModbusMonitorManager::begin()
{
    LOG_INFO(TAG, "Starting Modbus Monitor Manager...");
    
    // Start the service
    modbusService.begin();
    
    // Set initial status
    lastReportedStatus = modbusService.getModbusStatus();
    updateStatusViewModel();
    
    LOG_INFO(TAG, "Modbus Monitor Manager started");
}

void ModbusMonitorManager::loop()
{
    // Update the service
    modbusService.loop();
    
    // Check for status changes
    ModbusMonitorStatus currentStatus = modbusService.getModbusStatus();
    if (currentStatus != lastReportedStatus)
    {
        lastReportedStatus = currentStatus;
        updateStatusViewModel();
        
        // Call status change callback if set
        if (statusChangeCallback)
        {
            statusChangeCallback(currentStatus);
        }
        
        LOG_DEBUG(TAG, "Status changed to: %d", currentStatus);
    }
}

void ModbusMonitorManager::stop()
{
    LOG_INFO(TAG, "Stopping Modbus Monitor Manager...");
    modbusService.stop();
    lastReportedStatus = MODBUS_INACTIVE;
    updateStatusViewModel();
}

ModbusMonitorStatus ModbusMonitorManager::getModbusStatus() const
{
    return modbusService.getModbusStatus();
}

bool ModbusMonitorManager::isMonitoring() const
{
    return modbusService.isConnected();
}

void ModbusMonitorManager::setBaudRate(uint32_t baudRate)
{
    modbusService.setBaudRate(baudRate);
    LOG_INFO(TAG, "Baud rate set to: %lu", baudRate);
}

void ModbusMonitorManager::setSlaveId(uint8_t slaveId)
{
    modbusService.setSlaveId(slaveId);
    LOG_INFO(TAG, "Slave ID set to: 0x%02X", slaveId);
}

void ModbusMonitorManager::setOutputFlags(bool serial, bool file, bool mqtt)
{
    modbusService.setOutputFlags(serial, file, mqtt);
    LOG_INFO(TAG, "Output flags set - Serial: %s, File: %s, MQTT: %s",
             serial ? "ON" : "OFF", file ? "ON" : "OFF", mqtt ? "ON" : "OFF");
}

ModbusConfig ModbusMonitorManager::getConfiguration() const
{
    return modbusService.getModbusConfig();
}

unsigned long ModbusMonitorManager::getFramesReceived() const
{
    return modbusService.getFramesReceived();
}

unsigned long ModbusMonitorManager::getValidFrames() const
{
    return modbusService.getValidFrames();
}

unsigned long ModbusMonitorManager::getInvalidFrames() const
{
    return modbusService.getInvalidFrames();
}

unsigned long ModbusMonitorManager::getLastActivityTime() const
{
    return modbusService.getLastActivityTime();
}

bool ModbusMonitorManager::getDSEData(DSEData& data) const
{
    return modbusService.getDSEData(data);
}

float ModbusMonitorManager::getGeneratorTotalWatts() const
{
    return modbusService.getGeneratorTotalWatts();
}

float ModbusMonitorManager::getGeneratorL1NVoltage() const
{
    return modbusService.getGeneratorL1NVoltage();
}

void ModbusMonitorManager::setStatusChangeCallback(std::function<void(ModbusMonitorStatus)> callback)
{
    statusChangeCallback = callback;
}

void ModbusMonitorManager::updateStatusViewModel()
{
    // Update the status view model with current Modbus status
    statusViewModel.setModbusStatus(lastReportedStatus);
}