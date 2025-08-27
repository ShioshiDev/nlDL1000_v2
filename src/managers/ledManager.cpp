#include "ledManager.h"

extern LEDManager hwLEDs;
extern StatusViewModel displayViewModel;

void TaskLEDsUpdate(void *pvParameters)
{
	for (;;)
	{
		updateLEDs();
		vTaskDelay(LED_UPDATE_INTERVAL / portTICK_PERIOD_MS);
	}
}

void updateLEDs()
{
	// Use the new dirty flag pattern for efficient updates
	if (displayViewModel.isDirty())
	{
		DeviceStatus currentDeviceStatus = displayViewModel.getDeviceStatus();
		NetworkStatus currentNetworkStatus = displayViewModel.getNetworkStatus();
		ConnectivityStatus currentConnectivityStatus = displayViewModel.getConnectivityStatus();
		ServicesStatus currentServiceStatus = displayViewModel.getServicesStatus();

		switch (currentDeviceStatus)
		{
		case DEVICE_STARTED:
			hwLEDs.setLEDColour(LED_SYSTEM, CRGB::Green);
			break;
		case DEVICE_UPDATING:
			hwLEDs.setLEDColour(LED_SYSTEM, CRGB::Blue);
			break;
		case DEVICE_UPDATE_FAILED:
			hwLEDs.setLEDColour(LED_SYSTEM, CRGB::Red);
			break;
		default:
			hwLEDs.setLEDColour(LED_SYSTEM, CRGB::White);
			break;
		}

		// Network LED shows network connectivity
		CRGB networkingStatusColor;

		switch (currentNetworkStatus)
		{
		case NETWORK_STOPPED:
			networkingStatusColor = CRGB::Grey;
			break;
		case NETWORK_STARTED:
			networkingStatusColor = CRGB::White;
			break;
		case NETWORK_DISCONNECTED:
			networkingStatusColor = CRGB::Orange;
			break;
		case NETWORK_CONNECTED:
			networkingStatusColor = CRGB::Yellow;
			break;
		case NETWORK_CONNECTED_IP:
			switch (currentConnectivityStatus)
			{
			case CONNECTIVITY_OFFLINE:
				networkingStatusColor = CRGB::Violet;
				break;
			case CONNECTIVITY_CHECKING:
				networkingStatusColor = CRGB::Cyan;
				break;
			case CONNECTIVITY_ONLINE:
				switch (currentServiceStatus)
				{
				case SERVICES_STOPPED:
					networkingStatusColor = CRGB::DarkBlue;
					break;
				case SERVICES_NOT_CONNECTED:
					networkingStatusColor = CRGB::DarkBlue;
					break;
				case SERVICES_STARTING:
					networkingStatusColor = CRGB::Blue;
					break;
				case SERVICES_CONNECTING:
					networkingStatusColor = CRGB::Purple;
					break;
				case SERVICES_CONNECTED:
					networkingStatusColor = CRGB::Green;
					break;
				case SERVICES_ERROR:
					networkingStatusColor = CRGB::Red;
					break;
				default:
					networkingStatusColor = CRGB::DarkBlue;
					break;
				}
				break;
			default:
				networkingStatusColor = CRGB::Violet;
				break;
			}
			break;
		default:
			networkingStatusColor = CRGB::Grey;
			break;
		}

		// Set the network LED color based on connectivity status
		hwLEDs.setLEDColour(LED_NETWORK, networkingStatusColor);

		// Clear the dirty flag after processing updates
		displayViewModel.clearDirty();
	}
}

LEDManager::LEDManager()
{
	FastLED.addLeds<NEOPIXEL, BOARD_PIN_RGBLED_STRIP>(_rgbLEDs, RGBLED_COUNT);
	init();
}

void LEDManager::init()
{
	setBrightness(RGBLED_MAX_BRIGHTNESS);
}

void LEDManager::toggle()
{
	if (_isOn)
		off();
	else
		on();
}

void LEDManager::on()
{
	FastLED.show();
	_isOn = true;
}

void LEDManager::off()
{
	_rgbLEDs.fill_solid(CRGB(0, 0, 0));
	FastLED.show();
	_isOn = false;
}

void LEDManager::togglePulsing()
{
	_isPulsing = !_isPulsing;
}

void LEDManager::toggleRainbow()
{
	_isRainbow = !_isRainbow;
}

void LEDManager::setColour(int red, int green, int blue)
{
	_colorRed = map(red, 0, 255, 0, RGBLED_MAX_BRIGHTNESS);
	_colorGreen = map(green, 0, 255, 0, RGBLED_MAX_BRIGHTNESS);
	_colorBlue = map(blue, 0, 255, 0, RGBLED_MAX_BRIGHTNESS);

	_rgbLEDs.fill_solid(CRGB(_colorRed, _colorGreen, _colorBlue));
	FastLED.show();

	_isRainbow = false;
}

void LEDManager::setBrightness(int brightness)
{
	_brightness = map(brightness, 0, 255, 0, RGBLED_MAX_BRIGHTNESS);
	FastLED.setBrightness(_brightness);
}

void LEDManager::update()
{
	if (_isOn)
	{
		if (_isPulsing)
		{
			if (pulseDirection == 1)
			{
				currentBrightness++;
				setBrightness(currentBrightness);
				if (currentBrightness == RGBLED_MAX_BRIGHTNESS)
					pulseDirection = 0;
			}
			else
			{
				currentBrightness--;
				setBrightness(currentBrightness);
				if (currentBrightness == 0)
					pulseDirection = 1;
			}
			FastLED.show();
		}
		if (_isRainbow)
		{
			currentColourIndex++;

			_rgbLEDs.fill_rainbow(0, currentColourIndex);
			FastLED.show();

			if (currentColourIndex == 255)
				currentColourIndex = 0;
		}
	}
}

void LEDManager::setLEDColour(int index, CRGB colour)
{
	int _colourRed = map(colour.red, 0, 255, 0, RGBLED_MAX_BRIGHTNESS);
	int _colourGreen = map(colour.green, 0, 255, 0, RGBLED_MAX_BRIGHTNESS);
	int _colourBlue = map(colour.blue, 0, 255, 0, RGBLED_MAX_BRIGHTNESS);

	_ledColours[index] = CRGB(_colourRed, _colourGreen, _colourBlue);
	_rgbLEDs[index] = CRGB(_colourRed, _colourGreen, _colourBlue);
	FastLED.show();
}

void LEDManager::setLEDState(int index, bool state)
{
	if (index >= 0 && index < RGBLED_COUNT)
	{
		_ledStates[index] = state;
		if (state)
		{
			_rgbLEDs[index] = _ledColours[index];
		}
		else
		{
			_rgbLEDs[index] = CRGB(0, 0, 0);
		}
		FastLED.show();
	}
}
