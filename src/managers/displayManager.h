#pragma once
#ifndef __DISPLAYMANAGER_H__
#define __DISPLAYMANAGER_H__

#include <Arduino.h>
#include <freertos/semphr.h>

#include <U8g2lib.h>
#include <ArtronShop_PCF85363.h>

#include "coreApplication.h"
#include "services/baseService.h"
#include "graphics.h"
#include "definitions.h"
#include "statusViewModel.h"
#include "menuManager.h"

void TaskDisplayUpdate(void *pvParameters);
void TaskRefreshDisplay(void *pvParameters);

void updateDateTime();
void updateDisplay();

void updateDisplayNormal();
void updateDisplayMenu();
void updateDisplayFactoryResetConfirm();

// Menu system functions
void handleMenuKeyPress(char key);
void startMenuTimeout();
void drawMainMenu();
void drawDeviceInfoMenu();
void drawEthernetInfoMenu();
void drawSettingsMenu();
void drawLoggingSettingsMenu();

// New menu system functions
void initializeMenuSystem();
void createMainMenu();
void createDeviceInfoMenu();
void createEthernetInfoMenu();
void createSettingsMenu();
void createLoggingSettingsMenu();
void showMenuSystem();
void handleMenuKeyPressNew(char key);
void cleanupMenuSystem();

// Display power management
bool updateDisplayActivity();
void checkDisplayPowerManagement();
void setDisplayBrightness(uint8_t contrast);
bool wakeDisplay();

// Settings management
void loadSettings();
void saveSettings();
AppSettings& getAppSettings();

#endif // __DISPLAYMANAGER_H__