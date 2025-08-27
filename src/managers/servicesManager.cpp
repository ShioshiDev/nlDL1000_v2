#include "loggingManager.h"
#include "servicesManager.h"

// Logging tag
static const char* TAG = "ServicesManager";

ServicesManager::ServicesManager(ConnectivityManager &connectivityMgr, StatusViewModel &statusVM)
    : connectivityManager(connectivityMgr), statusViewModel(statusVM), 
      novaLogicService(statusVM), tagoIOService(),
      currentState(SERVICES_STOPPED), initialized(false), lastMQTTConnectedState(false), stateChangeCallback(nullptr),
      lastStateCheck(0)
{
    // Set up service status change callbacks
    novaLogicService.setStatusChangeCallback([this](ServiceStatus status) {
        this->onServiceStatusChange(status);
    });
    
    tagoIOService.setStatusChangeCallback([this](ServiceStatus status) {
        this->onServiceStatusChange(status);
    });
}

ServicesManager::~ServicesManager()
{
    // Services will clean up themselves in their destructors
}

void ServicesManager::begin()
{
    LOG_INFO(TAG, "Initializing services orchestrator...");

    // Initialize individual services
    novaLogicService.begin();
    tagoIOService.begin();

    // Start in STOPPED state - will transition to STARTING only when connectivity is available
    setState(SERVICES_STOPPED);
    initialized = true;

    LOG_INFO(TAG, "Services orchestrator initialized");
}

void ServicesManager::loop()
{
    if (!initialized)
    {
        return;
    }

    // Update individual services
    novaLogicService.loop();
    tagoIOService.loop();

    // Periodic status evaluation
    unsigned long now = millis();
    if (now - lastStateCheck >= STATE_CHECK_INTERVAL_MS)
    {
        updateOverallStatus();
        lastStateCheck = now;
    }

    // State machine logic for service orchestration
    switch (currentState)
    {
    case SERVICES_STOPPED:
        // Only start services when we have internet connectivity
        if (hasInternetConnection())
        {
            LOG_INFO(TAG, "Internet connectivity available, starting services");
            setState(SERVICES_STARTING);
            
            // Start both services
            if (novaLogicService.getStatus() == SERVICE_STOPPED)
            {
                novaLogicService.start();
            }
            if (tagoIOService.getStatus() == SERVICE_STOPPED)
            {
                tagoIOService.start();
            }
        }
        break;

    case SERVICES_STARTING:
        if (!hasInternetConnection())
        {
            // Lost connectivity while starting, stop services
            LOG_WARN(TAG, "Lost connectivity while starting, stopping services");
            novaLogicService.stop();
            tagoIOService.stop();
            setState(SERVICES_STOPPED);
        }
        // updateOverallStatus() will handle transition to CONNECTING/CONNECTED
        break;

    case SERVICES_CONNECTING:
        if (!hasInternetConnection())
        {
            // Lost connectivity while connecting
            LOG_WARN(TAG, "Lost connectivity while connecting, stopping services");
            novaLogicService.stop();
            tagoIOService.stop();
            setState(SERVICES_STOPPED);
        }
        // updateOverallStatus() will handle transition to CONNECTED or ERROR
        break;

    case SERVICES_CONNECTED:
        if (!hasInternetConnection())
        {
            LOG_WARN(TAG, "Internet connectivity lost, stopping services");
            novaLogicService.stop();
            tagoIOService.stop();
            setState(SERVICES_NOT_CONNECTED);
        }
        break;

    case SERVICES_ERROR:
    case SERVICES_NOT_CONNECTED:
        if (!hasInternetConnection())
        {
            // Go back to stopped if no connectivity
            if (currentState != SERVICES_STOPPED)
            {
                novaLogicService.stop();
                tagoIOService.stop();
                setState(SERVICES_STOPPED);
            }
        }
        else if (hasInternetConnection())
        {
            // Retry starting services
            LOG_INFO(TAG, "Retrying service connections");
            setState(SERVICES_STARTING);
            
            // Restart services
            if (novaLogicService.getStatus() == SERVICE_STOPPED || 
                novaLogicService.getStatus() == SERVICE_ERROR)
            {
                novaLogicService.start();
            }
            if (tagoIOService.getStatus() == SERVICE_STOPPED || 
                tagoIOService.getStatus() == SERVICE_ERROR)
            {
                tagoIOService.start();
            }
        }
        break;
    }
}

