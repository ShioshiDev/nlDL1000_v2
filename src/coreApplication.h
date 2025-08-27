#pragma once
#ifndef __COREMANAGER_H__
#define __COREMANAGER_H__

#include <Arduino.h>
#include "esp_mac.h"
#include <U8g2lib.h>
#include <ArtronShop_PCF85363.h>
#include <Keypad.h>
#include <BlockNot.h>
#include "LittleFS.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include <memory>
#include "mbedtls/md.h"

#include "definitions.h"
#include "statusViewModel.h"
#include "managers/displayManager.h"
#include "managers/ledManager.h"
#include "managers/networkingManager.h"
#include "managers/connectivityManager.h"
#include "managers/servicesManager.h"
#include "managers/loggingManager.h"

void coreSetup();
void coreLoop();

// External manager references
extern ServicesManager servicesManager;
extern LoggingManager loggingManager;

// Task Functions
void TaskManagersUpdate(void *pvParameters);

String getMacAddress();
String getSerialNumber();

void initDisplay();
void initLEDs();
bool initRTC();

void updateKeyPad();
void keypadEvent(KeypadEvent key);
void onKeyRelease(KeypadEvent key);
void onKeyPress(KeypadEvent key);
void checkFactoryResetCombo();
void performFactoryReset();

void showSerialMenu();
void handleOption1();
void handleOption2();
void handleOption3();
void handleOption4();
void handleOption5();
void handleOption6();
void handleOption7();
void handleOption8();
void handleOption9();
void runTestCodeBlock();

void handleExternalMQTTCommand(const char *topic, const char *payload);

#endif // __COREMANAGER_H__