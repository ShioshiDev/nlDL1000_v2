#include "connectivityManager.h"

// Initialize static constants
const char *ConnectivityManager::PING_HOST = NETWORK_PING_HOST;

ConnectivityManager::ConnectivityManager(NetworkingManager &networkingMgr, StatusViewModel &statusVM)
    : networkingManager(networkingMgr), statusViewModel(statusVM),
      currentState(CONNECTIVITY_OFFLINE), lastPingTime(0), lastStateChange(0),
      pingRetries(0), callback(nullptr)
{
}

void ConnectivityManager::begin()
{
    DEBUG_PRINTLN("[ConnectivityManager] Initializing...");
    setState(CONNECTIVITY_OFFLINE);
}

void ConnectivityManager::loop()
{
    unsigned long now = millis();

    // Only check connectivity if networking layer has IP connectivity
    if (!networkingManager.hasIP())
    {
        if (currentState != CONNECTIVITY_OFFLINE)
        {
            setState(CONNECTIVITY_OFFLINE);
        }
        return;
    }

    if (currentState != CONNECTIVITY_ONLINE)
    {
        // If we we're not verified online yet check connectivity
        if (now - lastPingTime >= PING_INTERVAL_MS)
        {
            lastPingTime = now;
            checkConnectivity();
        }
    }
}

ConnectivityStatus ConnectivityManager::getState() const
{
    return currentState;
}

bool ConnectivityManager::isOnline() const
{
    return currentState == CONNECTIVITY_ONLINE;
}

void ConnectivityManager::setCallback(std::function<void(ConnectivityStatus)> cb)
{
    callback = cb;
}

void ConnectivityManager::forceCheck()
{
    DEBUG_PRINTLN("[ConnectivityManager] Force checking connectivity...");
    lastPingTime = 0; // Reset timer to trigger immediate check
}

void ConnectivityManager::setState(ConnectivityStatus newState)
{
    if (currentState != newState)
    {
        DEBUG_PRINTF("[ConnectivityManager] State change: %d -> %d\n", currentState, newState);

        ConnectivityStatus oldState = currentState;
        currentState = newState;
        lastStateChange = millis();

        // Update status view model
        statusViewModel.setConnectivityStatus(currentState);

        // Call callback if set
        if (callback)
        {
            callback(currentState);
        }

        // Log state changes
        switch (newState)
        {
        case CONNECTIVITY_OFFLINE:
            DEBUG_PRINTLN("[ConnectivityManager] Internet connectivity: OFFLINE");
            break;
        case CONNECTIVITY_CHECKING:
            DEBUG_PRINTLN("[ConnectivityManager] Internet connectivity: CHECKING");
            break;
        case CONNECTIVITY_ONLINE:
            DEBUG_PRINTLN("[ConnectivityManager] Internet connectivity: ONLINE");
            break;
        }
    }
}

void ConnectivityManager::checkConnectivity()
{
    // Set checking state
    setState(CONNECTIVITY_CHECKING);

    // Perform ping test
    bool isConnected = performPing();

    // Update state based on result
    setState(isConnected ? CONNECTIVITY_ONLINE : CONNECTIVITY_OFFLINE);
}

bool ConnectivityManager::performPing()
{
    DEBUG_PRINTF("[ConnectivityManager] Pinging %s...\n", PING_HOST);

    pingRetries = 0;
    bool success = false;

    // Try multiple times before giving up
    for (pingRetries = 0; pingRetries < PING_RETRY_COUNT; pingRetries++)
    {
        // Perform the ping
        success = Ping.ping(PING_HOST, PING_TIMEOUT_MS / 1000); // ESPping uses seconds

        if (success)
        {
            DEBUG_PRINTF("[ConnectivityManager] Ping successful (attempt %d/%d)\n",
                         pingRetries + 1, PING_RETRY_COUNT);
            break;
        }
        else
        {
            DEBUG_PRINTF("[ConnectivityManager] Ping failed (attempt %d/%d)\n",
                         pingRetries + 1, PING_RETRY_COUNT);

            // Short delay between retries
            if (pingRetries < PING_RETRY_COUNT - 1)
            {
                delay(1000);
            }
        }
    }

    if (!success)
    {
        DEBUG_PRINTF("[ConnectivityManager] All ping attempts failed to %s\n", PING_HOST);
    }

    return success;
}