void ServicesManager::stop()
{
    LOG_INFO(TAG, "Stopping services orchestrator...");

    // Stop individual services
    novaLogicService.stop();
    tagoIOService.stop();

    setState(SERVICES_STOPPED);
    initialized = false;

    LOG_INFO(TAG, "Services orchestrator stopped");
}

ServicesStatus ServicesManager::getState() const
{
    return currentState;
}

bool ServicesManager::isConnected() const
{
    return currentState == SERVICES_CONNECTED;
}

bool ServicesManager::isNovaLogicConnected() const
{
    return novaLogicService.isConnected();
}

bool ServicesManager::isTagoIOConnected() const
{
    return tagoIOService.isConnected();
}

void ServicesManager::setState(ServicesStatus newState)
{
    if (currentState != newState)
    {
        LOG_DEBUG(TAG, "State change: %d -> %d", currentState, newState);

        ServicesStatus oldState = currentState;
        currentState = newState;
        
        // Update status view model
        statusViewModel.setServicesStatus(currentState);

        if (stateChangeCallback)
        {
            stateChangeCallback(newState);
        }

        // Log state changes
        switch (newState)
        {
        case SERVICES_STOPPED:
            LOG_DEBUG(TAG, "Services status: STOPPED");
            break;
        case SERVICES_STARTING:
            LOG_DEBUG(TAG, "Services status: STARTING");
            break;
        case SERVICES_CONNECTING:
            LOG_DEBUG(TAG, "Services status: CONNECTING");
            break;
        case SERVICES_CONNECTED:
            LOG_DEBUG(TAG, "Services status: CONNECTED");
            break;
        case SERVICES_ERROR:
            LOG_DEBUG(TAG, "Services status: ERROR");
            break;
        case SERVICES_NOT_CONNECTED:
            LOG_DEBUG(TAG, "Services status: NOT_CONNECTED");
            break;
        }
    }
}

void ServicesManager::updateOverallStatus()
{
    ServiceStatus novaLogicStatus = novaLogicService.getStatus();
    ServiceStatus tagoStatus = tagoIOService.getStatus();

    // Determine overall status based on individual service states
    ServicesStatus newOverallStatus = currentState;

    // Both services connected = CONNECTED
    if (novaLogicStatus == SERVICE_CONNECTED && tagoStatus == SERVICE_CONNECTED)
    {
        newOverallStatus = SERVICES_CONNECTED;
    }
    // Any service connecting = CONNECTING
    else if (novaLogicStatus == SERVICE_CONNECTING || tagoStatus == SERVICE_CONNECTING)
    {
        newOverallStatus = SERVICES_CONNECTING;
    }
    // Any service starting = STARTING
    else if (novaLogicStatus == SERVICE_STARTING || tagoStatus == SERVICE_STARTING)
    {
        if (currentState == SERVICES_STOPPED)
        {
            newOverallStatus = SERVICES_STARTING;
        }
    }
    // Any service error = ERROR
    else if (novaLogicStatus == SERVICE_ERROR || tagoStatus == SERVICE_ERROR)
    {
        newOverallStatus = SERVICES_ERROR;
    }
    // Any service not connected = NOT_CONNECTED (unless we're in a starting state)
    else if ((novaLogicStatus == SERVICE_NOT_CONNECTED || tagoStatus == SERVICE_NOT_CONNECTED) &&
             currentState != SERVICES_STARTING)
    {
        newOverallStatus = SERVICES_NOT_CONNECTED;
    }
    // Both services stopped = STOPPED
    else if (novaLogicStatus == SERVICE_STOPPED && tagoStatus == SERVICE_STOPPED)
    {
        newOverallStatus = SERVICES_STOPPED;
    }

    if (newOverallStatus != currentState)
    {
        LOG_DEBUG(TAG, "Overall status update: NovaLogic=%d, TagoIO=%d -> Overall=%d", 
                    novaLogicStatus, tagoStatus, newOverallStatus);
        setState(newOverallStatus);
    }
}

