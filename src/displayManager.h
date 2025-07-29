#pragma once
#ifndef __DISPLAYMANAGER_H__
#define __DISPLAYMANAGER_H__

#include <Arduino.h>
#include <freertos/semphr.h>

#include <U8g2lib.h>
#include <ArtronShop_PCF85363.h>

// #include "ledManager.h"

#include "definitions.h"
#include "graphics.h"

void TaskDisplayUpdate(void *pvParameters);
void TaskRefreshDisplay(void *pvParameters);

void updateDateTime();
void updateDisplay();

void updateDisplayNormal();
void updateDisplayMenu();

#endif // __DISPLAYMANAGER_H__