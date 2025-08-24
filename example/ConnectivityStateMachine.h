#pragma once
#include "NetworkStateMachine.h"
#include "Config.h"
#include <ESP32Ping.h>
#include <functional>

enum class ConnectivityState {
    OFFLINE,
    CHECKING,
    ONLINE
};

class ConnectivityStateMachine {
public:
    ConnectivityStateMachine(NetworkStateMachine& netSM);

    void loop();
    ConnectivityState getState() const;

    // Set callback for state change event
    void setCallback(std::function<void(ConnectivityState)> cb);

private:
    NetworkStateMachine& netSM;
    ConnectivityState state;
    unsigned long lastPingTime;
    int pingRetries;
    std::function<void(ConnectivityState)> callback;

    void setState(ConnectivityState newState);
    void checkPing();
};