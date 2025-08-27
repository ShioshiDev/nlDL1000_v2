#include "tagoIOService.h"

// Logging tag
static const char* TAG = "TagoIOService";

extern String getSerialNumber();
static String deviceSerial = getSerialNumber();
const char *MQTT_DEVICE_ID = deviceSerial.c_str();

TagoIOService::TagoIOService()
    : BaseService("TagoIOService"), mqttClient(nullptr), 
      lastKeepAlive(0), lastDataSend(0), initialized(false)
{
}

TagoIOService::~TagoIOService()
{
    if (mqttClient)
    {
        delete mqttClient;
        mqttClient = nullptr;
    }
}

void TagoIOService::begin()
{
    LOG_INFO(TAG, "Initializing...");
    
    setStatus(SERVICE_STOPPED);
    initialized = true;
    
    LOG_INFO(TAG, "Initialized");
}

void TagoIOService::loop()
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
            LOG_INFO(TAG, "Attempting MQTT connection to TagoIO broker...");
            setStatus(SERVICE_CONNECTING);
            connectMQTT();
        }
        break;

    case SERVICE_CONNECTING:
        // Check for connection timeout
        if (millis() - lastConnectionAttempt > CONNECTION_TIMEOUT_MS)
        {
            LOG_WARN(TAG, "Connection timeout");
            setStatus(SERVICE_ERROR);
        }
        break;

    case SERVICE_CONNECTED:
        processKeepAlive();
        processDataQueue();
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

void TagoIOService::stop()
{
    LOG_INFO(TAG, "Stopping...");

    if (currentStatus == SERVICE_CONNECTED || currentStatus == SERVICE_CONNECTING)
    {
        disconnectMQTT();
    }

    setStatus(SERVICE_STOPPED);
    // DO NOT RESET initialized = false; - Services should remain initialized even when stopped
}

void TagoIOService::start()
{
    LOG_INFO(TAG, "Starting...");
    
    if (currentStatus == SERVICE_STOPPED)
    {
        setStatus(SERVICE_STARTING);
    }
}

void TagoIOService::initializeMQTTClient()
{
    if (!mqttClient)
    {
        LOG_INFO(TAG, "Creating MQTT client...");
        mqttClient = new PicoMQTT::Client(MQTT_SERVER_TAGO_URL, MQTT_SERVER_TAGO_PORT, MQTT_DEVICE_ID, "Token", MQTT_SERVER_TAGO_DEVICE_TOKEN);

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

void TagoIOService::connectMQTT()
{
    LOG_INFO(TAG, "Attempting MQTT connection to TagoIO broker...");

    // Create MQTT client if it doesn't exist
    initializeMQTTClient();
    
    updateLastConnectionAttempt();
    mqttClient->begin();
}

void TagoIOService::disconnectMQTT()
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

void TagoIOService::setupSubscriptions()
{
    if (!mqttClient)
    {
        LOG_WARN(TAG, "MQTT client not initialized for subscriptions");
        return;
    }
    
    // TagoIO primarily receives data, but we can subscribe to downlink messages if needed
    // For now, just log that subscriptions are set up
    LOG_DEBUG(TAG, "TagoIO subscriptions configured");
}

void TagoIOService::onMQTTConnected()
{
    LOG_INFO(TAG, "MQTT connected to TagoIO broker!");

    setStatus(SERVICE_CONNECTED);

    // Set up subscriptions (if any)
    setupSubscriptions();

    // Send initial connection status
    publishDeviceStatus("connected");

    lastKeepAlive = millis();
    lastDataSend = millis();
}

void TagoIOService::onMQTTDisconnected()
{
    LOG_WARN(TAG, "MQTT disconnected from TagoIO broker!");
    setStatus(SERVICE_ERROR);
}

void TagoIOService::processKeepAlive()
{
    if (currentStatus != SERVICE_CONNECTED)
        return;

    unsigned long now = millis();
    if (now - lastKeepAlive >= KEEPALIVE_INTERVAL_MS)
    {
        // Send a heartbeat to TagoIO
        publishDeviceStatus("online");
        lastKeepAlive = now;
    }
}

void TagoIOService::processDataQueue()
{
    // This method can be extended to process queued data
    // For now, it's a placeholder for future batch data processing
    unsigned long now = millis();
    if (now - lastDataSend >= DATA_SEND_INTERVAL_MS)
    {
        // Example: Send periodic system status
        // This could be replaced with actual sensor data
        lastDataSend = now;
    }
}

void TagoIOService::publishSensorData(const char* variable, float value, const char* unit)
{
    if (currentStatus != SERVICE_CONNECTED)
    {
        LOG_WARN(TAG, "Cannot publish sensor data - not connected");
        return;
    }

    JsonDocument doc;
    buildDataPayload(doc, variable, value, unit);
    
    String payload;
    serializeJson(doc, payload);
    
    publishToTago(payload.c_str());
    
    LOG_DEBUG(TAG, "Published sensor data: %s = %.2f %s", 
                variable, value, unit ? unit : "");
}

void TagoIOService::publishDeviceStatus(const char* status)
{
    if (currentStatus != SERVICE_CONNECTED)
    {
        LOG_WARN(TAG, "Cannot publish device status - not connected");
        return;
    }

    JsonDocument doc;
    buildDataPayload(doc, "device_status", 0, status);
    
    String payload;
    serializeJson(doc, payload);
    
    publishToTago(payload.c_str());
    
    LOG_DEBUG(TAG, "Published device status: %s", status);
}

void TagoIOService::publishBatchData(JsonDocument& dataArray)
{
    if (currentStatus != SERVICE_CONNECTED)
    {
        LOG_WARN(TAG, "Cannot publish batch data - not connected");
        return;
    }

    String payload;
    serializeJson(dataArray, payload);
    
    publishToTago(payload.c_str());
    
    LOG_DEBUG(TAG, "Published batch data (%d items)", dataArray.size());
}

void TagoIOService::buildDataPayload(JsonDocument& doc, const char* variable, float value, const char* unit)
{
    doc["variable"] = variable;
    doc["value"] = value;
    doc["timestamp"] = millis(); // This should be replaced with actual timestamp
    
    if (unit)
    {
        doc["unit"] = unit;
    }
    
    // Add device metadata
    doc["metadata"]["device"] = "DL1000";
    doc["metadata"]["source"] = serviceName;
}

void TagoIOService::publishToTago(const char* payload)
{
    if (mqttClient && currentStatus == SERVICE_CONNECTED)
    {
        // TagoIO typically uses a specific topic format
        String topic = String("tago/data/post");
        mqttClient->publish(topic.c_str(), payload, 1);
        
        LOG_DEBUG(TAG, "Data sent to TagoIO: %s", payload);
    }
    else
    {
        LOG_WARN(TAG, "Cannot send to TagoIO - not connected");
    }
}
