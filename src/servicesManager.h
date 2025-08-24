#pragma once
#ifndef __SERVICESMANAGER_H__
#define __SERVICESMANAGER_H__

#include <PicoMQTT.h>
#include <functional>
#include <Update.h>

#include "definitions.h"
#include "credentials.h"
#include "statusViewModel.h"
#include "connectivityManager.h"

// Forward declarations
class StatusViewModel;
class ConnectivityManager;

class ServicesManager
{
public:
    ServicesManager(ConnectivityManager &connectivityMgr, StatusViewModel &statusVM);
    ~ServicesManager();  // Destructor to clean up MQTT client

    void begin();
    void loop();
    void stop();

    // State management
    ServicesStatus getState() const;
    bool isConnected() const;

    // MQTT functionality
    void publish(const char *topic, const char *payload, int qos = 0, bool retain = false);
    void subscribe(const char *topic, std::function<void(const char *, const char *)> callback);

    // Device management functions (extracted from legacy)
    void sendDeviceModel();
    void sendFirmwareVersion();
    void sendConnectionStatus(bool connected);
    void checkOTAVersion();

    // Set callback for state change event
    void setStateChangeCallback(std::function<void(ServicesStatus)> callback);

private:
    ConnectivityManager &connectivityManager;
    StatusViewModel &statusViewModel;
    ServicesStatus currentState;
    PicoMQTT::Client *mqttClient;  // Changed to pointer to defer initialization

    unsigned long lastConnectAttempt;
    unsigned long lastKeepAlive;
    bool initialized;

    std::function<void(ServicesStatus)> stateChangeCallback;

    // State management
    void setState(ServicesStatus newState);
    void updateStatusViewModel();

    // Connection management
    void initializeMQTTClient();
    void connectMQTT();
    void disconnectMQTT();
    void setupSubscriptions();
    void setupWillMessage();

    // Event handlers
    void onMQTTConnected();
    void onMQTTDisconnected();

    // Keep alive and maintenance
    void processKeepAlive();

    // Utility functions
    void buildTopicPath(char *buffer, size_t bufferSize, const char *suffix);
    bool canAttemptConnection();
    bool hasInternetConnection();

    // OTA functionality (extracted from legacy networkManager)
    bool isOTAVersionNewer(const char *version);
    void requestOTAUpdate();
    void handleOTAUpdate(PicoMQTT::IncomingPacket& packets);
    void publishOTAStatus(const char *message);
};

#endif // __SERVICESMANAGER_H__
