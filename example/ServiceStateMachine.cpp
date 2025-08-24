#include "ServiceStateMachine.h"

ServiceStateMachine::ServiceStateMachine(ConnectivityStateMachine& connSM)
    : connSM(connSM), state(ServiceState::STOPPED), lastConnectAttempt(0),
      mqttClient(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT_ID), callback(nullptr)
{
    mqttClient.onConnected([this]() { this->onMQTTConnected(); });
    mqttClient.onDisconnected([this]() { this->onMQTTDisconnected(); });
}

void ServiceStateMachine::loop() {
    if (connSM.getState() == ConnectivityState::ONLINE) {
        if (state == ServiceState::STOPPED || state == ServiceState::ERROR) {
            unsigned long now = millis();
            if (now - lastConnectAttempt > MQTT_CONNECT_RETRY_INTERVAL_MS) {
                lastConnectAttempt = now;
                setState(ServiceState::STARTING);
                connectMQTT();
            }
        }
    } else {
        if (state == ServiceState::RUNNING || state == ServiceState::STARTING) {
            disconnectMQTT();
            setState(ServiceState::STOPPED);
        }
    }
    mqttClient.loop();
}

void ServiceStateMachine::connectMQTT() {
    mqttClient.connect();
}

void ServiceStateMachine::disconnectMQTT() {
    mqttClient.disconnect();
}

ServiceState ServiceStateMachine::getState() const {
    return state;
}

void ServiceStateMachine::setCallback(std::function<void(ServiceState)> cb) {
    callback = cb;
}

void ServiceStateMachine::setState(ServiceState newState) {
    if (state != newState) {
        state = newState;
        if (callback) callback(state);
    }
}

void ServiceStateMachine::onMQTTConnected() {
    setState(ServiceState::RUNNING);
}

void ServiceStateMachine::onMQTTDisconnected() {
    setState(ServiceState::ERROR);
}