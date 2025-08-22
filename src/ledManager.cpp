#include "ledManager.h"

extern LEDManager hwLEDs;
extern StatusViewModel displayViewModel;
// Initialize previous states to invalid values to force initial LED update
DeviceStatus  _previousDeviceStatus = (DeviceStatus)-1;
NetworkStatus _previousNetworkStatus = (NetworkStatus)-1;
ServiceStatus _previousServiceStatus = (ServiceStatus)-1;

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
    // Compare the pointed-to values, not the pointers themselves
    if (*displayViewModel.statusDevice != _previousDeviceStatus ||
        *displayViewModel.statusNetwork != _previousNetworkStatus ||
        *displayViewModel.statusService != _previousServiceStatus)
    {

        switch (*displayViewModel.statusDevice)
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
        switch (*displayViewModel.statusNetwork)
        {
        case NETWORK_STOPPED:
            hwLEDs.setLEDColour(LED_NETWORK, CRGB::Red);
            break;
        case NETWORK_STARTED:
            hwLEDs.setLEDColour(LED_NETWORK, CRGB::White);
            break;
        case NETWORK_NOT_CONNECTED:
            hwLEDs.setLEDColour(LED_NETWORK, CRGB::Red);
            break;
        case NETWORK_CONNECTED:
            hwLEDs.setLEDColour(LED_NETWORK, CRGB::Orange);
            break;
        case NETWORK_CONNECTED_IP:
            hwLEDs.setLEDColour(LED_NETWORK, CRGB::Blue);
            break;
        case NETWORK_CONNECTED_INTERNET:
            hwLEDs.setLEDColour(LED_NETWORK, CRGB::Yellow);
            break;
        case NETWORK_CONNECTED_SERVICES:
            // When services are connected, show service status on MID LED instead
            if (*displayViewModel.statusService == SERVICE_CONNECTED)
                hwLEDs.setLEDColour(LED_NETWORK, CRGB::Green);
            else
                hwLEDs.setLEDColour(LED_NETWORK, CRGB::Yellow); // Internet but no services
            break;
        default:
            hwLEDs.setLEDColour(LED_NETWORK, CRGB::White);
            break;
        }
        
        // MID LED shows service connectivity
        switch (*displayViewModel.statusService)
        {
        case SERVICE_DISCONNECTED:
            hwLEDs.setLEDColour(LED_MID, CRGB::White);
            break;
        case SERVICE_CONNECTING:
            hwLEDs.setLEDColour(LED_MID, CRGB::Orange);
            break;
        case SERVICE_CONNECTED:
            hwLEDs.setLEDColour(LED_MID, CRGB::Green);
            break;
        case SERVICE_ERROR:
            hwLEDs.setLEDColour(LED_MID, CRGB::Red);
            break;
        default:
            hwLEDs.setLEDColour(LED_MID, CRGB::White);
            break;
        }
    }
        // Update the previous state with current values
	_previousDeviceStatus = *displayViewModel.statusDevice;
	_previousNetworkStatus = *displayViewModel.statusNetwork;
	_previousServiceStatus = *displayViewModel.statusService;
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
