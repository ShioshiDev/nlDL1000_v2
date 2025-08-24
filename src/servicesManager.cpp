#include "servicesManager.h"

// Logging tag
static const char* TAG = "[ServicesManager]";

// Device serial number for MQTT Client ID
extern String getSerialNumber();
String deviceSerialNumber = getSerialNumber();
const char *MQTT_DEVICE_ID = deviceSerialNumber.c_str();

ServicesManager::ServicesManager(ConnectivityManager &connectivityMgr, StatusViewModel &statusVM)
    : connectivityManager(connectivityMgr), statusViewModel(statusVM), currentState(SERVICES_STOPPED), mqttClient(nullptr), lastConnectAttempt(0), lastKeepAlive(0), initialized(false), stateChangeCallback(nullptr)
{
    // MQTT client will be created when first needed
}

ServicesManager::~ServicesManager()
{
    if (mqttClient)
    {
        delete mqttClient;
        mqttClient = nullptr;
    }
}

void ServicesManager::begin()
{
    DEBUG_PRINTF("%s Initializing...\n", TAG);

    // Start in STOPPED state - will transition to STARTING only when connectivity is available
    setState(SERVICES_STOPPED);
    initialized = true;

    DEBUG_PRINTF("%s Initialized\n", TAG);
}

void ServicesManager::loop()
{
    if (!initialized)
    {
        return;
    }

    // Process MQTT client loop (only if client exists and we're in a state that should use it)
    if (mqttClient && (currentState == SERVICES_CONNECTING || currentState == SERVICES_CONNECTED))
    {
        mqttClient->loop();
    }

    // State machine logic
    switch (currentState)
    {
    case SERVICES_STOPPED:
        // Only start services when we have internet connectivity
        if (hasInternetConnection())
        {
            DEBUG_PRINTF("%s Internet connectivity available, starting services\n", TAG);
            setState(SERVICES_STARTING);
        }
        break;

    case SERVICES_STARTING:
        if (hasInternetConnection() && canAttemptConnection())
        {
            setState(SERVICES_CONNECTING);
            connectMQTT();
        }
        else if (!hasInternetConnection())
        {
            // Lost connectivity while starting, go back to stopped
            setState(SERVICES_STOPPED);
        }
        break;

    case SERVICES_CONNECTING:
        // Waiting for connection callback
        // Check for timeout
        if (millis() - lastConnectAttempt > SERVICES_CONNECTION_TIMEOUT_MS)
        {
            DEBUG_PRINTF("%s Connection timeout\n", TAG);
            setState(SERVICES_ERROR);
        }
        break;

    case SERVICES_CONNECTED:
        if (!hasInternetConnection())
        {
            DEBUG_PRINTF("%s Internet connectivity lost, disconnecting\n", TAG);
            disconnectMQTT();
            setState(SERVICES_NOT_CONNECTED);
        }
        else
        {
            processKeepAlive();
        }
        break;

    case SERVICES_ERROR:
    case SERVICES_NOT_CONNECTED:
        // If we lost internet connectivity, go back to stopped state
        if (!hasInternetConnection())
        {
            // Ensure MQTT is properly disconnected when losing connectivity
            if (mqttClient)
            {
                disconnectMQTT();
            }
            setState(SERVICES_STOPPED);
        }
        else if (hasInternetConnection() && canAttemptConnection())
        {
            setState(SERVICES_CONNECTING);
            connectMQTT();
        }
        break;
    }
}

void ServicesManager::stop()
{
    DEBUG_PRINTF("%s Stopping...\n", TAG);

    if (currentState == SERVICES_CONNECTED || currentState == SERVICES_CONNECTING)
    {
        disconnectMQTT();
    }

    setState(SERVICES_STOPPED);
    initialized = false;
}

ServicesStatus ServicesManager::getState() const
{
    return currentState;
}

bool ServicesManager::isConnected() const
{
    return currentState == SERVICES_CONNECTED;
}

