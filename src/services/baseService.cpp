#include "baseService.h"

BaseService::BaseService(const char* serviceName)
    : serviceName(serviceName), currentStatus(SERVICE_STOPPED), 
      lastConnectionAttempt(0), statusChangeCallback(nullptr)
{
}

BaseService::~BaseService()
{
}

void BaseService::setStatus(ServiceStatus newStatus)
{
    if (currentStatus != newStatus)
    {
        LOG_DEBUG(serviceName, "Status change: %d -> %d", currentStatus, newStatus);
        
        currentStatus = newStatus;
        
        // Call callback if set
        if (statusChangeCallback)
        {
            statusChangeCallback(newStatus);
        }

        // Log status changes with descriptive names
        switch (newStatus)
        {
        case SERVICE_STOPPED:
            LOG_DEBUG(serviceName, "Service status: STOPPED");
            break;
        case SERVICE_STARTING:
            LOG_DEBUG(serviceName, "Service status: STARTING");
            break;
        case SERVICE_CONNECTING:
            LOG_DEBUG(serviceName, "Service status: CONNECTING");
            break;
        case SERVICE_CONNECTED:
            LOG_DEBUG(serviceName, "Service status: CONNECTED");
            break;
        case SERVICE_ERROR:
            LOG_DEBUG(serviceName, "Service status: ERROR");
            break;
        case SERVICE_NOT_CONNECTED:
            LOG_DEBUG(serviceName, "Service status: NOT_CONNECTED");
            break;
        }
    }
}

void BaseService::setStatusChangeCallback(std::function<void(ServiceStatus)> callback)
{
    statusChangeCallback = callback;
}

bool BaseService::canAttemptConnection()
{
    unsigned long now = millis();
    // Allow immediate connection on first attempt (lastConnectionAttempt == 0)
    // or after retry interval has passed
    return (lastConnectionAttempt == 0) || ((now - lastConnectionAttempt) >= CONNECTION_RETRY_INTERVAL_MS);
}

void BaseService::updateLastConnectionAttempt()
{
    lastConnectionAttempt = millis();
}
