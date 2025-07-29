#include <Arduino.h>
#include "coreManager.h"
#include "definitions.h"

void setup()
{
	// Initialize Serial Monitor
	Serial.begin(115200);
	delay(1000);

	DEBUG_PRINTLN("Firmware version: " + String(FIRMWARE_VERSION));

	DEBUG_PRINTLN("Booting Device Core...");

	coreSetup();

	DEBUG_PRINTLN("Booting Device Core Complete!");
}

void loop()
{
	coreLoop();
}
