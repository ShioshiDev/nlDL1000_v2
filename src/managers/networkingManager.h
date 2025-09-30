#pragma once
#ifndef __NETWORKINGMANAGER_H__
#define __NETWORKINGMANAGER_H__

#include <EthernetESP32.h>
#include <SPI.h>
#include <BlockNot.h>
#include <functional>

#include "definitions.h"
#include "statusViewModel.h"

// // Forward declarations
// extern W5500Driver ethernetDriver1;
// extern EthernetClass Ethernet1;

class NetworkingManager
{
public:
    NetworkingManager(StatusViewModel &statusVM);

    void begin();
    void loop();

    NetworkStatus getState() const;
    bool isConnected() const;
    bool hasIP() const;

    // Set callback for state change events
    void setCallback(std::function<void(NetworkStatus)> cb);

    // Manual control
    void restart();
    
    // Status and diagnostics
    void printEthernetStatus();
    
    // Network information getters for display
    String getLocalIP() const;
    String getSubnetMask() const;
    String getGatewayIP() const;
    String getDNSServerIP() const;
    String getMACAddress() const;
    bool getLinkStatus() const;
    String getLinkSpeed() const;
    String getDuplexMode() const;
    bool getAutoNegotiation() const;

private:
    StatusViewModel &statusViewModel;
    NetworkStatus currentState;
    unsigned long connectStartTime;
    unsigned long lastStateChange;
    int retryCount;                    // Current retry attempt counter
    std::function<void(NetworkStatus)> callback;

    void setState(NetworkStatus newState);
    void initEthernet();
    void restartEthernet();
    void keepAliveRouterUDP();

    // Event handlers
    static void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);

    static NetworkingManager *instance;

    // Timeouts and intervals
    static const unsigned long CONNECT_TIMEOUT_MS = NETWORKING_CONNECT_TIMEOUT_MS;
    static const unsigned long RETRY_INTERVAL_MS = NETWORKING_RETRY_INTERVAL_MS;
    static const int MAX_RETRY_COUNT = 3;          // Maximum retries before full restart
};

#endif // __NETWORKINGMANAGER_H__
