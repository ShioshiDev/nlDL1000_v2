#include "loggingManager.h"
#include "networkingManager.h"
#include "esp_eth_driver.h"

// Logging tag
static const char* TAG = "NetworkingManager";

// Static declarations
NetworkingManager *NetworkingManager::instance = nullptr;

// External hardware definitions (to be moved from legacy)
W5500Driver ethernetDriver1(BOARD_PIN_ETHERNET_1_CS, BOARD_PIN_ETHERNET_1_INT, BOARD_PIN_ETHERNET_RESET);
EthernetClass Ethernet1;
const char *ethHostname1 = "nlDL1000-eth1";

IPAddress routerIP;
EthernetUDP udpClient;

BlockNot tmrKeepAlive = BlockNot(KEEP_ALIVE_INTERVAL);

NetworkingManager::NetworkingManager(StatusViewModel &statusVM)
    : statusViewModel(statusVM), currentState(NETWORK_STOPPED), connectStartTime(0), lastStateChange(0), retryCount(0), callback(nullptr)
{
    instance = this;
}

void NetworkingManager::begin()
{
    LOG_INFO(TAG, "Initializing...");
    setState(NETWORK_STARTED);
    initEthernet();
}

void NetworkingManager::initEthernet()
{
    LOG_INFO(TAG, "Initializing Ethernet...");

    // Initialize SPI for Ethernet
    SPI.begin(BOARD_PIN_SCK, BOARD_PIN_MISO, BOARD_PIN_MOSI);

    // Set up event handling
    Network.onEvent(onNetworkEvent);

    // Initialize Ethernet driver
    Ethernet1.init(ethernetDriver1);
    Ethernet1.setHostname(ethHostname1);

    // Begin Ethernet connection
    Ethernet1.begin(CONNECT_TIMEOUT_MS);
}

void NetworkingManager::restartEthernet()
{
    LOG_INFO(TAG, "Restarting Ethernet...");

    // Clean shutdown
    Ethernet1.end();
    ethernetDriver1.end();

    // Wait before restart
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Reinitialize
    Ethernet1.init(ethernetDriver1);
    Ethernet1.setHostname(ethHostname1);

    // Begin Ethernet connection
    Ethernet1.begin(CONNECT_TIMEOUT_MS);
}

void NetworkingManager::loop()
{
    unsigned long now = millis();

    switch (currentState)
    {
    case NETWORK_STOPPED:
        // Do nothing, waiting for begin() to be called
        break;

    case NETWORK_STARTED:
    case NETWORK_CONNECTED:
        // Waiting for IP address or timeout
        // Safety check: Don't timeout if we already have IP
        if (currentState < NETWORK_CONNECTED_IP && now - connectStartTime > CONNECT_TIMEOUT_MS)
        {
            LOG_WARN(TAG, "Timeout triggered in state %d after %lu ms", currentState, now - connectStartTime);
            
            retryCount++;
            if (retryCount >= MAX_RETRY_COUNT)
            {
                LOG_WARN(TAG, "IP timeout after %d retries, full restart...", retryCount);
                retryCount = 0;  // Reset counter
                setState(NETWORK_STOPPED);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                restartEthernet();
                // Only set to STARTED if we're not already connected (avoid race condition with events)
                if (currentState < NETWORK_CONNECTED_IP)
                {
                    LOG_DEBUG(TAG, "Setting state to NETWORK_STARTED (full restart)");
                    setState(NETWORK_STARTED);
                }
                else
                {
                    LOG_DEBUG(TAG, "Already connected during restart, keeping current state %d", currentState);
                }
            }
            else
            {
                LOG_WARN(TAG, "IP timeout, retry %d/%d...", retryCount, MAX_RETRY_COUNT);
                setState(NETWORK_STOPPED);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                LOG_DEBUG(TAG, "Setting state to NETWORK_STARTED (timeout retry)");
                setState(NETWORK_STARTED);
                Ethernet1.begin(CONNECT_TIMEOUT_MS);
            }
        }
        break;

    case NETWORK_CONNECTED_IP:
        // Connected state - monitor for disconnection via events and run keep-alive to the router
        if (tmrKeepAlive.TRIGGERED)
			keepAliveRouterUDP(); // Send UDP keep-alive to router

        break;

    case NETWORK_DISCONNECTED:
        // Cable disconnected - wait for cable reconnection event, don't auto-retry
        // The ARDUINO_EVENT_ETH_CONNECTED event will trigger reconnection when cable is plugged back
        break;

    case NETWORK_LOST_IP:
        // Lost IP but cable still connected - attempt reconnection
        LOG_WARN(TAG, "Lost IP, restarting...");
        setState(NETWORK_DISCONNECTED);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        restartEthernet();
        LOG_DEBUG(TAG, "Setting state to NETWORK_STARTED (lost IP)");
        setState(NETWORK_STARTED);
        break;
    }
}

