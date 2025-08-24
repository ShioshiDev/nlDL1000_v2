#include "NetworkStateMachine.h"

NetworkStateMachine* NetworkStateMachine::instance = nullptr;

NetworkStateMachine::NetworkStateMachine() : state(NetworkState::DISCONNECTED), connectStartTime(0), callback(nullptr) {
    instance = this;
}

void NetworkStateMachine::begin() {
    ETH.begin();
    WiFi.onEvent(onEthEvent);
}

void NetworkStateMachine::loop() {
    if (state == NetworkState::CONNECTING) {
        if (millis() - connectStartTime > NETWORK_CONNECT_TIMEOUT_MS) {
            setState(NetworkState::DISCONNECTED);
        }
    }
}

NetworkState NetworkStateMachine::getState() const {
    return state;
}

void NetworkStateMachine::setCallback(std::function<void(NetworkState)> cb) {
    callback = cb;
}

void NetworkStateMachine::setState(NetworkState newState) {
    if (state != newState) {
        state = newState;
        if (callback) callback(state);
    }
}

void NetworkStateMachine::onEthEvent(WiFiEvent_t event) {
    if (!instance) return;
    switch (event) {
        case ARDUINO_EVENT_ETH_CONNECTED:
            instance->setState(NetworkState::CONNECTING);
            instance->connectStartTime = millis();
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            instance->setState(NetworkState::CONNECTED);
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
        case ARDUINO_EVENT_ETH_STOP:
            instance->setState(NetworkState::DISCONNECTED);
            break;
        default: break;
    }
}