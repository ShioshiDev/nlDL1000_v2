#include "services/modbusMonitorService.h"
#include "managers/loggingManager.h"

static const char *TAG = "ModbusMonitorService";

// Static instance pointer
ModbusMonitorService *ModbusMonitorService::instance = nullptr;

ModbusMonitorService::ModbusMonitorService()
    : BaseService("ModbusMonitor"),
      modbusClient(nullptr),
      modbusSerial(nullptr),
      clientInitialized(false),
      modbusStatus(MODBUS_INACTIVE),
      lastActivityTime(0),
      lastValidFrameTime(0),
      framesReceived(0),
      validFrames(0),
      invalidFrames(0),
      lastRequestTime(0),
      currentPage(4),
      nextToken(1)
{
    // Initialize mutexes
    statusMutex = xSemaphoreCreateMutex();
    configMutex = xSemaphoreCreateMutex();
    dataMutex = xSemaphoreCreateMutex();

    // Set static instance
    instance = this;

    // Initialize DSE data
    memset(&dseData, 0, sizeof(DSEData));

    LOG_INFO(TAG, "ModbusMonitorService initialized");
}

ModbusMonitorService::~ModbusMonitorService()
{
    stop();
    deinitializeModbusClient();

    if (statusMutex)
        vSemaphoreDelete(statusMutex);
    if (configMutex)
        vSemaphoreDelete(configMutex);
    if (dataMutex)
        vSemaphoreDelete(dataMutex);

    instance = nullptr;
    LOG_INFO(TAG, "ModbusMonitorService destroyed");
}

void ModbusMonitorService::begin()
{
    LOG_INFO(TAG, "Starting Modbus Monitor Service...");

    if (initializeModbusClient())
    {
        setModbusStatus(MODBUS_INACTIVE);
        setStatus(SERVICE_CONNECTED);
        LOG_INFO(TAG, "Modbus Monitor Service started successfully");
    }
    else
    {
        LOG_ERROR(TAG, "Failed to initialize Modbus client");
        setStatus(SERVICE_ERROR);
    }
}

void ModbusMonitorService::loop()
{
    if (!isConnected() || !clientInitialized)
    {
        return;
    }

    unsigned long currentTime = millis();

    // Update status periodically
    static unsigned long lastStatusUpdate = 0;
    if (currentTime - lastStatusUpdate >= STATUS_UPDATE_INTERVAL_MS)
    {
        updateStatus();
        lastStatusUpdate = currentTime;
    }

    // Send periodic requests to DSE controller
    if (currentTime - lastRequestTime >= REQUEST_INTERVAL_MS)
    {
        requestNextPage();
        lastRequestTime = currentTime;
    }

    // Process any pending Modbus messages - eModbus handles this internally
}

void ModbusMonitorService::stop()
{
    LOG_INFO(TAG, "Stopping Modbus Monitor Service...");
    setStatus(SERVICE_STOPPED);
    deinitializeModbusClient();
    setModbusStatus(MODBUS_INACTIVE);
}

void ModbusMonitorService::start()
{
    begin();
}

bool ModbusMonitorService::initializeModbusClient()
{
    if (clientInitialized)
    {
        deinitializeModbusClient();
    }

    try
    {
        // Initialize the Modbus serial port to start in receive mode
        pinMode(BOARD_PIN_RS485_DE_RE, OUTPUT);
        digitalWrite(BOARD_PIN_RS485_DE_RE, LOW);
        pinMode(BOARD_PIN_RS485_RX_EN, OUTPUT);
        digitalWrite(BOARD_PIN_RS485_RX_EN, LOW);

        // Initialize serial port (Serial1 for Modbus)
        modbusSerial = &Serial1;
        RTUutils::prepareHardwareSerial(Serial1);
        modbusSerial->begin(config.baudRate, SERIAL_8N1, BOARD_PIN_RS485_RX, BOARD_PIN_RS485_TX);
        modbusSerial->flush();
        vTaskDelay(250 / portTICK_PERIOD_MS); // Allow time for serial to stabilize

        // Create Modbus client with RTS pin (-1 means no RTS control)
        modbusClient = new ModbusClientRTU(BOARD_PIN_RS485_DE_RE, 1); // No RTS pin, queue limit 100
        modbusClient->setTimeout(1000);                               // 1 second timeout

        // Set up callbacks
        modbusClient->onDataHandler(&ModbusMonitorService::onModbusData);
        modbusClient->onErrorHandler(&ModbusMonitorService::onModbusError);

        // Start the client with serial interface - use HardwareSerial overload
        modbusClient->begin(static_cast<HardwareSerial &>(*modbusSerial), -1, 0U);

        clientInitialized = true;
        LOG_INFO(TAG, "Modbus client initialized successfully - Baud: %lu, Slave: 0x%02X",
                 config.baudRate, config.slaveId);

        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(TAG, "Exception initializing Modbus client: %s", e.what());
        clientInitialized = false;
        return false;
    }
}