void ServicesManager::setState(ServicesStatus newState)
{
    if (currentState != newState)
    {
        DEBUG_PRINTF("%s State change: %d -> %d\n", TAG, currentState, newState);

        // Handle cleanup when transitioning to STOPPED state
        if (newState == SERVICES_STOPPED && mqttClient != nullptr)
        {
            DEBUG_PRINTF("%s Cleaning up MQTT client on transition to STOPPED\n", TAG);
            // Force disconnect and cleanup
            if (mqttClient)
            {
                mqttClient->disconnect();
                delay(50);
                delete mqttClient;
                mqttClient = nullptr;
            }
        }

        ServicesStatus oldState = currentState;
        currentState = newState;
        updateStatusViewModel();

        if (stateChangeCallback)
        {
            stateChangeCallback(newState);
        }

        // Log state changes
        switch (newState)
        {
        case SERVICES_STOPPED:
            DEBUG_PRINTF("%s Services status: STOPPED\n", TAG);
            break;
        case SERVICES_STARTING:
            DEBUG_PRINTF("%s Services status: STARTING\n", TAG);
            break;
        case SERVICES_CONNECTING:
            DEBUG_PRINTF("%s Services status: CONNECTING\n", TAG);
            break;
        case SERVICES_CONNECTED:
            DEBUG_PRINTF("%s Services status: CONNECTED\n", TAG);
            break;
        case SERVICES_ERROR:
            DEBUG_PRINTF("%s Services status: ERROR\n", TAG);
            break;
        case SERVICES_NOT_CONNECTED:
            DEBUG_PRINTF("%s Services status: NOT_CONNECTED\n", TAG);
            break;
        }
    }
}

void ServicesManager::updateStatusViewModel()
{
    statusViewModel.setServicesStatus(currentState);
}

void ServicesManager::initializeMQTTClient()
{
    if (!mqttClient)
    {
        DEBUG_PRINTF("%s Creating MQTT client...\n", TAG);
        mqttClient = new PicoMQTT::Client(MQTT_SERVER_NL_URL, MQTT_SERVER_NL_PORT, MQTT_DEVICE_ID, MQTT_SERVER_NL_USERNAME, MQTT_SERVER_NL_PASSWORD);
        
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

void ServicesManager::connectMQTT()
{
    DEBUG_PRINTF("%s Attempting MQTT connection...\n", TAG);

    // Create MQTT client if it doesn't exist
    initializeMQTTClient();
    
    // Setup will message before connecting
    setupWillMessage();
    
    lastConnectAttempt = millis();
    mqttClient->begin();
}

void ServicesManager::disconnectMQTT()
{
    DEBUG_PRINTF("%s Disconnecting MQTT...\n", TAG);
    if (mqttClient)
    {
        // First try graceful disconnect
        mqttClient->disconnect();
        
        // Give it a moment to disconnect gracefully
        delay(100);
        
        // Destroy and recreate client to ensure clean state
        delete mqttClient;
        mqttClient = nullptr;
        
        DEBUG_PRINTF("%s MQTT client destroyed\n", TAG);
    }
}

void ServicesManager::setupSubscriptions()
{
    if (!mqttClient)
    {
        DEBUG_PRINTF("%s MQTT client not initialized for subscriptions\n", TAG);
        return;
    }
    
    char mqttTopic[128];

    // Subscribe to general messages
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "messages");
    mqttClient->subscribe(mqttTopic, [this](const char *topic, const char *payload)
                         {
        // Forward to external parser (maintained from legacy)
        extern void parseMQTTMessage(const char* topic, const char* payload);
        parseMQTTMessage(topic, payload); });

    // Subscribe to OTA version updates
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "ota/version");
    mqttClient->subscribe(mqttTopic, [this](const char* topic, const char* payload) {
        DEBUG_PRINTF("ServicesManager: OTA version received: %s\n", payload);
        // To be implemented
    });

    // Subscribe to OTA MD5 hash
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "ota/md5");
    mqttClient->subscribe(mqttTopic, [this](const char* topic, const char* payload) {
        DEBUG_PRINTF("ServicesManager: OTA MD5 received: %s\n", payload);
        // To be implemented
    });

    // Subscribe to OTA update binary
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "ota/update");
    mqttClient->subscribe(mqttTopic, [this](const char* topic, PicoMQTT::IncomingPacket& packets) {
        DEBUG_PRINTLN("ServicesManager: OTA update binary received");
        // To be implemented
    });
}

