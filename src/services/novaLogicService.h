#pragma once
#ifndef __NOVALOGICSERVICE_H__
#define __NOVALOGICSERVICE_H__

#include "baseService.h"

#include <PicoMQTT.h>
#include <Update.h>
#include <functional>

#include "statusViewModel.h"
#include "credentials.h"

class NovaLogicService : public BaseService
{
public:
    NovaLogicService(StatusViewModel& statusVM);
    virtual ~NovaLogicService();

    // BaseService interface
    void begin() override;
    void loop() override;
    void stop() override;
    void start() override;

    // Public interface for external calls
    void checkOTAVersion();
    void sendFirmwareVersion();
    void sendDeviceModel();
    void sendConnectionStatus(bool connected);

    // Callback for external command processing
    void setCommandCallback(std::function<void(const char*, const char*)> callback);

private:
    // MQTT client management
    void initializeMQTTClient();
    void connectMQTT();
    void disconnectMQTT();
    void setupSubscriptions();
    void setupWillMessage();

    // MQTT event handlers
    void onMQTTConnected();
    void onMQTTDisconnected();

    // MQTT message processing
    void parseMQTTMessage(const char* topic, const char* payload);

    // OTA functionality
    bool isOTAVersionNewer(const char* version);
    void requestOTAUpdate();
    void handleOTAUpdate(PicoMQTT::IncomingPacket& packets);
    void publishOTAStatus(const char* message);

    // Utility functions
    void buildTopicPath(char* buffer, size_t bufferSize, const char* suffix);
    void processKeepAlive();

    // Member variables
    StatusViewModel& statusViewModel;
    PicoMQTT::Client* mqttClient;
    unsigned long lastKeepAlive;
    bool initialized;
    std::function<void(const char*, const char*)> commandCallback;
};

#endif // __NOVALOGICSERVICE_H__