#pragma once
#include "ConnectivityStateMachine.h"
#include "Config.h"
#include <PicoMQTT.h>
#include <functional>

enum class ServiceState {
    STOPPED,
    STARTING,
    RUNNING,
    ERROR
};

class ServiceStateMachine {
public:
    ServiceStateMachine(ConnectivityStateMachine& connSM);

    void loop();
    ServiceState getState() const;

    // Set callback for state change event
    void setCallback(std::function<void(ServiceState)> cb);

    // For extending: add methods to publish/subscribe

private:
    ConnectivityStateMachine& connSM;
    ServiceState state;
    PicoMQTT::Client mqttClient;
    unsigned long lastConnectAttempt;
    std::function<void(ServiceState)> callback;

    void setState(ServiceState newState);

    void connectMQTT();
    void disconnectMQTT();

    // Event handlers
    void onMQTTConnected();
    void onMQTTDisconnected();
};