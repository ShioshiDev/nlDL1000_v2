#pragma once
#ifndef __CONNECTIVITYMANAGER_H__
#define __CONNECTIVITYMANAGER_H__

#include "networkingManager.h"
#include "statusViewModel.h"
#include "definitions.h"
#include <ESPping.h>
#include <functional>

class ConnectivityManager
{
public:
    ConnectivityManager(NetworkingManager &networkingMgr, StatusViewModel &statusVM);

    void begin();
    void loop();

    ConnectivityStatus getState() const;
    bool isOnline() const;

    // Set callback for state change events
    void setCallback(std::function<void(ConnectivityStatus)> cb);

    // Manual control
    void forceCheck();

private:
    NetworkingManager &networkingManager;
    StatusViewModel &statusViewModel;
    ConnectivityStatus currentState;
    unsigned long lastPingTime;
    unsigned long lastStateChange;
    int pingRetries;
    std::function<void(ConnectivityStatus)> callback;

    void setState(ConnectivityStatus newState);
    void checkConnectivity();
    bool performPing();

    // Configuration constants
    static const unsigned long PING_INTERVAL_MS = CONNECTIVITY_PING_INTERVAL_MS;
    static const unsigned long PING_TIMEOUT_MS = CONNECTIVITY_PING_TIMEOUT_MS;
    static const int PING_RETRY_COUNT = CONNECTIVITY_PING_RETRY_COUNT;
    static const char *PING_HOST;
};

#endif // __CONNECTIVITYMANAGER_H__
