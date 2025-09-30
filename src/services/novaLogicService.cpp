#include "novaLogicService.h"
#include "definitions.h"
#include <esp_task_wdt.h>

// Logging tag
static const char* TAG = "NovaLogicService";

// Device serial number for MQTT Client ID
extern String getSerialNumber();
static String deviceSerialNumber = getSerialNumber();
static const char* MQTT_DEVICE_ID = deviceSerialNumber.c_str();

NovaLogicService::NovaLogicService(StatusViewModel& statusVM)
    : BaseService("NovaLogicService"), statusViewModel(statusVM), 
      mqttClient(nullptr), lastKeepAlive(0), initialized(false), commandCallback(nullptr)
{
}

NovaLogicService::~NovaLogicService()
{
    if (mqttClient)
    {
        delete mqttClient;
        mqttClient = nullptr;
    }
}

void NovaLogicService::begin()
{
    LOG_INFO(serviceName, "Initializing...");
    
    setStatus(SERVICE_STOPPED);
    initialized = true;
    
    LOG_INFO(serviceName, "Initialized");
}

void NovaLogicService::loop()
{
    if (!initialized)
    {
        return;
    }

    // Process MQTT client loop (only if client exists and we're in a connecting/connected state)
    if (mqttClient && (currentStatus == SERVICE_CONNECTING || currentStatus == SERVICE_CONNECTED))
    {
        mqttClient->loop();
    }

    // State machine logic
    switch (currentStatus)
    {
    case SERVICE_STOPPED:
        // Service is stopped, waiting for external start command
        break;

    case SERVICE_STARTING:
        if (canAttemptConnection())
        {
            LOG_INFO(TAG, "Attempting MQTT connection to NovaLogic broker...");
            setStatus(SERVICE_CONNECTING);
            connectMQTT();
        }
        break;

    case SERVICE_CONNECTING:
        // Check for connection timeout
        if (millis() - lastConnectionAttempt > SERVICES_CONNECTION_TIMEOUT_MS)
        {
            LOG_WARN(TAG, "Connection timeout");
            setStatus(SERVICE_ERROR);
        }
        break;

    case SERVICE_CONNECTED:
        processKeepAlive();
        break;

    case SERVICE_ERROR:
    case SERVICE_NOT_CONNECTED:
        if (canAttemptConnection())
        {
            setStatus(SERVICE_CONNECTING);
            connectMQTT();
        }
        break;
    }
}

void NovaLogicService::stop()
{
    LOG_INFO(TAG, "Stopping...");

    if (currentStatus == SERVICE_CONNECTED || currentStatus == SERVICE_CONNECTING)
    {
        disconnectMQTT();
    }

    setStatus(SERVICE_STOPPED);
    // DO NOT RESET initialized = false; - Services should remain initialized even when stopped
}

void NovaLogicService::start()
{
    LOG_INFO(TAG, "Starting...");
    
    if (currentStatus == SERVICE_STOPPED)
    {
        setStatus(SERVICE_STARTING);
    }
}

void NovaLogicService::setCommandCallback(std::function<void(const char*, const char*)> callback)
{
    commandCallback = callback;
    LOG_DEBUG(TAG, "Command callback set");
}

void NovaLogicService::initializeMQTTClient()
{
    if (!mqttClient)
    {
        LOG_INFO(TAG, "Creating MQTT client...");
        mqttClient = new PicoMQTT::Client(MQTT_SERVER_NL_URL, MQTT_SERVER_NL_PORT, 
                                        MQTT_DEVICE_ID, MQTT_SERVER_NL_USERNAME, MQTT_SERVER_NL_PASSWORD);
        
        // Set up MQTT event handlers
        mqttClient->connected_callback = [this]()
        {
            this->onMQTTConnected();
        };

        mqttClient->disconnected_callback = [this]()
        {
            this->onMQTTDisconnected();
        };
    }
}

void NovaLogicService::connectMQTT()
{
    LOG_INFO(TAG, "Attempting MQTT connection to NovaLogic broker...");

    // Create MQTT client if it doesn't exist
    initializeMQTTClient();
    
    // Setup will message before connecting
    setupWillMessage();
    
    updateLastConnectionAttempt();
    mqttClient->begin();
}

void NovaLogicService::disconnectMQTT()
{
    LOG_INFO(TAG, "Disconnecting MQTT...");
    if (mqttClient)
    {
        // First try graceful disconnect
        mqttClient->disconnect();
        
        // Give it a moment to disconnect gracefully
        delay(100);
        
        // Destroy and recreate client to ensure clean state
        delete mqttClient;
        mqttClient = nullptr;
        
        LOG_DEBUG(TAG, "MQTT client destroyed");
    }
}

