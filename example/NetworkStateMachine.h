#pragma once
#include <ETH.h>
#include "Config.h"
#include <functional>

enum class NetworkState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED
};

class NetworkStateMachine {
public:
    NetworkStateMachine();

    void begin();
    void loop();

    NetworkState getState() const;

    // Set callback for state change event
    void setCallback(std::function<void(NetworkState)> cb);

private:
    NetworkState state;
    unsigned long connectStartTime;
    std::function<void(NetworkState)> callback;

    void setState(NetworkState newState);

    // ETH event handler
    static void onEthEvent(WiFiEvent_t event);
    static NetworkStateMachine* instance;
};