void ServicesManager::onServiceStatusChange(ServiceStatus status)
{
    // Trigger immediate status evaluation when any service changes state
    updateOverallStatus();
    
    // Check current MQTT connectivity state and notify logging manager if changed
    bool currentMQTTConnected = novaLogicService.isConnected();
    
    if (lastMQTTConnectedState != currentMQTTConnected)
    {
        if (globalLoggingManager)
        {
            if (currentMQTTConnected)
            {
                LOG_DEBUG(TAG, "MQTT connectivity established, notifying logging manager");
                globalLoggingManager->onMQTTConnected();
            }
            else
            {
                LOG_DEBUG(TAG, "MQTT connectivity lost, notifying logging manager");
                globalLoggingManager->onMQTTDisconnected();
            }
        }
        lastMQTTConnectedState = currentMQTTConnected;
    }
}

bool ServicesManager::hasInternetConnection()
{
    return connectivityManager.isOnline();
}

// Delegated functions to NovaLogic service
void ServicesManager::sendDeviceModel()
{
    novaLogicService.sendDeviceModel();
}

void ServicesManager::sendFirmwareVersion()
{
    novaLogicService.sendFirmwareVersion();
}

void ServicesManager::sendConnectionStatus(bool connected)
{
    novaLogicService.sendConnectionStatus(connected);
}

void ServicesManager::checkOTAVersion()
{
    novaLogicService.checkOTAVersion();
}

// Delegated functions to TagoIO service
void ServicesManager::publishSensorData(const char* variable, float value, const char* unit)
{
    tagoIOService.publishSensorData(variable, value, unit);
}

void ServicesManager::publishDeviceStatus(const char* status)
{
    tagoIOService.publishDeviceStatus(status);
}

void ServicesManager::setStateChangeCallback(std::function<void(ServicesStatus)> callback)
{
    stateChangeCallback = callback;
}

void ServicesManager::onConnectivityChanged(ConnectivityStatus connectivityStatus)
{
    // Only act on final connectivity states, ignore intermediate states like CHECKING
    if (connectivityStatus == CONNECTIVITY_CHECKING)
    {
        // LOG_DEBUG(TAG, "Ignoring intermediate CHECKING state, waiting for final result");
        return;
    }
    
    // Force immediate state evaluation instead of waiting for next polling cycle
    updateOverallStatus();
    
    // Process state machine logic immediately based on new connectivity
    unsigned long now = millis();
    switch (currentState)
    {
    case SERVICES_STOPPED:
        if (connectivityStatus == CONNECTIVITY_ONLINE)
        {
            LOG_INFO(TAG, "Internet connectivity restored, starting services");
            setState(SERVICES_STARTING);
            
            if (novaLogicService.getStatus() == SERVICE_STOPPED)
            {
                novaLogicService.start();
            }
            if (tagoIOService.getStatus() == SERVICE_STOPPED)
            {
                tagoIOService.start();
            }
        }
        break;

    case SERVICES_STARTING:
    case SERVICES_CONNECTING:
    case SERVICES_CONNECTED:
        if (connectivityStatus == CONNECTIVITY_OFFLINE)
        {
            LOG_WARN(TAG, "Internet connectivity lost immediately, stopping services");
            novaLogicService.stop();
            tagoIOService.stop();
            setState(SERVICES_STOPPED);
        }
        break;

    case SERVICES_ERROR:
    case SERVICES_NOT_CONNECTED:
        if (connectivityStatus == CONNECTIVITY_OFFLINE)
        {
            if (currentState != SERVICES_STOPPED)
            {
                novaLogicService.stop();
                tagoIOService.stop();
                setState(SERVICES_STOPPED);
            }
        }
        else if (connectivityStatus == CONNECTIVITY_ONLINE)
        {
            LOG_INFO(TAG, "Internet connectivity restored, retrying service connections");
            setState(SERVICES_STARTING);
            
            if (novaLogicService.getStatus() == SERVICE_STOPPED || 
                novaLogicService.getStatus() == SERVICE_ERROR)
            {
                novaLogicService.start();
            }
            if (tagoIOService.getStatus() == SERVICE_STOPPED || 
                tagoIOService.getStatus() == SERVICE_ERROR)
            {
                tagoIOService.start();
            }
        }
        break;
    }
}
