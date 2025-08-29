#include "coreApplication.h"

static const char* TAG = "CoreApplication";

// Hardware Definitions -------------------------------------------------------------------

U8G2_SH1106_128X64_NONAME_1_HW_I2C hwDisplay(U8G2_R0, BOARD_PIN_OLED_SCREEN_RESET);
LEDManager hwLEDs = LEDManager();

const byte BTN_ROWS = 3;
const byte BTN_COLS = 2;
char buttonMatrix[BTN_ROWS][BTN_COLS] = {
	{'S', 'M'},
	{'D', 'U'},
	{'R', 'L'}};
byte rowPins[BTN_ROWS] = {BOARD_PIN_BUTTON_MATRIX_R1, BOARD_PIN_BUTTON_MATRIX_R2, BOARD_PIN_BUTTON_MATRIX_R3};
byte colPins[BTN_COLS] = {BOARD_PIN_BUTTON_MATRIX_C1, BOARD_PIN_BUTTON_MATRIX_C2};
Keypad keypad = Keypad(makeKeymap(buttonMatrix), rowPins, colPins, BTN_ROWS, BTN_COLS);

// Global Variables -----------------------------------------------------------------------

const String DEVICE_ID = getSerialNumber();
const String DEVICE_MAC = getMacAddress();
const String DEVICE_SERIAL = getSerialNumber();

DeviceStatus statusDevice = DEVICE_STARTED;

StatusViewModel displayViewModel;
DisplayMode displayMode = NORMAL;

// Manager instances for three-tier architecture
LoggingManager loggingManager;
NetworkingManager networkingManager(displayViewModel);
ConnectivityManager connectivityManager(networkingManager, displayViewModel);
ServicesManager servicesManager(connectivityManager, displayViewModel);

bool bIsRunningTestBlock = false;

// Factory Reset Variables
bool factoryResetComboPressed = false;
unsigned long factoryResetStartTime = 0;
const unsigned long FACTORY_RESET_HOLD_TIME = 5000; // 5 seconds
uint8_t factoryResetSelection = 0;					// 0 = Cancel, 1 = Confirm
bool ignoreNextSRelease = false;
bool ignoreNextMRelease = false;

// Define Mutex handles
SemaphoreHandle_t i2cMutex;
SemaphoreHandle_t displayModelMutex;

SemaphoreHandle_t statusDeviceMutex;
SemaphoreHandle_t statusNetworkMutex;
SemaphoreHandle_t statusServiceMutex;

// Core Setup and Loop Functions ----------------------------------------------------------

void coreSetup()
{
	// Initialize enhanced logging system first
	LOG_INFO(TAG, "Initializing Enhanced Logging System");
	loggingManager.begin();
	
	// Initialize View Model
	displayViewModel.setMacAddress(DEVICE_MAC.c_str());
	displayViewModel.setSerialNumber(DEVICE_SERIAL.c_str());
	displayViewModel.setDeviceStatus(statusDevice);
	displayViewModel.setStatusString("Initializing");

	// Create Mutexes
	i2cMutex = xSemaphoreCreateMutex();
	displayModelMutex = xSemaphoreCreateMutex();
	statusDeviceMutex = xSemaphoreCreateMutex();
	statusServiceMutex = xSemaphoreCreateMutex();

	// Initialize hardware components
	LOG_INFO(TAG, "Initializing I2C Bus");
	Wire.begin(BOARD_PIN_I2C_SDA, BOARD_PIN_I2C_SCL);

	// Quick reset of the display
	pinMode(BOARD_PIN_OLED_SCREEN_RESET, OUTPUT);
	digitalWrite(BOARD_PIN_OLED_SCREEN_RESET, LOW);
	vTaskDelay(10);
	digitalWrite(BOARD_PIN_OLED_SCREEN_RESET, LOW);

	// Disable unused SPI device selection pins
	pinMode(BOARD_PIN_ETHERNET_1_CS, OUTPUT);
	pinMode(BOARD_PIN_ETHERNET_2_CS, OUTPUT);
	pinMode(BOARD_PIN_ADS1256_CS, OUTPUT);
	pinMode(BOARD_PIN_SD_CS, OUTPUT);
	pinMode(BOARD_PIN_UART_CS, OUTPUT);
	digitalWrite(BOARD_PIN_ETHERNET_2_CS, HIGH);
	digitalWrite(BOARD_PIN_ADS1256_CS, HIGH);
	digitalWrite(BOARD_PIN_SD_CS, HIGH);

	LOG_INFO(TAG, "Initializing Display");
	initDisplay();

	LOG_INFO(TAG, "Initializing LEDs");
	initLEDs();

	LOG_INFO(TAG, "Initializing Networking Managers");
	networkingManager.begin();
	LOG_INFO(TAG, "Initializing Connectivity Manager");
	connectivityManager.begin();
	LOG_INFO(TAG, "Initializing Services Manager");
	servicesManager.begin();

	// Setup connectivity change callback for immediate service state updates
	connectivityManager.setCallback([](ConnectivityStatus status) {
		servicesManager.onConnectivityChanged(status);
	});

	// Setup NovaLogic service command callback for external processing
	servicesManager.getNovaLogicService().setCommandCallback([](const char* topic, const char* payload) {
		handleExternalMQTTCommand(topic, payload);
	});

	LOG_INFO(TAG, "Initializing Button Matrix");
	keypad.setDebounceTime(20);
	// Using direct key scanning in updateKeyPad() instead of event listener

	LOG_INFO(TAG, "Loading Application Settings");
	loadSettings();

	// Create Tasks
	xTaskCreatePinnedToCore(TaskDisplayUpdate, "TaskDisplayUpdate", 8192, NULL, 7, NULL, 1);
	xTaskCreatePinnedToCore(TaskLEDsUpdate, "TaskLEDsUpdate", 2048, NULL, 6, NULL, 1);
	xTaskCreatePinnedToCore(TaskManagersUpdate, "TaskManagersUpdate", 8192, NULL, 8, NULL, 1);
}

