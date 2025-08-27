#pragma once
#ifndef __TAGOIOSERVICE_H__
#define __TAGOIOSERVICE_H__

#include "baseService.h"

#include <PicoMQTT.h>
#include <ArduinoJson.h>

#include "credentials.h"

class TagoIOService : public BaseService
{
public:
    TagoIOService();
    virtual ~TagoIOService();

    // BaseService interface
    void begin() override;
    void loop() override;
    void stop() override;
    void start() override;

    // Data publishing interface
    void publishSensorData(const char* variable, float value, const char* unit = nullptr);
    void publishDeviceStatus(const char* status);
    void publishBatchData(JsonDocument& dataArray);

private:
    // MQTT client management
    void initializeMQTTClient();
    void connectMQTT();
    void disconnectMQTT();
    void setupSubscriptions();

    // MQTT event handlers
    void onMQTTConnected();
    void onMQTTDisconnected();

    // Data publishing helpers
    void buildDataPayload(JsonDocument& doc, const char* variable, float value, const char* unit = nullptr);
    void publishToTago(const char* payload);
    void processDataQueue();

    // Utility functions
    void processKeepAlive();

    // Member variables
    PicoMQTT::Client* mqttClient;
    unsigned long lastKeepAlive;
    unsigned long lastDataSend;
    bool initialized;

    // Configuration constants
    static const unsigned long CONNECTION_TIMEOUT_MS = 30000;  // 30 seconds
    static const unsigned long KEEPALIVE_INTERVAL_MS = 60000; // 60 seconds
    static const unsigned long DATA_SEND_INTERVAL_MS = 10000; // 10 seconds
};

#endif // __TAGOIOSERVICE_H__