void ServicesManager::setupWillMessage()
{
    // MQTT client should already be initialized when this is called
    if (!mqttClient)
    {
        DEBUG_PRINTF("%s Error - MQTT client not initialized for will message\n", TAG);
        return;
    }
    
    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "connected");

    mqttClient->will.topic = String(mqttTopic);
    mqttClient->will.payload = "false";
    mqttClient->will.qos = 1;
    mqttClient->will.retain = true;
}

void ServicesManager::onMQTTConnected()
{
    DEBUG_PRINTF("%s MQTT connected!\n", TAG);

    setState(SERVICES_CONNECTED);

    // Set up subscriptions
    setupSubscriptions();

    // Send connection status
    sendConnectionStatus(true);
    sendFirmwareVersion();
    checkOTAVersion();

    lastKeepAlive = millis();
}

void ServicesManager::onMQTTDisconnected()
{
    DEBUG_PRINTF("%s MQTT disconnected!\n", TAG);
    setState(SERVICES_ERROR);
}

void ServicesManager::processKeepAlive()
{
    if (!isConnected())
        return;

    unsigned long now = millis();
    if (now - lastKeepAlive >= SERVICES_KEEPALIVE_INTERVAL_MS)
    {
        sendConnectionStatus(true);

        lastKeepAlive = now;
    }
}

void ServicesManager::publish(const char *topic, const char *payload, int qos, bool retain)
{
    if (isConnected() && mqttClient)
    {
        mqttClient->publish(topic, payload, qos, retain);
    }
    else
    {
        DEBUG_PRINTF("%s Cannot publish - not connected\n", TAG);
    }
}

void ServicesManager::subscribe(const char *topic, std::function<void(const char *, const char *)> callback)
{
    if (isConnected() && mqttClient)
    {
        mqttClient->subscribe(topic, callback);
    }
    else
    {
        DEBUG_PRINTF("%s Cannot subscribe - not connected\n", TAG);
    }
}

void ServicesManager::sendDeviceModel()
{
    if (!isConnected())
        return;

    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "model");
    publish(mqttTopic, DEVICE_MODEL, 1);

    DEBUG_PRINTF("%s Device model sent\n", TAG);
}

void ServicesManager::sendFirmwareVersion()
{
    if (!isConnected())
        return;

    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "version");
    publish(mqttTopic, FIRMWARE_VERSION, 1);

    DEBUG_PRINTF("%s Firmware version sent\n", TAG);
}

void ServicesManager::sendConnectionStatus(bool connected)
{
    if (!isConnected() && connected)
        return; // Can't send if not connected

    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "connected");
    publish(mqttTopic, connected ? "true" : "false", 1, true);

    DEBUG_PRINTF("%s Connection status sent: %s\n", TAG, connected ? "true" : "false");
}

void ServicesManager::setStateChangeCallback(std::function<void(ServicesStatus)> callback)
{
    stateChangeCallback = callback;
}

bool ServicesManager::canAttemptConnection()
{
    unsigned long now = millis();
    return (now - lastConnectAttempt) >= SERVICES_CONNECT_RETRY_INTERVAL_MS;
}

bool ServicesManager::hasInternetConnection()
{
    return connectivityManager.isOnline();
}

void ServicesManager::buildTopicPath(char *buffer, size_t bufferSize, const char *suffix)
{
    snprintf(buffer, bufferSize, "devices/%s/%s", MQTT_DEVICE_ID, suffix);
}

// OTA functionality (extracted from legacy networkManager) -------------------------

void ServicesManager::checkOTAVersion()
{
    if (!isConnected())
        return;

    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "messages");
    publish(mqttTopic, MQTT_OTA_CMD_VERSION);

    DEBUG_PRINTF("%s OTA version check requested\n", TAG);
}

bool ServicesManager::isOTAVersionNewer(const char *version)
{
    bool isNewer = false;

    String currentVersion = FIRMWARE_VERSION; // In format X.YY.ZZ
    String newVersion = String(version);      // In format X.YY.ZZ

    DEBUG_PRINTF("%s Current Firmware Version: %s\n", TAG, currentVersion.c_str());
    DEBUG_PRINTF("%s Latest OTA version: %s\n", TAG, newVersion.c_str());

    DEBUG_PRINTF("%s Comparing versions...\n", TAG);
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
        DEBUG_PRINTF("%s OTA version is newer than current version\n", TAG);
    else
        DEBUG_PRINTF("%s OTA version is not newer than current version\n", TAG);

    return isNewer;
}