void coreLoop()
{
	// Check for serial input to show the menu
	if (Serial.read() == '-')
		showSerialMenu();

	// Check for keypad events but handle them in the event handler
	updateKeyPad();

	// Check for factory reset combo
	checkFactoryResetCombo();
}

// Task Functions -----------------------------------------------------------------------

void TaskManagersUpdate(void *pvParameters)
{
	for (;;)
	{
		// Run the state machine managers loop functions
		loggingManager.loop();
		networkingManager.loop();
		connectivityManager.loop();
		servicesManager.loop();

		// Small delay to prevent task from hogging CPU
		vTaskDelay(pdMS_TO_TICKS(50));
	}
}

// Hardware Initialization Functions ------------------------------------------------------

String getMacAddress()
{
	String macAddress = "";
	uint8_t base_mac_addr[6];
	esp_err_t er = esp_efuse_mac_get_default(base_mac_addr);
	if (er != ESP_OK)
	{
		LOG_ERROR(TAG, "Failed to get MAC address");
		macAddress = "00:00:00:00:00:00"; // Default MAC address if retrieval fails
		return macAddress;
	}
	else
	{
		for (int i = 0; i < 6; i++)
		{
			char hexByte[3];
			sprintf(hexByte, "%02X", base_mac_addr[i]);
			macAddress += hexByte;
			if (i < 5)
			{
				macAddress += ":";
			}
		}
	}
	return macAddress;
}

String getSerialNumber()
{
	String serialNumber = "00000000"; // Default MAC address if retrieval fails
	String macAddress = getMacAddress();

	if (macAddress == "00:00:00:00:00:00")
	{
		LOG_ERROR(TAG, "Failed to get MAC address");
		return serialNumber;
	}
	else
	{
		const uint64_t BASE36_MOD = 2821109907456ULL; // 36^8
		const char *B36 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

		// Create a unique input combining MAC and device model
		// 1) Input = model salt + MAC (maximizes uniqueness per model)
		String in = DEVICE_NAME;
		in += macAddress;

		// 2) SHA-256
		unsigned char hash[32];
		const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
		if (!info)
			return String("ERRMD000");

		mbedtls_md_context_t ctx;
		mbedtls_md_init(&ctx);
		if (mbedtls_md_setup(&ctx, info, 0) != 0 ||
			mbedtls_md_starts(&ctx) != 0 ||
			mbedtls_md_update(&ctx, (const unsigned char *)in.c_str(), in.length()) != 0 ||
			mbedtls_md_finish(&ctx, hash) != 0)
		{
			mbedtls_md_free(&ctx);
			return String("ERRMD001");
		}
		mbedtls_md_free(&ctx);

		// 3) Reduce FULL digest modulo 36^8 for uniform 8-char space
		uint64_t mod = 0;
		for (int i = 0; i < 32; ++i)
		{
			mod = (((mod << 8) % BASE36_MOD) + hash[i]) % BASE36_MOD;
		}

		// 4) Base36 encode to exactly 8 chars
		char out[9];
		out[8] = '\0';
		for (int i = 7; i >= 0; --i)
		{
			out[i] = B36[mod % 36];
			mod /= 36;
		}
		serialNumber = String(out); // e.g., "3F8K1Z0Q"
		return serialNumber;
	}
}

