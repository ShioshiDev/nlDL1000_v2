#pragma once
#ifndef __BASE_SERVICE_H__
#define __BASE_SERVICE_H__

#include <functional>
#include <Arduino.h>
#include "managers/loggingManager.h"
#include "definitions.h"

// Base service status
enum ServiceStatus
{
    SERVICE_STOPPED,
    SERVICE_STARTING,
    SERVICE_CONNECTING,
    SERVICE_CONNECTED,
    SERVICE_ERROR,
    SERVICE_NOT_CONNECTED
};

// Base class for all services
class BaseService
{
public:
    BaseService(const char* serviceName);
    virtual ~BaseService();

    // Core service interface
    virtual void begin() = 0;
    virtual void loop() = 0;
    virtual void stop() = 0;
    virtual void start() = 0;  // Start the service

    // Status management
    ServiceStatus getStatus() const { return currentStatus; }
    bool isConnected() const { return currentStatus == SERVICE_CONNECTED; }
    
    // Callback for status changes
    void setStatusChangeCallback(std::function<void(ServiceStatus)> callback);

    // Service information
    const char* getServiceName() const { return serviceName; }

protected:
    // Status management for derived classes
    void setStatus(ServiceStatus newStatus);
    
    // Connection management
    virtual bool canAttemptConnection();
    void updateLastConnectionAttempt();

    // Service properties
    const char* serviceName;
    ServiceStatus currentStatus;
    unsigned long lastConnectionAttempt;
    std::function<void(ServiceStatus)> statusChangeCallback;

private:
    static const unsigned long CONNECTION_RETRY_INTERVAL_MS = 30000; // 30 seconds
};

#endif // __BASE_SERVICE_H__