NetworkStatus NetworkingManager::getState() const
{
    return currentState;
}

bool NetworkingManager::isConnected() const
{
    return currentState >= NETWORK_CONNECTED;
}

bool NetworkingManager::hasIP() const
{
    return currentState >= NETWORK_CONNECTED_IP;
}

void NetworkingManager::setCallback(std::function<void(NetworkStatus)> cb)
{
    callback = cb;
}

void NetworkingManager::restart()
{
    LOG_INFO(TAG, "Manual restart requested");
    restartEthernet();
    LOG_DEBUG(TAG, "Setting state to NETWORK_STARTED (manual restart)");
    setState(NETWORK_STARTED);
}

void NetworkingManager::setState(NetworkStatus newState)
{
    if (currentState != newState)
    {
        LOG_DEBUG(TAG, "State change: %d -> %d", currentState, newState);
        
        NetworkStatus oldState = currentState;
        currentState = newState;
        lastStateChange = millis();

        // Update the status view model
        statusViewModel.setNetworkStatus(newState);

        // Notify callback if set
        if (callback)
        {
            callback(newState);
        }

        // Log state changes
        switch (newState)
        {
        case NETWORK_STOPPED:
            LOG_DEBUG(TAG, "Network status: STOPPED");
            break;
        case NETWORK_STARTED:
            LOG_DEBUG(TAG, "Network status: STARTED");
            // Reset connect start time when entering STARTED state
            connectStartTime = millis();
            break;
        case NETWORK_DISCONNECTED:
            LOG_DEBUG(TAG, "Network status: DISCONNECTED");
            break;
        case NETWORK_LOST_IP:
            LOG_DEBUG(TAG, "Network status: LOST_IP");
            break;
        case NETWORK_CONNECTED:
            LOG_DEBUG(TAG, "Network status: CONNECTED");
            break;
        case NETWORK_CONNECTED_IP:
            LOG_DEBUG(TAG, "Network status: CONNECTED_IP");
            break;
        }
    }
}

void NetworkingManager::onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    if (!instance)
        return;

    switch (event)
    {
    case ARDUINO_EVENT_ETH_START:
        LOG_INFO(TAG, "Ethernet started");
        instance->setState(NETWORK_STARTED);
        break;

    case ARDUINO_EVENT_ETH_CONNECTED:
        LOG_INFO(TAG, "Ethernet cable connected");
        // Reset retry count when cable is reconnected
        instance->retryCount = 0;
        instance->setState(NETWORK_CONNECTED);
        instance->connectStartTime = millis();
        break;

    case ARDUINO_EVENT_ETH_GOT_IP:
        LOG_INFO(TAG, "Got IP address: %s", Ethernet1.localIP().toString().c_str());
        instance->retryCount = 0;  // Reset retry counter on successful IP acquisition
        instance->connectStartTime = millis(); // Reset connect time to prevent false timeouts
        routerIP = Ethernet1.gatewayIP(); // Store router IP for keep-alive
        instance->setState(NETWORK_CONNECTED_IP);
        break;

    case ARDUINO_EVENT_ETH_LOST_IP:
        LOG_WARN(TAG, "Lost IP address");
        instance->setState(NETWORK_LOST_IP);
        break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
        LOG_WARN(TAG, "Ethernet cable disconnected");
        instance->setState(NETWORK_DISCONNECTED);
        break;

    case ARDUINO_EVENT_ETH_STOP:
        LOG_INFO(TAG, "Ethernet stopped");
        instance->setState(NETWORK_STOPPED);
        break;

    default:
        break;
    }
}

void NetworkingManager::keepAliveRouterUDP()
{
    // Send UDP keepalive message to router
    udpClient.beginPacket(routerIP, 8888);
    udpClient.write((uint8_t *)"keepalive", 9);
    udpClient.endPacket();
}

