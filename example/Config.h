#pragma once

// Network State Machine
constexpr unsigned long NETWORK_CONNECT_TIMEOUT_MS = 5000; // How long to wait for IP after cable plugged

// Connectivity State Machine
constexpr unsigned long PING_INTERVAL_MS = 10000;      // How often to ping (ms)
constexpr unsigned long PING_TIMEOUT_MS = 3000;        // How long to wait for ping reply (ms)
constexpr int          PING_RETRY_COUNT = 2;           // How many ping retries before marking offline

// Service State Machine (MQTT)
constexpr unsigned long MQTT_CONNECT_RETRY_INTERVAL_MS = 15000; // Wait before retrying MQTT connect (ms)
constexpr unsigned long MQTT_KEEPALIVE_INTERVAL_MS = 60000;     // MQTT keepalive interval (ms)
constexpr const char*   MQTT_BROKER = "broker.hivemq.com";
constexpr uint16_t      MQTT_PORT   = 1883;
constexpr const char*   MQTT_CLIENT_ID = "esp32-client";