void NovaLogicService::setupSubscriptions()
{
    if (!mqttClient)
    {
        LOG_WARN(TAG, "MQTT client not initialized for subscriptions");
        return;
    }
    
    char mqttTopic[128];

    // Subscribe to general messages
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "messages");
    mqttClient->subscribe(mqttTopic, [this](const char *topic, const char *payload)
    {
        // Use internal MQTT message parser
        this->parseMQTTMessage(topic, payload);
    });

    // Subscribe to OTA version updates
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "ota/version");
    mqttClient->subscribe(mqttTopic, [this](const char* topic, const char* payload) {
        LOG_INFO(TAG, "OTA version received: %s", payload);
        if (isOTAVersionNewer(payload))
        {
            requestOTAUpdate();
        }
    });

    // Subscribe to OTA MD5 hash
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "ota/md5");
    mqttClient->subscribe(mqttTopic, [this](const char* topic, const char* payload) {
        LOG_INFO(TAG, "OTA MD5 received: %s", payload);
        // Store MD5 for validation if needed
    });

    // Subscribe to OTA update binary
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "ota/update");
    mqttClient->subscribe(mqttTopic, [this](const char* topic, PicoMQTT::IncomingPacket& packets) {
        LOG_INFO(TAG, "OTA update binary received");
        handleOTAUpdate(packets);
    });
}

void NovaLogicService::setupWillMessage()
{
    if (!mqttClient)
    {
        LOG_ERROR(TAG, "MQTT client not initialized for will message");
        return;
    }
    
    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "connected");

    mqttClient->will.topic = String(mqttTopic);
    mqttClient->will.payload = "false";
    mqttClient->will.qos = 1;
    mqttClient->will.retain = true;
}

void NovaLogicService::onMQTTConnected()
{
    LOG_INFO(TAG, "MQTT connected to NovaLogic broker!");

    setStatus(SERVICE_CONNECTED);

    // Set up subscriptions
    setupSubscriptions();

    // Send connection status and device info
    sendConnectionStatus(true);
    sendFirmwareVersion();
    sendDeviceModel();
    checkOTAVersion();

    lastKeepAlive = millis();
    
    // Notify logging manager about MQTT connection
    if (globalLoggingManager)
    {
        globalLoggingManager->onMQTTConnected();
    }
}

void NovaLogicService::onMQTTDisconnected()
{
    LOG_WARN(TAG, "MQTT disconnected from NovaLogic broker!");
    setStatus(SERVICE_ERROR);
    
    // Notify logging manager about MQTT disconnection
    if (globalLoggingManager)
    {
        globalLoggingManager->onMQTTDisconnected();
    }
}

void NovaLogicService::processKeepAlive()
{
    if (currentStatus != SERVICE_CONNECTED)
        return;

    unsigned long now = millis();
    if (now - lastKeepAlive >= SERVICES_KEEPALIVE_INTERVAL_MS)
    {
        sendConnectionStatus(true);
        lastKeepAlive = now;
    }
}

void NovaLogicService::buildTopicPath(char* buffer, size_t bufferSize, const char* suffix)
{
    snprintf(buffer, bufferSize, "devices/%s/%s", MQTT_DEVICE_ID, suffix);
}

void NovaLogicService::checkOTAVersion()
{
    if (currentStatus != SERVICE_CONNECTED)
        return;

    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "messages");
    mqttClient->publish(mqttTopic, MQTT_DVC_CMD_VERSION);

    LOG_DEBUG(TAG, "OTA version check requested");
}

void NovaLogicService::sendFirmwareVersion()
{
    if (currentStatus != SERVICE_CONNECTED)
        return;

    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "version");
    mqttClient->publish(mqttTopic, FIRMWARE_VERSION, 1);

    LOG_DEBUG(TAG, "Firmware version sent");
}

void NovaLogicService::sendDeviceModel()
{
    if (currentStatus != SERVICE_CONNECTED)
        return;

    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "model");
    mqttClient->publish(mqttTopic, DEVICE_MODEL, 1);

    LOG_DEBUG(TAG, "Device model sent");
}

void NovaLogicService::sendConnectionStatus(bool connected)
{
    if (currentStatus != SERVICE_CONNECTED && connected)
        return; // Can't send if not connected

    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "connected");
    mqttClient->publish(mqttTopic, connected ? "true" : "false", 1, true);

    LOG_DEBUG(TAG, "Connection status sent: %s", connected ? "true" : "false");
}