void NetworkingManager::printEthernetStatus()
{
    LOG_INFO(TAG, "=== Ethernet Status Report ===");
    
    // Basic connection state
    const char* stateNames[] = {
        "STOPPED",         // 0: NETWORK_STOPPED
        "STARTED",         // 1: NETWORK_STARTED
        "DISCONNECTED",    // 2: NETWORK_DISCONNECTED
        "LOST_IP",         // 3: NETWORK_LOST_IP
        "CONNECTED",       // 4: NETWORK_CONNECTED
        "CONNECTED_IP"     // 5: NETWORK_CONNECTED_IP
    };
    LOG_INFO(TAG, "Current State: %s (%d)", 
             currentState < 6 ? stateNames[currentState] : "UNKNOWN", currentState);
    
    // Connection timing information
    unsigned long now = millis();
    if (connectStartTime > 0) {
        LOG_INFO(TAG, "Connect Time: %lu ms ago", now - connectStartTime);
    }
    if (lastStateChange > 0) {
        LOG_INFO(TAG, "Last State Change: %lu ms ago", now - lastStateChange);
    }
    LOG_INFO(TAG, "Retry Count: %d/%d", retryCount, MAX_RETRY_COUNT);
    
    // Network configuration if connected
    if (hasIP()) {
        LOG_INFO(TAG, "=== Network Configuration ===");
        LOG_INFO(TAG, "Local IP: %s", Ethernet1.localIP().toString().c_str());
        LOG_INFO(TAG, "Subnet Mask: %s", Ethernet1.subnetMask().toString().c_str());
        LOG_INFO(TAG, "Gateway IP: %s", Ethernet1.gatewayIP().toString().c_str());
        LOG_INFO(TAG, "DNS Server: %s", Ethernet1.dnsServerIP().toString().c_str());
        
        // MAC Address
        uint8_t mac[6];
        Ethernet1.macAddress(mac);
        LOG_INFO(TAG, "MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", 
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        // Hostname
        LOG_INFO(TAG, "Hostname: %s", ethHostname1);
        
        // Router keep-alive status
        if (routerIP != IPAddress(0, 0, 0, 0)) {
            LOG_INFO(TAG, "Router IP (Keep-alive): %s", routerIP.toString().c_str());
        }
        
        // Link status (basic check)
        LOG_INFO(TAG, "Link Status: %s", Ethernet1.linkStatus() == LinkON ? "UP" : "DOWN");
        
        // Get detailed link information using ESP Ethernet driver
        esp_eth_handle_t eth_handle = Ethernet1.getEthHandle();
        if (eth_handle != nullptr) {
            // Get Auto-negotiation status
            bool autonego = false;
            if (esp_eth_ioctl(eth_handle, ETH_CMD_G_AUTONEGO, &autonego) == ESP_OK) {
                LOG_INFO(TAG, "Auto-Negotiation: %s", autonego ? "Enabled" : "Disabled");
            }
            
            // Get link speed
            eth_speed_t speed;
            if (esp_eth_ioctl(eth_handle, ETH_CMD_G_SPEED, &speed) == ESP_OK) {
                const char* speed_str = (speed == ETH_SPEED_10M) ? "10 Mbps" : 
                                       (speed == ETH_SPEED_100M) ? "100 Mbps" : "Unknown";
                LOG_INFO(TAG, "Link Speed: %s", speed_str);
            }
            
            // Get duplex mode
            eth_duplex_t duplex;
            if (esp_eth_ioctl(eth_handle, ETH_CMD_G_DUPLEX_MODE, &duplex) == ESP_OK) {
                const char* duplex_str = (duplex == ETH_DUPLEX_FULL) ? "Full Duplex" : "Half Duplex";
                LOG_INFO(TAG, "Link Duplex: %s", duplex_str);
            }
        } else {
            LOG_WARN(TAG, "Unable to get Ethernet handle for detailed link info");
        }
        
        // Additional network information
        LOG_INFO(TAG, "DHCP Enabled: %s", "Yes"); // EthernetESP32 uses DHCP by default
        
    } else {
        LOG_INFO(TAG, "=== No IP Address ===");
        LOG_INFO(TAG, "Link Status: %s", Ethernet1.linkStatus() == LinkON ? "UP" : "DOWN");
        if (Ethernet1.linkStatus() == LinkON) {
            LOG_INFO(TAG, "Physical connection detected but no IP assigned");
        } else {
            LOG_INFO(TAG, "No physical connection detected");
        }
    }
    
    // Connection status summary
    LOG_INFO(TAG, "=== Connection Summary ===");
    LOG_INFO(TAG, "Is Connected: %s", isConnected() ? "Yes" : "No");
    LOG_INFO(TAG, "Has IP: %s", hasIP() ? "Yes" : "No");
    
    LOG_INFO(TAG, "=== End Ethernet Status ===");
}

