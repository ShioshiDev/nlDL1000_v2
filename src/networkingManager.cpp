#include "networkingManager.h"

// Logging tag
static const char* TAG = "[NetworkingManager]";

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
    DEBUG_PRINTF("%s Initializing...\n", TAG);
    setState(NETWORK_STARTED);
    initEthernet();
}

void NetworkingManager::initEthernet()
{
    DEBUG_PRINTF("%s Initializing Ethernet...\n", TAG);

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
    DEBUG_PRINTF("%s Restarting Ethernet...\n", TAG);

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
            DEBUG_PRINTF("%s Timeout triggered in state %d after %lu ms\n", TAG, currentState, now - connectStartTime);
            retryCount++;
            if (retryCount >= MAX_RETRY_COUNT)
            {
                DEBUG_PRINTF("%s IP timeout after %d retries, full restart...\n", TAG, retryCount);
                retryCount = 0;  // Reset counter
                setState(NETWORK_STOPPED);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                restartEthernet();
                DEBUG_PRINTF("%s Setting state to NETWORK_STARTED (full restart)\n", TAG);
                setState(NETWORK_STARTED);
            }
            else
            {
                DEBUG_PRINTF("%s IP timeout, retry %d/%d...\n", TAG, retryCount, MAX_RETRY_COUNT);
                setState(NETWORK_STOPPED);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                DEBUG_PRINTF("%s Setting state to NETWORK_STARTED (timeout retry)\n", TAG);
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
    case NETWORK_LOST_IP:
        DEBUG_PRINTF("%s Lost connection, restarting...\n", TAG);
        setState(NETWORK_DISCONNECTED);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        restartEthernet();
        DEBUG_PRINTF("%s Setting state to NETWORK_STARTED (lost connection)\n", TAG);
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
    DEBUG_PRINTF("%s Manual restart requested\n", TAG);
    restartEthernet();
    DEBUG_PRINTF("%s Setting state to NETWORK_STARTED (manual restart)\n", TAG);
    setState(NETWORK_STARTED);
}

void NetworkingManager::setState(NetworkStatus newState)
{
    if (currentState != newState)
    {
        DEBUG_PRINTF("%s State change: %d -> %d\n", TAG, currentState, newState);
        
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
            DEBUG_PRINTF("%s Network status: STOPPED\n", TAG);
            break;
        case NETWORK_STARTED:
            DEBUG_PRINTF("%s Network status: STARTED\n", TAG);
            break;
        case NETWORK_DISCONNECTED:
            DEBUG_PRINTF("%s Network status: DISCONNECTED\n", TAG);
            break;
        case NETWORK_LOST_IP:
            DEBUG_PRINTF("%s Network status: LOST_IP\n", TAG);
            break;
        case NETWORK_CONNECTED:
            DEBUG_PRINTF("%s Network status: CONNECTED\n", TAG);
            break;
        case NETWORK_CONNECTED_IP:
            DEBUG_PRINTF("%s Network status: CONNECTED_IP\n", TAG);
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
        DEBUG_PRINTF("%s Ethernet started\n", TAG);
        instance->setState(NETWORK_STARTED);
        break;

    case ARDUINO_EVENT_ETH_CONNECTED:
        DEBUG_PRINTF("%s Ethernet cable connected\n", TAG);
        instance->setState(NETWORK_CONNECTED);
        instance->connectStartTime = millis();
        break;

    case ARDUINO_EVENT_ETH_GOT_IP:
        DEBUG_PRINTF("%s Got IP address: %s\n", TAG, Ethernet1.localIP().toString().c_str());
        instance->retryCount = 0;  // Reset retry counter on successful IP acquisition
        routerIP = Ethernet1.gatewayIP(); // Store router IP for keep-alive
        instance->setState(NETWORK_CONNECTED_IP);
        break;

    case ARDUINO_EVENT_ETH_LOST_IP:
        DEBUG_PRINTF("%s Lost IP address\n", TAG);
        instance->setState(NETWORK_LOST_IP);
        break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
        DEBUG_PRINTF("%s Ethernet cable disconnected\n", TAG);
        instance->setState(NETWORK_DISCONNECTED);
        break;

    case ARDUINO_EVENT_ETH_STOP:
        DEBUG_PRINTF("%s Ethernet stopped\n", TAG);
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

