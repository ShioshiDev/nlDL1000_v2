#include "services/modbusMonitorService.h"
#include "managers/loggingManager.h"

// Static instance pointer for callback access
ModbusMonitorService* ModbusMonitorService::instance = nullptr;

ModbusMonitorService::ModbusMonitorService()
    : BaseService("ModbusMonitor")
    , modbusClient(nullptr)
    , modbusSerial(nullptr)
    , clientInitialized(false)
    , modbusStatus(MODBUS_INACTIVE)
    , lastActivityTime(0)
    , lastValidFrameTime(0)
    , framesReceived(0)
    , validFrames(0)
    , invalidFrames(0)
    , monitorTaskHandle(nullptr)
    , lastRegisterRequestTime(0)
    , nextToken(1000)
{
    // Initialize mutexes
    statusMutex = xSemaphoreCreateMutex();
    configMutex = xSemaphoreCreateMutex();
    responseMutex = xSemaphoreCreateMutex();
    
    // Initialize response data
    memset(&lastValidResponse, 0, sizeof(ModbusResponseData));
    
    // Set default configuration
    config.baudRate = 9600;
    config.slaveId = 0x0A;
    config.outputToSerial = true;   // Enable serial debug output by default
    config.outputToFile = false;
    config.outputToMQTT = false;

    // Set static instance pointer for callbacks
    instance = this;
}

ModbusMonitorService::~ModbusMonitorService()
{
    stop();
    
    // Clean up mutexes
    if (statusMutex) {
        vSemaphoreDelete(statusMutex);
    }
    if (configMutex) {
        vSemaphoreDelete(configMutex);
    }
    if (responseMutex) {
        vSemaphoreDelete(responseMutex);
    }

    // Clear static instance pointer
    instance = nullptr;
}

void ModbusMonitorService::begin()
{
    LOG_INFO("ModbusMonitor", "Starting ModBus monitor service with eModbus library");
    setStatus(SERVICE_STARTING);
    start();
}

void ModbusMonitorService::start()
{
    if (getStatus() == SERVICE_CONNECTED) {
        LOG_DEBUG("ModbusMonitor", "Service already running");
        return;
    }
    
    setStatus(SERVICE_STARTING);
    
    // Initialize Modbus client
    if (!initializeModbusClient()) {
        LOG_ERROR("ModbusMonitor", "Failed to initialize Modbus client");
        setStatus(SERVICE_ERROR);
        return;
    }
    
    // Create monitoring task
    BaseType_t result = xTaskCreate(
        monitorTask,
        "ModbusMonitor",
        4096,           // Stack size
        this,           // Parameter passed to task
        2,              // Priority
        &monitorTaskHandle
    );
    
    if (result != pdPASS) {
        LOG_ERROR("ModbusMonitor", "Failed to create monitoring task");
        deinitializeModbusClient();
        setStatus(SERVICE_ERROR);
        return;
    }
    
    setStatus(SERVICE_CONNECTED);
    LOG_INFO("ModbusMonitor", "ModBus monitor service started successfully with eModbus");
}

void ModbusMonitorService::loop()
{
    // Main service loop - status updates are handled by the monitoring task
    // This method is called by the service manager periodically
    updateStatus();
}

void ModbusMonitorService::stop()
{
    if (getStatus() == SERVICE_STOPPED) {
        return;
    }
    
    LOG_INFO("ModbusMonitor", "Stopping ModBus monitor service");
    setStatus(SERVICE_STOPPING);
    
    // Stop monitoring task
    if (monitorTaskHandle) {
        vTaskDelete(monitorTaskHandle);
        monitorTaskHandle = nullptr;
    }
    
    // Deinitialize Modbus client
    deinitializeModbusClient();
    
    // Reset status
    setModbusStatus(MODBUS_INACTIVE);
    
    setStatus(SERVICE_STOPPED);
    LOG_INFO("ModbusMonitor", "ModBus monitor service stopped");
}

