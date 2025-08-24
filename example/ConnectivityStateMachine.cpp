#include "ConnectivityStateMachine.h"

ConnectivityStateMachine::ConnectivityStateMachine(NetworkStateMachine& netSM)
    : netSM(netSM), state(ConnectivityState::OFFLINE), lastPingTime(0), pingRetries(0), callback(nullptr) {}

void ConnectivityStateMachine::loop() {
    if (netSM.getState() != NetworkState::CONNECTED) {
        setState(ConnectivityState::OFFLINE);
        return;
    }
    unsigned long now = millis();
    if (now - lastPingTime >= PING_INTERVAL_MS) {
        lastPingTime = now;
        setState(ConnectivityState::CHECKING);
        checkPing();
    }
}

void ConnectivityStateMachine::checkPing() {
    bool success = false;
    pingRetries = 0;
    for (; pingRetries < PING_RETRY_COUNT; ++pingRetries) {
        success = Ping.ping("8.8.8.8", PING_TIMEOUT_MS / 1000);
        if (success) break;
    }
    setState(success ? ConnectivityState::ONLINE : ConnectivityState::OFFLINE);
}

ConnectivityState ConnectivityStateMachine::getState() const {
    return state;
}

void ConnectivityStateMachine::setCallback(std::function<void(ConnectivityState)> cb) {
    callback = cb;
}

void ConnectivityStateMachine::setState(ConnectivityState newState) {
    if (state != newState) {
        state = newState;
        if (callback) callback(state);
    }
}