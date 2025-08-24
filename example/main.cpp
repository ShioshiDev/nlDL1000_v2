#include "NetworkStateMachine.h"
#include "ConnectivityStateMachine.h"
#include "ServiceStateMachine.h"
#include <Arduino.h>

// Instantiate state machines
NetworkStateMachine netSM;
ConnectivityStateMachine connSM(netSM);
ServiceStateMachine serviceSM(connSM);

// Status LED pin
constexpr int LED_PIN = 2;

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    // Set up callbacks for state changes
    netSM.setCallback([](NetworkState state) {
        Serial.print("[Network] State: ");
        switch (state) {
            case NetworkState::DISCONNECTED: Serial.println("DISCONNECTED"); break;
            case NetworkState::CONNECTING:   Serial.println("CONNECTING");   break;
            case NetworkState::CONNECTED:    Serial.println("CONNECTED");    break;
        }
    });

    connSM.setCallback([](ConnectivityState state) {
        Serial.print("[Connectivity] State: ");
        switch (state) {
            case ConnectivityState::OFFLINE:  Serial.println("OFFLINE");  break;
            case ConnectivityState::CHECKING: Serial.println("CHECKING"); break;
            case ConnectivityState::ONLINE:   Serial.println("ONLINE");   break;
        }
    });

    serviceSM.setCallback([](ServiceState state) {
        Serial.print("[Service] State: ");
        switch (state) {
            case ServiceState::STOPPED:  Serial.println("STOPPED");  digitalWrite(LED_PIN, LOW); break;
            case ServiceState::STARTING: Serial.println("STARTING"); digitalWrite(LED_PIN, LOW); break;
            case ServiceState::RUNNING:  Serial.println("RUNNING");  digitalWrite(LED_PIN, HIGH); break;
            case ServiceState::ERROR:    Serial.println("ERROR");    digitalWrite(LED_PIN, LOW); break;
        }
    });

    netSM.begin();
}

void loop() {
    netSM.loop();
    connSM.loop();
    serviceSM.loop();

    // Add your MQTT publish/subscribe logic here as needed

    delay(50);
}