bool ModbusMonitorService::initializeModbusClient()
{
    if (clientInitialized) {
        return true;
    }
    
    LOG_DEBUG("ModbusMonitor", "Initializing eModbus RTU client...");
    
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        try {
            // Create HardwareSerial instance for UART1
            if (modbusSerial) {
                modbusSerial->end();
                delete modbusSerial;
            }
            delay(100);
            
            modbusSerial = new HardwareSerial(2);
            RTUutils::prepareHardwareSerial(*modbusSerial);
            
            LOG_DEBUG("ModbusMonitor", "Configuring UART2 - Baud: %d, RX: %d, TX: %d", 
                     config.baudRate, BOARD_PIN_RS485_RX, BOARD_PIN_RS485_TX);
            
            // Initialize serial port with current configuration
            modbusSerial->begin(config.baudRate, SERIAL_8N1, BOARD_PIN_RS485_RX, BOARD_PIN_RS485_TX);
            
            // Create ModbusRTU client instance with DE/RE pin
            modbusClient = new ModbusClientRTU(BOARD_PIN_RS485_DE_RE);
            
            LOG_DEBUG("ModbusMonitor", "Using DE/RE pin: %d", BOARD_PIN_RS485_DE_RE);
            
            // Set up callback handlers
            modbusClient->onDataHandler(&ModbusMonitorService::onModbusData);
            modbusClient->onErrorHandler(&ModbusMonitorService::onModbusError);
            
            // Set timeout (increased for troubleshooting)
            modbusClient->setTimeout(5000);
            
            LOG_DEBUG("ModbusMonitor", "Modbus client timeout set to 5000ms");
            
            // Start the modbus client
            modbusClient->begin(*modbusSerial);
            
            LOG_DEBUG("ModbusMonitor", "Modbus client begin() called");
            
            clientInitialized = true;
            
            LOG_INFO("ModbusMonitor", "eModbus RTU client initialized successfully");
            
            xSemaphoreGive(configMutex);
            return true;
            
        } catch (const std::exception& e) {
            LOG_ERROR("ModbusMonitor", "Exception during client initialization: %s", e.what());
            xSemaphoreGive(configMutex);
            return false;
        }
    }
    
    LOG_ERROR("ModbusMonitor", "Failed to acquire config mutex for initialization");
    return false;
}

void ModbusMonitorService::deinitializeModbusClient()
{
    if (!clientInitialized) {
        return;
    }
    
    LOG_DEBUG("ModbusMonitor", "Deinitializing eModbus client...");
    
    if (modbusClient) {
        modbusClient->end();
        delete modbusClient;
        modbusClient = nullptr;
    }
    
    if (modbusSerial) {
        modbusSerial->end();
        delete modbusSerial;
        modbusSerial = nullptr;
    }
    
    clientInitialized = false;
    
    LOG_DEBUG("ModbusMonitor", "eModbus client deinitialized");
}

void ModbusMonitorService::monitorTask(void* parameter)
{
    ModbusMonitorService* service = static_cast<ModbusMonitorService*>(parameter);
    unsigned long lastStatusUpdate = 0;
    
    LOG_DEBUG("ModbusMonitor", "Monitoring task started with eModbus");
    
    while (true) {
        unsigned long currentTime = millis();
        
        // Send periodic register 6 request every 2500ms
        if (currentTime - service->lastRegisterRequestTime >= REGISTER_REQUEST_INTERVAL_MS) {
            service->sendRegister6Request();
            service->lastRegisterRequestTime = currentTime;
        }
        
        // Update status periodically
        if (currentTime - lastStatusUpdate >= STATUS_UPDATE_INTERVAL_MS) {
            service->updateStatus();
            lastStatusUpdate = currentTime;
        }
        
        // Delay to prevent excessive CPU usage
        vTaskDelay(pdMS_TO_TICKS(100));  // Increased delay since eModbus handles communication
    }
}

void ModbusMonitorService::sendRegister6Request()
{
    if (!clientInitialized || !modbusClient) {
        LOG_DEBUG("ModbusMonitor", "Cannot send request - client not initialized");
        return;
    }
    
    // Generate unique token for this request
    uint32_t token = nextToken++;
    
    LOG_DEBUG("ModbusMonitor", "Sending Modbus request - Slave: 0x%02X, Register: 0x0600, Count: 23, Token: %08X", 
              config.slaveId, token);
    
    // Send Read Holding Registers request (function code 0x03)
    // Register page 6 (0x0600), 23 registers (0x0017)
    Error err = modbusClient->addRequest(token, config.slaveId, READ_HOLD_REGISTER, 0x0600, 23);
    
    if (err == SUCCESS) {
        LOG_DEBUG("ModbusMonitor", "Sent register 6 request to slave 0x%02X, token: %08X", 
                  config.slaveId, token);
    } else {
        LOG_ERROR("ModbusMonitor", "Failed to send register 6 request: error %02X", err);
        invalidFrames++;
    }
}

