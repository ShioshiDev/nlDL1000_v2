#include "loggingManager.h"
#include "connectivityManager.h"

// Logging tag
static const char* TAG = "ConnectivityManager";

// Initialize static constants
const char *ConnectivityManager::PING_HOST = CONNECTIVITY_PING_HOST;

ConnectivityManager::ConnectivityManager(NetworkingManager &networkingMgr, StatusViewModel &statusVM)
    : networkingManager(networkingMgr), statusViewModel(statusVM),
      currentState(CONNECTIVITY_OFFLINE), lastPingTime(0), lastStateChange(0),
      pingRetries(0), callback(nullptr)
{
}

void ConnectivityManager::begin()
{
    LOG_INFO(TAG, "Initializing...");
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
        // If we're not verified online yet, check connectivity
        // Check immediately if we haven't pinged yet (lastPingTime == 0) or after the interval
        if (lastPingTime == 0 || (now - lastPingTime >= PING_INTERVAL_MS))
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
    LOG_INFO(TAG, "Force checking connectivity...");
    lastPingTime = 0; // Reset timer to trigger immediate check;
}

void ConnectivityManager::setState(ConnectivityStatus newState)
{
    if (currentState != newState)
    {
        LOG_DEBUG(TAG, "State change: %d -> %d", currentState, newState);

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
            LOG_DEBUG(TAG, "Internet connectivity: OFFLINE");
            break;
        case CONNECTIVITY_CHECKING:
            LOG_DEBUG(TAG, "Internet connectivity: CHECKING");
            break;
        case CONNECTIVITY_ONLINE:
            LOG_DEBUG(TAG, "Internet connectivity: ONLINE");
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
    LOG_DEBUG(TAG, "Pinging %s...", PING_HOST);

    pingRetries = 0;
    bool success = false;

    // Try multiple times before giving up
    for (pingRetries = 0; pingRetries < PING_RETRY_COUNT; pingRetries++)
    {
        // Perform the ping
        success = Ping.ping(PING_HOST, PING_TIMEOUT_MS / 1000); // ESPping uses seconds

        if (success)
        {
            LOG_DEBUG(TAG, "Ping successful (attempt %d/%d)",
                         pingRetries + 1, PING_RETRY_COUNT);
            break;
        }
        else
        {
            LOG_DEBUG(TAG, "Ping failed (attempt %d/%d)",
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
        LOG_WARN(TAG, "All ping attempts failed to %s", PING_HOST);
    }

    return success;
}
