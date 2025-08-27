#include <Arduino.h>
#include "coreApplication.h"
#include "definitions.h"
#include "managers/loggingManager.h"

void setup()
{
	// Initialize Serial Monitor
	Serial.begin(115200);
	delay(1000);

	Serial.println("Firmware version: " + String(FIRMWARE_VERSION));
	Serial.println("Booting Core Application...");

	coreSetup();

	// Now that logging is initialized, use the enhanced logger
	if (globalLoggingManager) {
		globalLoggingManager->logInfo("System", "Booting Core Application Complete!");
	} else {
		Serial.println("Booting Core Application Complete!");
	}
}

void loop()
{
	coreLoop();
}