void ModbusMonitorService::handleModbusData(ModbusMessage response, uint32_t token)
{
    if (xSemaphoreTake(responseMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Update statistics
        framesReceived++;
        validFrames++;
        lastActivityTime = millis();
        lastValidFrameTime = lastActivityTime;
        
        // Store response data
        lastValidResponse.slaveId = response.getServerID();
        lastValidResponse.functionCode = response.getFunctionCode();
        lastValidResponse.timestamp = lastActivityTime;
        lastValidResponse.hasError = false;
        lastValidResponse.errorCode = 0;
        
        // Extract register data (skip server ID, function code, and byte count)
        lastValidResponse.registers.clear();
        
        if (response.size() >= 5) {  // Minimum: serverID + FC + byte_count + 2 bytes data + CRC
            uint16_t byteCount = response[2];
            uint16_t numRegisters = byteCount / 2;
            
            // Extract 16-bit registers from response data (starting at byte 3)
            for (uint16_t i = 0; i < numRegisters && (3 + i * 2 + 1) < response.size(); i++) {
                uint16_t registerValue = (response[3 + i * 2] << 8) | response[3 + i * 2 + 1];
                lastValidResponse.registers.push_back(registerValue);
            }
        }
        
        // Update modbus status to active
        setModbusStatus(MODBUS_ACTIVE);
        
        if (config.outputToSerial) {
            LOG_INFO("ModbusMonitor", "Response received: Server=%d, FC=%d, Token=%08X, Registers=%d", 
                     lastValidResponse.slaveId, 
                     lastValidResponse.functionCode,
                     token,
                     lastValidResponse.registers.size());
        }
        
        xSemaphoreGive(responseMutex);
    }
}

void ModbusMonitorService::handleModbusError(Error error, uint32_t token)
{
    // Update statistics
    framesReceived++;
    invalidFrames++;
    lastActivityTime = millis();
    
    if (xSemaphoreTake(responseMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Store error information
        lastValidResponse.hasError = true;
        lastValidResponse.errorCode = static_cast<uint8_t>(error);
        lastValidResponse.timestamp = lastActivityTime;
        
        xSemaphoreGive(responseMutex);
    }
    
    // Update status based on error type
    if (error == TIMEOUT) {
        setModbusStatus(MODBUS_INACTIVE);
    } else {
        setModbusStatus(MODBUS_INVALID);
    }
    
    LOG_ERROR("ModbusMonitor", "Modbus error: %02X, Token: %08X", error, token);
}

// Static callback wrappers for eModbus
void ModbusMonitorService::onModbusData(ModbusMessage response, uint32_t token)
{
    if (instance) {
        instance->handleModbusData(response, token);
    }
}

void ModbusMonitorService::onModbusError(Error error, uint32_t token)
{
    if (instance) {
        instance->handleModbusError(error, token);
    }
}

// Configuration methods
ModbusMonitorStatus ModbusMonitorService::getModbusStatus() const
{
    return modbusStatus;
}

void ModbusMonitorService::setModbusConfig(const ModbusConfig& newConfig)
{
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        config = newConfig;
        xSemaphoreGive(configMutex);
        LOG_DEBUG("ModbusMonitor", "Configuration updated - Baud: %d, SlaveID: 0x%02X", 
                  config.baudRate, config.slaveId);
    }
}

ModbusConfig ModbusMonitorService::getModbusConfig() const
{
    ModbusConfig result;
    if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        result = config;
        xSemaphoreGive(configMutex);
    }
    return result;
}

void ModbusMonitorService::setBaudRate(uint32_t baudRate)
{
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        config.baudRate = baudRate;
        xSemaphoreGive(configMutex);
        LOG_DEBUG("ModbusMonitor", "Baud rate updated to: %d", baudRate);
    }
}

void ModbusMonitorService::setSlaveId(uint8_t slaveId)
{
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        config.slaveId = slaveId;
        xSemaphoreGive(configMutex);
        LOG_DEBUG("ModbusMonitor", "Slave ID updated to: 0x%02X", slaveId);
    }
}

void ModbusMonitorService::setOutputFlags(bool serial, bool file, bool mqtt)
{
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        config.outputToSerial = serial;
        config.outputToFile = file;
        config.outputToMQTT = mqtt;
        xSemaphoreGive(configMutex);
    }
}

// Statistics methods
unsigned long ModbusMonitorService::getFramesReceived() const
{
    return framesReceived;
}

unsigned long ModbusMonitorService::getValidFrames() const
{
    return validFrames;
}

unsigned long ModbusMonitorService::getInvalidFrames() const
{
    return invalidFrames;
}

unsigned long ModbusMonitorService::getLastActivityTime() const
{
    return lastActivityTime;
}

bool ModbusMonitorService::getLastValidResponse(ModbusResponseData& response) const
{
    bool hasResponse = false;
    
    if (xSemaphoreTake(responseMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (lastValidResponse.timestamp > 0) {
            response = lastValidResponse;
            hasResponse = true;
        }
        xSemaphoreGive(responseMutex);
    }
    
    return hasResponse;
}

void ModbusMonitorService::updateStatus()
{
    unsigned long currentTime = millis();
    
    // Check for timeout
    if (currentTime - lastActivityTime > ACTIVITY_TIMEOUT_MS) {
        setModbusStatus(MODBUS_INACTIVE);
    }
    // If we have recent valid activity, set to active
    else if (currentTime - lastValidFrameTime < ACTIVITY_TIMEOUT_MS) {
        setModbusStatus(MODBUS_ACTIVE);
    }
}

void ModbusMonitorService::setModbusStatus(ModbusMonitorStatus status)
{
    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (modbusStatus != status) {
            modbusStatus = status;
            LOG_DEBUG("ModbusMonitor", "Status changed to: %d", status);
        }
        xSemaphoreGive(statusMutex);
    }
}