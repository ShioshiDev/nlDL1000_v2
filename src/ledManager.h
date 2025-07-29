#pragma once
#ifndef __LEDMANAGER_H__
#define __LEDMANAGER_H__

#include <Arduino.h>
#include <FastLED.h>
#include <definitions.h>

void TaskLEDsUpdate(void *pvParameters);
void updateLEDs();

class LEDManager
{

private:
	CRGBArray<RGBLED_COUNT> _rgbLEDs;
	CRGB _ledColours[RGBLED_COUNT];
	bool _ledStates[RGBLED_COUNT];

	bool _isOn = false;
	int _colorRed = 0;
	int _colorGreen = 0;
	int _colorBlue = 0;
	int _brightness = RGBLED_MAX_BRIGHTNESS;
	bool _isPulsing = false;
	bool _isRainbow = false;

	int currentColourIndex = 0;
	int currentBrightness = 0;
	int pulseDirection = 1;

public:
    LEDManager();
    void init();
    void toggle();
	void on();
	void off();
	void update();
	void togglePulsing();
	void toggleRainbow();
	void setColour(int red, int green, int blue); // Range 0-255
	void setBrightness(int brightness);			  // Range 0-255

	void setLEDColour(int index, CRGB colour);
	void setLEDState(int index, bool state);
};

#endif // __LEDMANAGER_H__