void ModbusMonitorService::deinitializeModbusClient()
{
    if (modbusClient)
    {
        delete modbusClient;
        modbusClient = nullptr;
    }

    if (modbusSerial)
    {
        modbusSerial->end();
        modbusSerial = nullptr;
    }

    clientInitialized = false;
    LOG_INFO(TAG, "Modbus client deinitialized");
}

void ModbusMonitorService::requestNextPage()
{
    if (!clientInitialized || !modbusClient)
    {
        return;
    }

    uint16_t address;
    uint16_t count;

    // Determine address and count for current page
    switch (currentPage)
    {
    case 4:
        address = MODBUS_PAGE4_ADDRESS;
        count = MODBUS_PAGE4_SIZE;
        break;
    case 5:
        address = MODBUS_PAGE5_ADDRESS + 10; // Start at offset 10
        count = MODBUS_PAGE5_SIZE;
        break;
    case 6:
        address = MODBUS_PAGE6_ADDRESS;
        count = MODBUS_PAGE6_SIZE;
        break;
    case 7:
        address = MODBUS_PAGE7_ADDRESS + 6; // Start at offset 6
        count = MODBUS_PAGE7_SIZE;
        break;
    default:
        currentPage = 4; // Reset to page 4
        return;
    }

    // Send read holding registers request
    Error err = modbusClient->addRequest(nextToken++, config.slaveId, READ_HOLD_REGISTER, address, count);

    if (err == SUCCESS)
    {
        LOG_DEBUG(TAG, "Requesting Page %d - Address: %d, Count: %d, Token: %lu",
                  currentPage, address, count, nextToken - 1);

        // Move to next page for next request
        currentPage++;
        if (currentPage > 7)
        {
            currentPage = 4;
        }
    }
    else
    {
        LOG_ERROR(TAG, "Failed to add Modbus request for page %d, Error: %d", currentPage, err);
    }
}

void ModbusMonitorService::processPageResponse(ModbusMessage response, uint8_t pageNum)
{
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        uint16_t *dataPtr = nullptr;
        bool *validPtr = nullptr;

        switch (pageNum)
        {
        case 4:
            dataPtr = (uint16_t *)&dseData.page4;
            validPtr = &dseData.page4Valid;
            break;
        case 5:
            dataPtr = (uint16_t *)&dseData.page5;
            validPtr = &dseData.page5Valid;
            break;
        case 6:
            dataPtr = (uint16_t *)&dseData.page6;
            validPtr = &dseData.page6Valid;
            break;
        case 7:
            dataPtr = (uint16_t *)&dseData.page7;
            validPtr = &dseData.page7Valid;
            break;
        }

        if (dataPtr && validPtr)
        {
            // Copy response data to appropriate structure
            uint16_t dataLength = response.size() - 3; // Subtract slave, function, byte count
            for (uint16_t i = 0; i < dataLength / 2; i++)
            {
                dataPtr[i] = response.get(3 + i * 2) << 8 | response.get(3 + i * 2 + 1);
            }

            *validPtr = true;
            dseData.lastUpdateTime = millis();

            LOG_DEBUG(TAG, "Page %d data updated - %d registers", pageNum, dataLength / 2);
        }

        xSemaphoreGive(dataMutex);
    }
}

void ModbusMonitorService::updateStatus()
{
    unsigned long currentTime = millis();

    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        ModbusMonitorStatus newStatus = MODBUS_INACTIVE;

        // Check if we have recent activity
        if (currentTime - lastActivityTime < ACTIVITY_TIMEOUT_MS)
        {
            // Check if we have valid data
            if (dseData.page4Valid || dseData.page5Valid || dseData.page6Valid || dseData.page7Valid)
            {
                newStatus = MODBUS_VALID;
            }
            else
            {
                newStatus = MODBUS_ACTIVE;
            }
        }
        else
        {
            // No recent activity - check if data is stale
            if (currentTime - dseData.lastUpdateTime > ACTIVITY_TIMEOUT_MS * 2)
            {
                // Mark all data as invalid if too old
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE)
                {
                    dseData.page4Valid = false;
                    dseData.page5Valid = false;
                    dseData.page6Valid = false;
                    dseData.page7Valid = false;
                    xSemaphoreGive(dataMutex);
                }
            }
            newStatus = MODBUS_INACTIVE;
        }

        if (modbusStatus != newStatus)
        {
            modbusStatus = newStatus;
            LOG_DEBUG(TAG, "Status changed to: %d", newStatus);
        }

        xSemaphoreGive(statusMutex);
    }
}