void ServicesManager::requestOTAUpdate()
{
    if (!isConnected())
        return;

    char mqttTopic[128];
    buildTopicPath(mqttTopic, sizeof(mqttTopic), "messages");
    publish(mqttTopic, MQTT_OTA_CMD_UPDATE);

    DEBUG_PRINTF("%s OTA update requested\n", TAG);
}

void ServicesManager::handleOTAUpdate(PicoMQTT::IncomingPacket& packets)
{
    DEBUG_PRINTF("%s Performing OTA update...\n", TAG);

    size_t payload_size = packets.get_remaining_size();
    DEBUG_PRINTF("%s OTA Update Size: %zu\n", TAG, payload_size);

    // Update status to indicate device is updating
    statusViewModel.setDeviceStatus(DEVICE_UPDATING);

    if (!Update.begin(packets.available()))
    {
        publishOTAStatus("Error: Not enough space for update");
        statusViewModel.setDeviceStatus(DEVICE_UPDATE_FAILED);
        DEBUG_PRINTF("%s OTA Update failed: Not enough space\n", TAG);
        delay(2500);
        statusViewModel.setDeviceStatus(DEVICE_STARTED);
        return;
    }

    publishOTAStatus("Beginning OTA update, this may take a minute...");
    size_t written = Update.write(packets);
    if (written == payload_size)
    {
        DEBUG_PRINTF("%s Written: %zu successfully\n", TAG, written);
    }
    else
    {
        DEBUG_PRINTF("%s Written only: %zu/%zu\n", TAG, written, payload_size);
    }

    if (Update.size() == payload_size)
    {
        DEBUG_PRINTF("%s Update size matches payload size: %zu\n", TAG, payload_size);
        publishOTAStatus("OTA update received, preparing to install...");
    }
    else
    {
        DEBUG_PRINTF("%s Update size does not match payload size: %zu\n", TAG, payload_size);
        publishOTAStatus("OTA update receive failed...");
    }

    if (Update.hasError())
    {
        DEBUG_PRINTF("%s Update error: %d\n", TAG, Update.getError());
        publishOTAStatus("Error: Update failed!");
        statusViewModel.setDeviceStatus(DEVICE_UPDATE_FAILED);
        delay(2500);
        statusViewModel.setDeviceStatus(DEVICE_STARTED);
        return;
    }

    if (Update.end(true))
    {
        if (Update.isFinished())
        {
            DEBUG_PRINTF("%s Update successfully completed. Rebooting...\n", TAG);
            publishOTAStatus("Update successfully completed.");
            statusViewModel.setDeviceStatus(DEVICE_STARTED);
            delay(2500);
            
            // Gracefully stop services before restart
            stop();
            delay(1000);
            
            DEBUG_PRINTF("%s Restarting...\n", TAG);
            ESP.restart();
        }
        else
        {
            DEBUG_PRINTF("%s Update not finished\n", TAG);
            publishOTAStatus("Update not finished.");
            statusViewModel.setDeviceStatus(DEVICE_UPDATE_FAILED);
            delay(2500);
            statusViewModel.setDeviceStatus(DEVICE_STARTED);
        }
    }
    else
    {
        DEBUG_PRINTF("%s Update error!\n", TAG);
        publishOTAStatus("Update error!");
        statusViewModel.setDeviceStatus(DEVICE_UPDATE_FAILED);
        delay(2500);
        statusViewModel.setDeviceStatus(DEVICE_STARTED);
    }
}

void ServicesManager::publishOTAStatus(const char *message)
{
    if (isConnected())
    {
        char mqttTopic[128];
        buildTopicPath(mqttTopic, sizeof(mqttTopic), "ota/status");
        publish(mqttTopic, message);
        DEBUG_PRINTF("%s OTA Status: %s\n", TAG, message);
    }
    else
    {
        DEBUG_PRINTF("%s OTA Status (not connected): %s\n", TAG, message);
    }
}
