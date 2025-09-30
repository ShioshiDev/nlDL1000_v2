#pragma once
#ifndef __STATUSVIEWMODEL_H__
#define __STATUSVIEWMODEL_H__

#include <cstring>
#include "definitions.h"

class StatusViewModel
{
public:
    StatusViewModel();

    // Getters
    const char *getVersion() const;
    const char *getMacAddress() const;
    const char *getSerialNumber() const;
    DeviceStatus getDeviceStatus() const;
    NetworkStatus getNetworkStatus() const;
    ConnectivityStatus getConnectivityStatus() const;
    ServicesStatus getServicesStatus() const;
    const char *getStatusString() const;

    // Setters
    void setMacAddress(const char *mac);
    void setSerialNumber(const char *serial);
    void setDeviceStatus(DeviceStatus status);
    void setNetworkStatus(NetworkStatus status);
    void setConnectivityStatus(ConnectivityStatus status);
    void setServicesStatus(ServicesStatus status);
    void setStatusString(const char *status);
    
    // OTA management
    void setOTAActive(bool active);
    bool isOTAActive() const;

    // Dirty flag management
    bool isDirty() const;
    void clearDirty();

    // Update aggregated status string based on current states
    void updateStatusString();

private:
    // Device information
    const char *version;
    char macAddress[18];   // MAC address string "XX:XX:XX:XX:XX:XX"
    char serialNumber[32]; // Device serial number

    // Status information
    DeviceStatus deviceStatus;
    NetworkStatus networkStatus;
    ConnectivityStatus connectivityStatus;
    ServicesStatus servicesStatus;
    char statusString[16]; // Aggregated status string

    // OTA status
    bool otaActive;

    // Dirty flag
    mutable bool dirty;

    // Helper methods
    void setDirty();
};

#endif // __STATUSVIEWMODEL_H__