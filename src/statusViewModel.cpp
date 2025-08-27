#include "statusViewModel.h"

StatusViewModel::StatusViewModel()
    : version(FIRMWARE_VERSION), deviceStatus(DEVICE_STARTED), networkStatus(NETWORK_STOPPED), connectivityStatus(CONNECTIVITY_OFFLINE), servicesStatus(SERVICES_STOPPED), dirty(true) // Start as dirty to trigger initial updates
{
    // Initialize strings
    strncpy(macAddress, "", sizeof(macAddress) - 1);
    macAddress[sizeof(macAddress) - 1] = '\0';

    strncpy(serialNumber, "", sizeof(serialNumber) - 1);
    serialNumber[sizeof(serialNumber) - 1] = '\0';

    strncpy(statusString, "INIT", sizeof(statusString) - 1);
    statusString[sizeof(statusString) - 1] = '\0';

    updateStatusString();
}

// Getters
const char *StatusViewModel::getVersion() const
{
    return version;
}

const char *StatusViewModel::getMacAddress() const
{
    return macAddress;
}

const char *StatusViewModel::getSerialNumber() const
{
    return serialNumber;
}

DeviceStatus StatusViewModel::getDeviceStatus() const
{
    return deviceStatus;
}

NetworkStatus StatusViewModel::getNetworkStatus() const
{
    return networkStatus;
}

ConnectivityStatus StatusViewModel::getConnectivityStatus() const
{
    return connectivityStatus;
}

ServicesStatus StatusViewModel::getServicesStatus() const
{
    return servicesStatus;
}

const char *StatusViewModel::getStatusString() const
{
    return statusString;
}

// Setters
void StatusViewModel::setMacAddress(const char *mac)
{
    if (mac && strcmp(macAddress, mac) != 0)
    {
        strncpy(macAddress, mac, sizeof(macAddress) - 1);
        macAddress[sizeof(macAddress) - 1] = '\0';
        setDirty();
    }
}

void StatusViewModel::setSerialNumber(const char *serial)
{
    if (serial && strcmp(serialNumber, serial) != 0)
    {
        strncpy(serialNumber, serial, sizeof(serialNumber) - 1);
        serialNumber[sizeof(serialNumber) - 1] = '\0';
        setDirty();
    }
}

void StatusViewModel::setDeviceStatus(DeviceStatus status)
{
    if (deviceStatus != status)
    {
        deviceStatus = status;
        setDirty();
        updateStatusString();
    }
}

void StatusViewModel::setNetworkStatus(NetworkStatus status)
{
    if (networkStatus != status)
    {
        networkStatus = status;
        setDirty();
        updateStatusString();
    }
}

void StatusViewModel::setConnectivityStatus(ConnectivityStatus status)
{
    if (connectivityStatus != status)
    {
        connectivityStatus = status;
        setDirty();
        updateStatusString();
    }
}

void StatusViewModel::setServicesStatus(ServicesStatus status)
{
    if (servicesStatus != status)
    {
        servicesStatus = status;
        setDirty();
        updateStatusString();
    }
}

void StatusViewModel::setStatusString(const char *status)
{
    if (status && strcmp(statusString, status) != 0)
    {
        strncpy(statusString, status, sizeof(statusString) - 1);
        statusString[sizeof(statusString) - 1] = '\0';
        setDirty();
    }
}

// Dirty flag management
bool StatusViewModel::isDirty() const
{
    return dirty;
}

void StatusViewModel::clearDirty()
{
    dirty = false;
}

void StatusViewModel::setDirty()
{
    dirty = true;
}

void StatusViewModel::updateStatusString()
{
    // Create aggregated status based on current states
    if (deviceStatus == DEVICE_UPDATING)
    {
        strncpy(statusString, "UPDATING", sizeof(statusString) - 1);
    }
    else if (deviceStatus == DEVICE_UPDATE_FAILED)
    {
        strncpy(statusString, "UPD_FAIL", sizeof(statusString) - 1);
    }
    else if (servicesStatus == SERVICES_CONNECTED)
    {
        strncpy(statusString, "ONLINE", sizeof(statusString) - 1);
    }
    else if (servicesStatus == SERVICES_CONNECTING)
    {
        strncpy(statusString, "CONNECTING", sizeof(statusString) - 1);
    }
    else if (servicesStatus == SERVICES_ERROR)
    {
        strncpy(statusString, "SVC_ERROR", sizeof(statusString) - 1);
    }
    else if (connectivityStatus == CONNECTIVITY_ONLINE)
    {
        strncpy(statusString, "INET_OK", sizeof(statusString) - 1);
    }
    else if (connectivityStatus == CONNECTIVITY_CHECKING)
    {
        strncpy(statusString, "CHECKING", sizeof(statusString) - 1);
    }
    else if (networkStatus == NETWORK_CONNECTED_IP)
    {
        strncpy(statusString, "ETH_OK", sizeof(statusString) - 1);
    }
    else if (networkStatus == NETWORK_DISCONNECTED)
    {
        strncpy(statusString, "ETH_DISC", sizeof(statusString) - 1);
    }
    else
    {
        strncpy(statusString, "STARTING", sizeof(statusString) - 1);
    }

    statusString[sizeof(statusString) - 1] = '\0';
    setDirty();
}