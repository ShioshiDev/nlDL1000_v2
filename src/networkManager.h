#pragma once
#ifndef __NETWORKMANAGER_H__
#define __NETWORKMANAGER_H__

#include <EthernetESP32.h>
#include <MacAddress.h>
#include <ESPping.h>
#include <Update.h>
#include <PicoMQTT.h>
#include <EthernetUdp.h>
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_log.h"

#include "definitions.h"
#include "credentials.h"

extern EthernetClass Ethernet1;

void TaskNetworkManager(void *pvParameters);

void initEthernetPorts();

void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);
void startIPReceivedChecker();
static void onWaitForIPTimeout(void *arg);
void startPingChecker();
static void onWaitForPingTimeout(void *arg);
void printEthernetStatus(EthernetClass &eth);
void printEthernetStatus();
bool testNetworkConnection(const char *host);

void mqttServiceLoop();
void mqttServiceInit();

void initNovaLogicServices();
extern void parseMQTTMessage(const char *topic, const char *payload);
void sendDeviceModel();
void sendFirmwareVersion();
void checkOTAVersion();
bool checkOTAVersionNewer(const char *version);
void requestOTAUpdate();
void handleOTAUpdate(PicoMQTT::IncomingPacket &packets);
void publishOTAStatus(const char *message);

void keepAliveNovaLogic();
void keepAliveRouterUDP();
void checkInternetConnectivity();
void setNeworkStatus(NetworkStatus status);
void restartEthernetPort();

#endif // __NETWORKMANAGER_H__