void initDisplay()
{
	if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
	{
		hwDisplay.begin();

		hwDisplay.firstPage();
		do
		{
			int strWidth = 0;
			hwDisplay.setFont(u8g_font_unifont);
			hwDisplay.drawStr(0, 20, DEVICE_FRIENDLY_ID);
			hwDisplay.drawHLine(4, 22, 120);
			hwDisplay.setFont(u8g2_font_5x7_tr);
			hwDisplay.drawStr(4, 40, "Firmware Version: ");
			strWidth = hwDisplay.getStrWidth(displayViewModel.getVersion());
			hwDisplay.drawStr((128 - strWidth - 4), 40, displayViewModel.getVersion());
			hwDisplay.drawStr(4, 50, "MAC: ");
			strWidth = hwDisplay.getStrWidth(displayViewModel.getMacAddress());
			hwDisplay.drawStr((128 - strWidth - 4), 50, displayViewModel.getMacAddress());
			hwDisplay.drawStr(4, 60, "Serial Number: ");
			strWidth = hwDisplay.getStrWidth(displayViewModel.getSerialNumber());
			hwDisplay.drawStr((128 - strWidth - 4), 60, displayViewModel.getSerialNumber());
		} while (hwDisplay.nextPage());
		vTaskDelay(pdMS_TO_TICKS(2500));
		xSemaphoreGive(i2cMutex);
	}
	else
	{
		LOG_WARN(TAG, "updateDateTime - Failed to take i2cMutex");
	}
}

void initLEDs()
{
	hwLEDs.init();
	hwLEDs.off();
	hwLEDs.setLEDColour(LED_SYSTEM, CRGB::Green);
}

// Keypad Event Handlers ------------------------------------------------------------------

void updateKeyPad()
{
	// Use getKeys() to fill keypad.key[] array with all pressed keys
	if (keypad.getKeys())
	{
		// Check for simultaneous S+M key press for factory reset
		bool sPressed = false;
		bool mPressed = false;

		for (int i = 0; i < LIST_MAX; i++) // Scan the whole key list
		{
			if (keypad.key[i].stateChanged) // Only find keys that have changed state
			{
				char currentKey = keypad.key[i].kchar;

				switch (keypad.key[i].kstate) // Report active key state
				{
				case PRESSED:
					LOG_DEBUG(TAG, "Key %c - Pressed", currentKey);
					// Only call onKeyPress for non-factory-reset keys or when not in combo
					if (!((currentKey == 'S' || currentKey == 'M') && factoryResetComboPressed))
					{
						onKeyPress(currentKey);
					}
					break;

				case RELEASED:
					LOG_DEBUG(TAG, "Key %c - Released", currentKey);
					// Always call onKeyRelease for UI handling
					onKeyRelease(currentKey);
					break;

				case HOLD:
					LOG_DEBUG(TAG, "Key %c - Hold", currentKey);
					break;

				default:
					break;
				}
			}

			// Check if this key is currently pressed OR held (both count as "active")
			if (keypad.key[i].kstate == PRESSED || keypad.key[i].kstate == HOLD)
			{
				if (keypad.key[i].kchar == 'S')
					sPressed = true;
				if (keypad.key[i].kchar == 'M')
					mPressed = true;
			}
		}

		// Update simultaneous key press state
		bool bothPressed = sPressed && mPressed;

		// Handle factory reset combo detection - but only if not already in confirmation mode
		if (displayMode != FACTORY_RESET_CONFIRM)
		{
			if (bothPressed && !factoryResetComboPressed)
			{
				factoryResetComboPressed = true;
				factoryResetStartTime = millis();
				LOG_INFO(TAG, "Factory reset combo detected! Hold for 5 seconds...");
				displayViewModel.setStatusString("Reset combo...");
			}
			else if (!bothPressed && factoryResetComboPressed)
			{
				factoryResetComboPressed = false;
				LOG_INFO(TAG, "Factory reset combo cancelled");
				displayViewModel.setStatusString("Started");
			}
		}
	}
}

void onKeyPress(KeypadEvent key)
{
	// Factory reset combo detection is now handled in updateKeyPad()
	// This function is kept for potential future single key handling
}

