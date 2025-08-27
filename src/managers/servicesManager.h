#pragma once
#ifndef __SERVICESMANAGER_H__
#define __SERVICESMANAGER_H__

#include <functional>

#include "definitions.h"
#include "statusViewModel.h"
#include "connectivityManager.h"
#include "services/baseService.h"
#include "services/novaLogicService.h"
#include "services/tagoIOService.h"

// Forward declarations
class StatusViewModel;
class ConnectivityManager;

class ServicesManager
{
public:
    ServicesManager(ConnectivityManager &connectivityMgr, StatusViewModel &statusVM);
    ~ServicesManager();

    void begin();
    void loop();
    void stop();

    // State management
    ServicesStatus getState() const;
    bool isConnected() const;
    bool isNovaLogicConnected() const;
    bool isTagoIOConnected() const;

    // Service access for external calls
    NovaLogicService& getNovaLogicService() { return novaLogicService; }
    TagoIOService& getTagoIOService() { return tagoIOService; }

    // Device management functions (delegated to NovaLogic service)
    void sendDeviceModel();
    void sendFirmwareVersion();
    void sendConnectionStatus(bool connected);
    void checkOTAVersion();

    // Data publishing functions (delegated to TagoIO service)
    void publishSensorData(const char* variable, float value, const char* unit = nullptr);
    void publishDeviceStatus(const char* status);

    // Set callback for state change event
    void setStateChangeCallback(std::function<void(ServicesStatus)> callback);

    // Handle immediate connectivity changes (called by connectivity manager)
    void onConnectivityChanged(ConnectivityStatus connectivityStatus);

private:
    // Service orchestration
    void updateOverallStatus();
    void setState(ServicesStatus newState);
    void onServiceStatusChange(ServiceStatus status);
    bool hasInternetConnection();

    // Dependencies
    ConnectivityManager &connectivityManager;
    StatusViewModel &statusViewModel;
    
    // Services
    NovaLogicService novaLogicService;
    TagoIOService tagoIOService;
    
    // State management
    ServicesStatus currentState;
    bool initialized;
    bool lastMQTTConnectedState;  // Track previous MQTT connectivity for logging manager notifications
    std::function<void(ServicesStatus)> stateChangeCallback;

    // Timing
    unsigned long lastStateCheck;

    // Constants
    static const unsigned long STATE_CHECK_INTERVAL_MS = 10000; // Periodic check every 10 seconds (reduced from 5s since we have immediate callbacks)
};

#endif // __SERVICESMANAGER_H__