bool NovaLogicService::isOTAVersionNewer(const char* version)
{
    bool isNewer = false;

    String currentVersion = FIRMWARE_VERSION; // In format X.YY.ZZ
    String newVersion = String(version);      // In format X.YY.ZZ

    LOG_INFO(TAG, "Current Firmware Version: %s", currentVersion.c_str());
    LOG_INFO(TAG, "Latest OTA version: %s", newVersion.c_str());

    LOG_DEBUG(TAG, "Comparing versions...");
    int currentMajor = currentVersion.substring(0, currentVersion.indexOf('.')).toInt();
    int currentMinor = currentVersion.substring(currentVersion.indexOf('.') + 1, currentVersion.lastIndexOf('.')).toInt();
    int currentPatch = currentVersion.substring(currentVersion.lastIndexOf('.') + 1).toInt();
    int newMajor = newVersion.substring(0, newVersion.indexOf('.')).toInt();
    int newMinor = newVersion.substring(newVersion.indexOf('.') + 1, newVersion.lastIndexOf('.')).toInt();
    int newPatch = newVersion.substring(newVersion.lastIndexOf('.') + 1).toInt();

    if (newMajor > currentMajor)
    {
        isNewer = true;
    }
    else if (newMajor == currentMajor)
    {
        if (newMinor > currentMinor)
        {
            isNewer = true;
        }
        else if (newMinor == currentMinor)
        {
            if (newPatch > currentPatch)
            {
                isNewer = true;
            }
        }
    }

    if (isNewer)
        LOG_INFO(TAG, "OTA version is newer than current version");
    else
        LOG_DEBUG(TAG, "OTA version is not newer than current version");

    return isNewer;
}

void NovaLogicService::requestOTAUpdate()
{
    if (currentStatus != SERVICE_CONNECTED)
        return;

    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "messages");
    mqttClient->publish(mqttTopic, MQTT_DVC_CMD_UPDATE);

    LOG_INFO(TAG, "OTA update requested");
}