void onKeyRelease(KeypadEvent key)
{
	// Factory reset combo detection is now handled in updateKeyPad()

	// Handle Factory Reset confirmation mode
	if (displayMode == FACTORY_RESET_CONFIRM)
	{
		// Check if we should ignore this key release (first release after entering confirmation mode)
		if (key == 'S' && ignoreNextSRelease)
		{
			ignoreNextSRelease = false;
			LOG_DEBUG(TAG, "Ignoring S release after factory reset confirmation start");
			return;
		}
		if (key == 'M' && ignoreNextMRelease)
		{
			ignoreNextMRelease = false;
			LOG_DEBUG(TAG, "Ignoring M release after factory reset confirmation start");
			return;
		}

		switch (key)
		{
		case 'L': // Left button - move to Cancel (0)
		case 'U': // Up button - move to Cancel (0)
			factoryResetSelection = 0;
			LOG_DEBUG(TAG, "Factory reset: Cancel selected");
			break;
		case 'R': // Right button - move to Confirm (1)
		case 'D': // Down button - move to Confirm (1)
			factoryResetSelection = 1;
			LOG_DEBUG(TAG, "Factory reset: Confirm selected");
			break;
		case 'S': // Select button - execute selection
			if (factoryResetSelection == 1)
			{
				// Confirm selected
				LOG_WARN(TAG, "Factory reset confirmed!");
				displayMode = NORMAL;
				performFactoryReset();
			}
			else
			{
				// Cancel selected
				LOG_INFO(TAG, "Factory reset cancelled");
				displayMode = NORMAL;
				displayViewModel.setStatusString("Started");
			}
			break;
		case 'M': // Menu button - cancel
			LOG_INFO(TAG, "Factory reset cancelled via menu");
			displayMode = NORMAL;
			displayViewModel.setStatusString("Started");
			break;
		}
		return; // Don't process normal key handling during factory reset
	}

	// Normal key handling
	switch (key)
	{
	case 'S':
		// Select button pressed
		LOG_DEBUG(TAG, "Select button pressed");
		if (displayMode == MENU)
		{
			handleMenuKeyPress(key);
		}
		break;
	case 'M':
		// Menu button pressed
		LOG_DEBUG(TAG, "Menu button pressed");
		if (displayMode == NORMAL)
		{
			// Enter menu mode
			displayMode = MENU;
			startMenuTimeout();
			LOG_DEBUG(TAG, "Entering menu mode");
		}
		else if (displayMode == MENU)
		{
			handleMenuKeyPress(key);
		}
		break;
	case 'D':
		// Down button pressed
		LOG_DEBUG(TAG, "Down button pressed");
		if (displayMode == MENU)
		{
			handleMenuKeyPress(key);
		}
		break;
	case 'U':
		// Up button pressed
		LOG_DEBUG(TAG, "Up button pressed");
		if (displayMode == MENU)
		{
			handleMenuKeyPress(key);
		}
		break;
	case 'R':
		// Right button pressed
		LOG_DEBUG(TAG, "Right button pressed");
		if (displayMode == MENU)
		{
			handleMenuKeyPress(key);
		}
		break;
	case 'L':
		// Left button pressed
		LOG_DEBUG(TAG, "Left button pressed");
		break;
	default:
		// Handle other keys if needed
		break;
	}
}

// Show a menu and handle user selection on the serial monitor for testing ----------------

void showSerialMenu()
{
	Serial.println(F("Serial Menu:"));
	Serial.println(F("1. Run Test Code Block"));
	Serial.println(F("2. Print Ethernet Status"));
	Serial.println(F("3. "));
	Serial.println(F("4. "));
	Serial.println(F("5. "));
	Serial.println(F("6. "));
	Serial.println(F("7. "));
	Serial.println(F("8. "));
	Serial.println(F("9. "));
	// Add more options as needed

	Serial.print(F("Enter your selection: "));
	int userSelection = 0;
	while (!Serial.available())
		;
	while (Serial.available())
		userSelection = Serial.parseInt();

	Serial.println(userSelection);

	switch (userSelection)
	{
	case 1:
		handleOption1();
		break;
	case 2:
		handleOption2();
		break;
	case 3:
		handleOption3();
		break;
	case 4:
		handleOption4();
		break;
	case 5:
		handleOption5();
		break;
	case 6:
		handleOption6();
		break;
	case 7:
		handleOption7();
		break;
	case 8:
		handleOption8();
		break;
	case 9:
		handleOption9();
		break;
	// Add more cases as needed
	default:
		Serial.println(F("Invalid selection"));
		break;
	}
}

