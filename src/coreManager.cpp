#include "coreManager.h"
#include "mbedtls/md.h"

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

BlockNot tmrKeepAlive = BlockNot(KEEP_ALIVE_INTERVAL);
BlockNot tmrCheckConnectivity = BlockNot(CHECK_INTERNET_INTERVAL);

DeviceStatus statusDevice = DEVICE_STARTED;
NetworkStatus statusNetwork = NETWORK_STOPPED;
ServiceStatus statusService = SERVICE_DISCONNECTED;

StatusViewModel displayViewModel;
DisplayMode displayMode = NORMAL;

bool bIsRunningTestBlock = false;

// Define Mutex handles
SemaphoreHandle_t i2cMutex;
SemaphoreHandle_t displayModelMutex;

SemaphoreHandle_t statusDeviceMutex;
SemaphoreHandle_t statusNetworkMutex;
SemaphoreHandle_t statusServiceMutex;

// Core Setup and Loop Functions ----------------------------------------------------------

void coreSetup()
{
	// Initialize View Model
	displayViewModel = {
		.mac = DEVICE_MAC.c_str(),
		.serial = DEVICE_SERIAL.c_str(),
		.statusDevice = &statusDevice,
		.statusNetwork = &statusNetwork,
		.statusService = &statusService,
		.status = "Initializing"};

	// Create Mutexes
	i2cMutex = xSemaphoreCreateMutex();
	displayModelMutex = xSemaphoreCreateMutex();
	statusDeviceMutex = xSemaphoreCreateMutex();
	statusNetworkMutex = xSemaphoreCreateMutex();
	statusServiceMutex = xSemaphoreCreateMutex();

	// Initialize hardware components
	DEBUG_PRINTLN(F("... Initializing I2C Bus"));
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

	DEBUG_PRINTLN(F("... Initializing Display"));
	initDisplay();

	DEBUG_PRINTLN(F("... Initializing LEDs"));
	initLEDs();

	DEBUG_PRINTLN("... Initializing Ethernet Ports");
	initEthernetPorts();

	DEBUG_PRINTLN(F("... Initializing Button Matrix"));
	keypad.setDebounceTime(20);
	keypad.addEventListener(keypadEvent);

	// Create Tasks
	xTaskCreatePinnedToCore(TaskDisplayUpdate, "TaskDisplayUpdate", 8192, NULL, 7, NULL, 1);
	xTaskCreatePinnedToCore(TaskLEDsUpdate, "TaskLEDsUpdate", 2048, NULL, 6, NULL, 1);
	xTaskCreatePinnedToCore(TaskNetworkManager, "TaskNetworkManager", 8192, NULL, 8, NULL, 1);
}

void coreLoop()
{
	// Check for serial input to show the menu
	if (Serial.read() == '-')
		showSerialMenu();

	// Check for keypad events but handle them in the event handler
	keypad.getKey();

	// Network connectivity and services are now handled entirely in TaskNetworkManager
	// This reduces main loop complexity and provides better state management

	// MQTT service loop is called here to handle message processing
	// but connection management is handled in the network task
	mqttServiceLoop();
}

// Hardware Initialization Functions ------------------------------------------------------

String getMacAddress()
{
	String macAddress = "";
	uint8_t base_mac_addr[6];
	esp_err_t er = esp_efuse_mac_get_default(base_mac_addr);
	if (er != ESP_OK)
	{
		DEBUG_PRINTLN("Failed to get MAC address");
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
		DEBUG_PRINTLN("Failed to get MAC address");
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
			strWidth = hwDisplay.getStrWidth(displayViewModel.version);
			hwDisplay.drawStr((128 - strWidth - 4), 40, displayViewModel.version);
			hwDisplay.drawStr(4, 50, "MAC: ");
			strWidth = hwDisplay.getStrWidth(displayViewModel.mac);
			hwDisplay.drawStr((128 - strWidth - 4), 50, displayViewModel.mac);
			hwDisplay.drawStr(4, 60, "Serial Number: ");
			strWidth = hwDisplay.getStrWidth(displayViewModel.serial);
			hwDisplay.drawStr((128 - strWidth - 4), 60, displayViewModel.serial);
		} while (hwDisplay.nextPage());
		xSemaphoreGive(i2cMutex);
	}
	else
	{
		Serial.println("updateDateTime - Failed to take i2cMutex");
	}
}

void initLEDs()
{
	hwLEDs.init();
	hwLEDs.off();
	hwLEDs.setLEDColour(LED_SYSTEM, CRGB::Green);
}

// Keypad Event Handler -------------------------------------------------------------------

void keypadEvent(KeypadEvent key)
{
	switch (keypad.getState())
	{
	case PRESSED:
		Serial.print(key);
		Serial.println(F(" - Pressed"));
		break;
	case RELEASED:
		Serial.print(key);
		Serial.println(F(" - Released"));
		onKeyRelease(key);
		break;
	case HOLD:
		Serial.print(key);
		Serial.println(F(" - Hold"));
		break;
	default:
		break;
	}
}

void onKeyRelease(KeypadEvent key)
{
	switch (key)
	{
	case 'S':
		// Select button pressed
		Serial.println(F("Select button pressed"));
		break;
	case 'M':
		// Menu button pressed
		Serial.println(F("Menu button pressed"));
		break;
	case 'D':
		// Down button pressed
		Serial.println(F("Down button pressed"));
		break;
	case 'U':
		// Up button pressed
		Serial.println(F("Up button pressed"));
		// Print Ethernet Status
		printEthernetStatus();
		break;
	case 'R':
		// Right button pressed
		Serial.println(F("Right button pressed"));
		break;
	case 'L':
		// Left button pressed
		Serial.println(F("Left button pressed"));
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
	printEthernetStatus();
}

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

// MQTT Related Functions  ----------------------------------------------------------------

void parseMQTTMessage(const char *topic, const char *payload)
{
	DEBUG_PRINTLN("MQTT message received on topic: " + String(topic) + " with payload: " + String(payload));

	if (strcmp(payload, MQTT_SRV_CMD_DEVICE_MODEL) == 0)
	{
		DEBUG_PRINTLN("Device model requested...");
		sendDeviceModel();
		return;
	}
	if (strcmp(payload, MQTT_SRV_CMD_FIRMWARE_VERSION) == 0)
	{
		DEBUG_PRINTLN("Device Firmware Version requested...");
		sendFirmwareVersion();
		return;
	}
}