void ModbusMonitorService::setModbusStatus(ModbusMonitorStatus status)
{
    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        modbusStatus = status;
        xSemaphoreGive(statusMutex);
    }
}

ModbusMonitorStatus ModbusMonitorService::getModbusStatus() const
{
    ModbusMonitorStatus status = MODBUS_INACTIVE;
    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        status = modbusStatus;
        xSemaphoreGive(statusMutex);
    }
    return status;
}

void ModbusMonitorService::setModbusConfig(const ModbusConfig &newConfig)
{
    if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        config = newConfig;
        xSemaphoreGive(configMutex);

        // Reinitialize if connected
        if (isConnected())
        {
            deinitializeModbusClient();
            initializeModbusClient();
        }
    }
}

ModbusConfig ModbusMonitorService::getModbusConfig() const
{
    ModbusConfig cfg;
    if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        cfg = config;
        xSemaphoreGive(configMutex);
    }
    return cfg;
}

void ModbusMonitorService::setBaudRate(uint32_t baudRate)
{
    ModbusConfig cfg = getModbusConfig();
    cfg.baudRate = baudRate;
    setModbusConfig(cfg);
}

void ModbusMonitorService::setSlaveId(uint8_t slaveId)
{
    ModbusConfig cfg = getModbusConfig();
    cfg.slaveId = slaveId;
    setModbusConfig(cfg);
}

void ModbusMonitorService::setOutputFlags(bool serial, bool file, bool mqtt)
{
    ModbusConfig cfg = getModbusConfig();
    cfg.outputToSerial = serial;
    cfg.outputToFile = file;
    cfg.outputToMQTT = mqtt;
    setModbusConfig(cfg);
}

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

bool ModbusMonitorService::getDSEData(DSEData &data) const
{
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        data = dseData;
        xSemaphoreGive(dataMutex);
        return true;
    }
    return false;
}

float ModbusMonitorService::getGeneratorTotalWatts() const
{
    DSEData data;
    if (getDSEData(data) && data.page6Valid)
    {
        // Convert from signed 32-bit integer to watts
        return (float)data.page6.generatorTotalWatts;
    }
    return 0.0f;
}

float ModbusMonitorService::getGeneratorL1NVoltage() const
{
    DSEData data;
    if (getDSEData(data) && data.page4Valid)
    {
        // Convert from 32-bit integer with 0.1V scale
        return (float)data.page4.generatorL1NVoltage / 10.0f;
    }
    return 0.0f;
}

void ModbusMonitorService::handleModbusData(ModbusMessage response, uint32_t token)
{
    framesReceived++;
    validFrames++;
    lastActivityTime = millis();

    // LOG_DEBUG(TAG, "Received Modbus response - Token: %lu, Size: %d", token, response.size());
    LOG_DEBUG(TAG, "Response: serverID=%d, FC=%d, Token=%08X, length=%d:\n", response.getServerID(), response.getFunctionCode(), token, response.size());
    for (auto &byte : response)
    {
        LOG_DEBUG(TAG, "%02X ", byte);
    }

    if (response.size() >= 3) // Minimum: slave + function + byte count
    {
        uint8_t slaveId = response.get(0);
        uint8_t functionCode = response.get(1);
        uint8_t byteCount = response.get(2);

        if (slaveId == config.slaveId && functionCode == READ_HOLD_REGISTER)
        {
            // Determine which page this response is for based on token or data size
            uint8_t pageNum = 4; // Default

            if (byteCount == MODBUS_PAGE4_SIZE * 2)
                pageNum = 4;
            else if (byteCount == MODBUS_PAGE5_SIZE * 2)
                pageNum = 5;
            else if (byteCount == MODBUS_PAGE6_SIZE * 2)
                pageNum = 6;
            else if (byteCount == MODBUS_PAGE7_SIZE * 2)
                pageNum = 7;

            processPageResponse(response, pageNum);
        }
    }
}

void ModbusMonitorService::handleModbusError(Error error, uint32_t token)
{
    framesReceived++;
    invalidFrames++;
    lastActivityTime = millis();
    ModbusError eModbusError(error);
    LOG_WARN(TAG, "Modbus error - Token: %lu, Error Code: %02X, Error: %s", token, error, (const char *)eModbusError);
}

// Static callback wrappers
void ModbusMonitorService::onModbusData(ModbusMessage response, uint32_t token)
{
    if (instance)
    {
        instance->handleModbusData(response, token);
    }
}

void ModbusMonitorService::onModbusError(Error error, uint32_t token)
{
    if (instance)
    {
        instance->handleModbusError(error, token);
    }
}