void NovaLogicService::handleOTAUpdate(PicoMQTT::IncomingPacket& packets)
{
    LOG_INFO(TAG, "Performing OTA update...");

    size_t payload_size = packets.get_remaining_size();
    LOG_INFO(TAG, "OTA Update Size: %zu", payload_size);

    // Update status to indicate device is updating
    statusViewModel.setDeviceStatus(DEVICE_UPDATING);
    statusViewModel.setOTAActive(true); // Signal other tasks to reduce activity

    if (!Update.begin(packets.available()))
    {
        publishOTAStatus("Error: Not enough space for update");
        statusViewModel.setDeviceStatus(DEVICE_UPDATE_FAILED);
        statusViewModel.setOTAActive(false); // Clear OTA flag
        LOG_ERROR(TAG, "OTA Update failed: Not enough space");
        delay(2500);
        statusViewModel.setDeviceStatus(DEVICE_STARTED);
        return;
    }

    publishOTAStatus("Beginning OTA update, this may take a minute...");
    
    // Write in chunks to avoid blocking for too long
    const size_t CHUNK_SIZE = OTA_CHUNK_SIZE;
    size_t totalWritten = 0;
    size_t remaining = payload_size;
    
    while (remaining > 0 && packets.available())
    {
        size_t toWrite = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        
        // Create a temporary buffer for the chunk
        uint8_t* buffer = new uint8_t[toWrite];
        if (!buffer)
        {
            LOG_ERROR(TAG, "Failed to allocate memory for OTA chunk");
            publishOTAStatus("Error: Memory allocation failed");
            statusViewModel.setDeviceStatus(DEVICE_UPDATE_FAILED);
            statusViewModel.setOTAActive(false); // Clear OTA flag
            delay(2500);
            statusViewModel.setDeviceStatus(DEVICE_STARTED);
            return;
        }
        
        // Read chunk from packets
        size_t bytesRead = packets.read(buffer, toWrite);
        if (bytesRead == 0)
        {
            LOG_ERROR(TAG, "No more data available from packets");
            delete[] buffer;
            break;
        }
        
        // Write chunk to update
        size_t bytesWritten = Update.write(buffer, bytesRead);
        delete[] buffer;
        
        if (bytesWritten != bytesRead)
        {
            LOG_ERROR(TAG, "Write failed: %zu/%zu bytes written", bytesWritten, bytesRead);
            publishOTAStatus("Error: Write failed");
            statusViewModel.setDeviceStatus(DEVICE_UPDATE_FAILED);
            statusViewModel.setOTAActive(false); // Clear OTA flag
            delay(2500);
            statusViewModel.setDeviceStatus(DEVICE_STARTED);
            return;
        }
        
        totalWritten += bytesWritten;
        remaining -= bytesWritten;
        
        // Yield to prevent blocking and allow other tasks to run
        yield();
        
        // Small delay every few chunks to allow critical system tasks to run
        if (totalWritten % (8 * 1024) == 0) {
            delay(10); // Delay every 8KB to give system more time
        }
        
        // Log progress every 64KB
        if (totalWritten % (64 * 1024) == 0)
        {
            LOG_DEBUG(TAG, "OTA Progress: %zu/%zu bytes (%.1f%%)", 
                     totalWritten, payload_size, 
                     (float)totalWritten / payload_size * 100.0);
            
            // Publish progress update
            char progressMsg[128];
            snprintf(progressMsg, sizeof(progressMsg), "OTA Progress: %.1f%% (%zu/%zu bytes)", 
                    (float)totalWritten / payload_size * 100.0, totalWritten, payload_size);
            publishOTAStatus(progressMsg);
        }
    }
    
    LOG_INFO(TAG, "Written: %zu/%zu bytes", totalWritten, payload_size);

    if (totalWritten == payload_size)
    {
        LOG_INFO(TAG, "Update size matches payload size: %zu", payload_size);
        publishOTAStatus("OTA update received, preparing to install...");
    }
    else
    {
        LOG_ERROR(TAG, "Update size does not match payload size: written %zu, expected %zu", totalWritten, payload_size);
        publishOTAStatus("OTA update receive failed...");
    }

    if (Update.hasError())
    {
        LOG_ERROR(TAG, "Update error: %d", Update.getError());
        publishOTAStatus("Error: Update failed!");
        statusViewModel.setDeviceStatus(DEVICE_UPDATE_FAILED);
        statusViewModel.setOTAActive(false); // Clear OTA flag
        delay(2500);
        statusViewModel.setDeviceStatus(DEVICE_STARTED);
        return;
    }

    if (Update.end(true))
    {
        if (Update.isFinished())
        {
            LOG_INFO(TAG, "Update successfully completed. Rebooting...");
            publishOTAStatus("Update successfully completed.");
            statusViewModel.setOTAActive(false); // Clear OTA flag before reboot
            statusViewModel.setDeviceStatus(DEVICE_STARTED);
            delay(2500);
            
            LOG_INFO(TAG, "Restarting...");
            ESP.restart();
        }
        else
        {
            LOG_ERROR(TAG, "Update not finished");
            publishOTAStatus("Update not finished.");
            statusViewModel.setDeviceStatus(DEVICE_UPDATE_FAILED);
            statusViewModel.setOTAActive(false); // Clear OTA flag
            delay(2500);
            statusViewModel.setDeviceStatus(DEVICE_STARTED);
        }
    }
    else
    {
        LOG_ERROR(TAG, "Update error!");
        publishOTAStatus("Update error!");
        statusViewModel.setDeviceStatus(DEVICE_UPDATE_FAILED);
        statusViewModel.setOTAActive(false); // Clear OTA flag
        delay(2500);
        statusViewModel.setDeviceStatus(DEVICE_STARTED);
    }
}

void NovaLogicService::publishOTAStatus(const char* message)
{
    if (currentStatus == SERVICE_CONNECTED)
    {
        char mqttTopic[128];
        buildTopicPath(mqttTopic, sizeof(mqttTopic), "ota/status");
        mqttClient->publish(mqttTopic, message);
        LOG_DEBUG(TAG, "OTA Status: %s", message);
    }
    else
    {
        LOG_DEBUG(TAG, "OTA Status (not connected): %s", message);
    }
}

void NovaLogicService::parseMQTTMessage(const char* topic, const char* payload)
{
    LOG_DEBUG(TAG, "MQTT message received on topic: %s with payload: %s", topic, payload);

    // Handle device model request
    if (strcmp(payload, MQTT_SVR_CMD_DEVICE_MODEL) == 0)
    {
        LOG_DEBUG(TAG, "Device model requested, sending response...");
        sendDeviceModel();
        return;
    }

    // Handle firmware version request
    if (strcmp(payload, MQTT_SVR_CMD_FIRMWARE_VERSION) == 0)
    {
        LOG_DEBUG(TAG, "Device firmware version requested, sending response...");
        sendFirmwareVersion();
        return;
    }

    // For any unhandled messages, forward to external command callback if set
    if (commandCallback)
    {
        LOG_DEBUG(TAG, "Forwarding unhandled message to external callback");
        commandCallback(topic, payload);
    }
    else
    {
        LOG_WARN(TAG, "Unhandled MQTT message: %s", payload);
    }
}