void handleOption1()
{
	Serial.println(F("Executing Option 1"));
	// Test code block
	runTestCodeBlock();
}

void handleOption2()
{
	Serial.println(F("Executing Option 2"));

	// Print Ethernet Status
	networkingManager.printEthernetStatus();}

void handleOption3()
{
	Serial.println(F("Executing Option 3"));
}

void handleOption4()
{
	Serial.println(F("Executing Option 4"));
}

void handleOption5()
{
	Serial.println(F("Executing Option 5"));
}

void handleOption6()
{
	Serial.println(F("Executing Option 6"));
}

void handleOption7()
{
	Serial.println(F("Executing Option 7"));
}

void handleOption8()
{
	Serial.println(F("Executing Option 8"));
}

void handleOption9()
{
	Serial.println(F("Executing Option 9"));
}

// Test Code Blocks -----------------------------------------------------------------------

void runTestCodeBlock()
{
	bIsRunningTestBlock = true;

	Serial.println(F("Running test code block..."));

	if (!LittleFS.begin(false, "/littlefs", 8, "littlefs"))
	{
		Serial.println("An Error has occurred while mounting LittleFS");
		return;
	}

	File file = LittleFS.open("/config.json", "r");
	if (!file)
	{
		Serial.println("Failed to open file for reading");
		return;
	}

	Serial.println("File Content:");
	while (file.available())
	{
		Serial.write(file.read());
	}
	file.close();
	LittleFS.end();

	bIsRunningTestBlock = false;
}

// Factory Reset Functions ----------------------------------------------------------------

void checkFactoryResetCombo()
{
	if (factoryResetComboPressed)
	{
		unsigned long holdTime = millis() - factoryResetStartTime;

		if (holdTime >= FACTORY_RESET_HOLD_TIME)
		{
			// 5 seconds reached, show confirmation dialog
			factoryResetComboPressed = false;
			LOG_INFO(TAG, "Factory reset confirmation dialog starting...");
			displayViewModel.setStatusString("Confirm reset?");

			// Switch to factory reset confirmation mode
			displayMode = FACTORY_RESET_CONFIRM;
			factoryResetSelection = 0; // Default to Cancel

			// Ignore the next release of S and M keys since user is still holding them
			ignoreNextSRelease = true;
			ignoreNextMRelease = true;
		}
	}
}

void performFactoryReset()
{
	LOG_WARN(TAG, "PERFORMING FACTORY RESET!");
	displayViewModel.setStatusString("Factory Reset");

	// Find the factory application partition
	const esp_partition_t *factory = esp_partition_find_first(
		ESP_PARTITION_TYPE_APP,
		ESP_PARTITION_SUBTYPE_APP_FACTORY,
		"factory");

	if (!factory)
	{
		LOG_ERROR(TAG, "Factory partition not found. Aborting factory reset.");
		displayViewModel.setStatusString("Factory Error");
		delay(3000);
		displayViewModel.setStatusString("Started");
		return;
	}

	// Set the factory partition as the boot partition
	esp_err_t err = esp_ota_set_boot_partition(factory);
	if (err != ESP_OK)
	{
		LOG_ERROR(TAG, "esp_ota_set_boot_partition failed: 0x%X", err);
		displayViewModel.setStatusString("Boot Error");
		delay(3000);
		displayViewModel.setStatusString("Started");
		return;
	}

	LOG_WARN(TAG, "Factory reset complete. Restarting...");
	displayViewModel.setStatusString("Restarting...");
	delay(2000);
	esp_restart();
}

// MQTT Related Functions  ----------------------------------------------------------------

void handleExternalMQTTCommand(const char *topic, const char *payload)
{
	LOG_DEBUG(TAG, "External MQTT command received on topic: %s with payload: %s", topic, payload);

	// Handle logging configuration commands
	String topicStr(topic);
	String payloadStr(payload);
	
	if (topicStr.endsWith("/logging/config"))
	{
		LOG_INFO(TAG, "Logging configuration request received (simplified logging system)");
		return;
	}
	
	if (topicStr.endsWith("/logging/get_config"))
	{
		LOG_INFO(TAG, "Logging config request received (simplified logging system)");
		return;
	}
	
	if (topicStr.endsWith("/logging/get_logs"))
	{
		LOG_INFO(TAG, "Log file request received (simplified logging system)");
		return;
	}

	// Add any other custom command processing here
	LOG_WARN(TAG, "Unhandled external MQTT command: %s", payload);
}
