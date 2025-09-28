#include "managers/modbusMonitorManager.h"
#include <Preferences.h>

// Static constants
const char* ModbusMonitorManager::NVS_NAMESPACE = "modbus_cfg";
const char* ModbusMonitorManager::NVS_KEY_BAUD_RATE = "baud_rate";
const char* ModbusMonitorManager::NVS_KEY_SLAVE_ID = "slave_id";
const char* ModbusMonitorManager::NVS_KEY_OUTPUT_SERIAL = "out_serial";
const char* ModbusMonitorManager::NVS_KEY_OUTPUT_FILE = "out_file";
const char* ModbusMonitorManager::NVS_KEY_OUTPUT_MQTT = "out_mqtt";

ModbusMonitorManager::ModbusMonitorManager(StatusViewModel &statusVM)
    : statusViewModel(statusVM)
    , modbusService()
    , initialized(false)
    , lastModbusStatus(MODBUS_INACTIVE)
    , lastStatusUpdate(0)
{
    // Set up service status change callback
    modbusService.setStatusChangeCallback([this](ServiceStatus status) {
        onServiceStatusChange(status);
    });
}

ModbusMonitorManager::~ModbusMonitorManager()
{
    stop();
}

void ModbusMonitorManager::begin()
{
    if (initialized) {
        LOG_DEBUG("ModbusMonitorManager", "Already initialized");
        return;
    }
    
    LOG_INFO("ModbusMonitorManager", "Starting ModBus Monitor Manager");
    
    // Load configuration from NVS
    loadConfiguration();
    
    // Start the ModBus monitoring service
    modbusService.begin();
    
    initialized = true;
    lastStatusUpdate = millis();
    
    LOG_INFO("ModbusMonitorManager", "ModBus Monitor Manager started successfully");
}

void ModbusMonitorManager::loop()
{
    if (!initialized) {
        return;
    }
    
    // Update the service
    modbusService.loop();
    
    // Periodic status updates
    unsigned long currentTime = millis();
    if (currentTime - lastStatusUpdate >= STATUS_UPDATE_INTERVAL_MS) {
        updateStatusViewModel();
        onModbusStatusChange();
        lastStatusUpdate = currentTime;
    }
}

void ModbusMonitorManager::stop()
{
    if (!initialized) {
        return;
    }
    
    LOG_INFO("ModbusMonitorManager", "Stopping ModBus Monitor Manager");
    
    // Stop the service
    modbusService.stop();
    
    initialized = false;
    
    LOG_INFO("ModbusMonitorManager", "ModBus Monitor Manager stopped");
}

void ModbusMonitorManager::updateStatusViewModel()
{
    // Update the status view model with current ModBus status
    ModbusMonitorStatus currentStatus = modbusService.getModbusStatus();
    
    // TODO: Add ModBus status to StatusViewModel once the enum is added to definitions.h
    // For now, we'll track it internally and provide access through getters
}

void ModbusMonitorManager::onServiceStatusChange(ServiceStatus status)
{
    const char* statusStr = "Unknown";
    switch (status) {
        case SERVICE_STOPPED: statusStr = "Stopped"; break;
        case SERVICE_STARTING: statusStr = "Starting"; break;
        case SERVICE_CONNECTING: statusStr = "Connecting"; break;
        case SERVICE_CONNECTED: statusStr = "Connected"; break;
        case SERVICE_ERROR: statusStr = "Error"; break;
        case SERVICE_NOT_CONNECTED: statusStr = "Not Connected"; break;
    }
    
    LOG_DEBUG("ModbusMonitorManager", "Service status changed to: %s", statusStr);
}

void ModbusMonitorManager::onModbusStatusChange()
{
    ModbusMonitorStatus currentStatus = modbusService.getModbusStatus();
    
    if (currentStatus != lastModbusStatus) {
        lastModbusStatus = currentStatus;
        
        // Call status change callback if set
        if (statusChangeCallback) {
            statusChangeCallback(currentStatus);
        }
        
        const char* statusStr = "Unknown";
        switch (currentStatus) {
            case MODBUS_INACTIVE: statusStr = "Inactive"; break;
            case MODBUS_ACTIVE: statusStr = "Active"; break;
            case MODBUS_VALID: statusStr = "Valid"; break;
            case MODBUS_INVALID: statusStr = "Invalid"; break;
        }
        
        LOG_DEBUG("ModbusMonitorManager", "ModBus status changed to: %s", statusStr);
    }
}

// Configuration management
void ModbusMonitorManager::loadConfiguration()
{
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, true)) { // Read-only mode
        LOG_WARN("ModbusMonitorManager", "Failed to open NVS namespace for reading");
        return;
    }
    
    ModbusConfig config;
    
    // Load configuration with defaults
    config.baudRate = preferences.getUInt(NVS_KEY_BAUD_RATE, 9600);
    config.slaveId = preferences.getUChar(NVS_KEY_SLAVE_ID, 0x0A);
    config.outputToSerial = preferences.getBool(NVS_KEY_OUTPUT_SERIAL, false);
    config.outputToFile = preferences.getBool(NVS_KEY_OUTPUT_FILE, false);
    config.outputToMQTT = preferences.getBool(NVS_KEY_OUTPUT_MQTT, false);
    
    preferences.end();
    
    // Apply configuration to service
    modbusService.setModbusConfig(config);
    
    LOG_INFO("ModbusMonitorManager", "Config loaded - Baud: %d, Slave ID: 0x%02X", 
        config.baudRate, config.slaveId);
}

void ModbusMonitorManager::saveConfiguration()
{
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) { // Read-write mode
        LOG_ERROR("ModbusMonitorManager", "Failed to open NVS namespace for writing");
        return;
    }
    
    ModbusConfig config = modbusService.getModbusConfig();
    
    // Save configuration
    preferences.putUInt(NVS_KEY_BAUD_RATE, config.baudRate);
    preferences.putUChar(NVS_KEY_SLAVE_ID, config.slaveId);
    preferences.putBool(NVS_KEY_OUTPUT_SERIAL, config.outputToSerial);
    preferences.putBool(NVS_KEY_OUTPUT_FILE, config.outputToFile);
    preferences.putBool(NVS_KEY_OUTPUT_MQTT, config.outputToMQTT);
    
    preferences.end();
    
    LOG_INFO("ModbusMonitorManager", "Configuration saved to NVS");
}

// Public interface methods
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
    saveConfiguration();
    LOG_INFO("ModbusMonitorManager", "Baud rate set to: %d", baudRate);
}

void ModbusMonitorManager::setSlaveId(uint8_t slaveId)
{
    modbusService.setSlaveId(slaveId);
    saveConfiguration();
    LOG_INFO("ModbusMonitorManager", "Slave ID set to: 0x%02X", slaveId);
}

void ModbusMonitorManager::setOutputFlags(bool serial, bool file, bool mqtt)
{
    modbusService.setOutputFlags(serial, file, mqtt);
    saveConfiguration();
    LOG_INFO("ModbusMonitorManager", "Output flags set - Serial: %s, File: %s, MQTT: %s", 
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

bool ModbusMonitorManager::getLastValidResponse(ModbusResponseData& response) const
{
    return modbusService.getLastValidResponse(response);
}

void ModbusMonitorManager::setStatusChangeCallback(std::function<void(ModbusMonitorStatus)> callback)
{
    statusChangeCallback = callback;
}
