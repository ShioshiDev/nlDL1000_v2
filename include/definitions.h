#pragma once
#ifndef __DEFINITIONS_H__
#define __DEFINITIONS_H__

// Firmware Version Information -----------------------------------------------------------
#define FIRMWARE_VERSION                "1.0.4" // Current Firmware Version in format X.YY.ZZ

// Device Name for Identification ---------------------------------------------------------
#define DEVICE_NAME                     "DL1000"
#define DEVICE_FRIENDLY_ID              "NOVALOGIC DL1000"
#define DEVICE_MODEL                    "DL1000v1"

// Debugging Toggle for Serial Output -----------------------------------------------------
#define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTLN2(x, y) Serial.println(x, y)
#define DEBUG_PRINTF(x, y) Serial.printf(x, y)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTLN2(x, y)
#define DEBUG_PRINTF(x, y)
#endif

// Pin Definitions ------------------------------------------------------------------------
#define BOARD_PIN_RGBLED_BOARD          8
#define BOARD_PIN_RGBLED_STRIP          42

#define BOARD_PIN_BUZZER                46

#define BOARD_PIN_BUTTON_MATRIX_R1      35
#define BOARD_PIN_BUTTON_MATRIX_R2      36
#define BOARD_PIN_BUTTON_MATRIX_R3      37
#define BOARD_PIN_BUTTON_MATRIX_C1      38
#define BOARD_PIN_BUTTON_MATRIX_C2      45

#define BOARD_PIN_I2C_SDA               4
#define BOARD_PIN_I2C_SCL               5

#define BOARD_PIN_SCK                   11
#define BOARD_PIN_MISO                  13
#define BOARD_PIN_MOSI                  12
#define BOARD_PIN_SD_CS                 2
#define BOARD_PIN_UART_CS               9
#define BOARD_PIN_ADS1256_CS            10
#define BOARD_PIN_ETHERNET_1_CS         7
#define BOARD_PIN_ETHERNET_2_CS         6 // Used by the OLED Screen for Reset
#define BOARD_PIN_ETHERNET_1_INT        8
#define BOARD_PIN_ETHERNET_2_INT        15
#define BOARD_PIN_ETHERNET_RESET        16

#define BOARD_PIN_OLED_SCREEN_RESET     6

#define BOARD_PIN_RS485_TX              17
#define BOARD_PIN_RS485_RX              18
#define BOARD_PIN_RS485_DE_RE           0
#define BOARD_PIN_RS485_RX_EN           1

// I2C Address Definitions ----------------------------------------------------------------
#define I2C_OLED_SCREEN                 0x3C
#define I2C_ADDRESS_RTC                 0x68

// Display Attributes ---------------------------------------------------------------------
#define LCD_HOR_RES                     128
#define LCD_VER_RES                     64

// Interval Definitions -------------------------------------------------------------------
#define DISPLAY_UPDATE_INTERVAL         250   // 250 milliseconds
#define LED_UPDATE_INTERVAL             100   // 100 milliseconds
#define KEEP_ALIVE_INTERVAL             10000 // 10 seconds
#define CHECK_INTERNET_INTERVAL         2500  // 2.5 seconds
#define CHECK_INTERNET_INTERVAL_FAST    5000  // 5 seconds (when connection issues)
#define CHECK_INTERNET_INTERVAL_SLOW    30000 // 30 seconds (when stable)
#define CHECK_SERVICES_INTERVAL         10000 // 10 seconds
#define TAGO_UPDATE_INTERVAL            60000 // 60 seconds
#define MODBUS_ACTIVITY_INTERVAL        2500  // 2.5 seconds
#define MODBUS_VALIDITY_INTERVAL        250   // 250 milliseconds

// RGB LED Definitions --------------------------------------------------------------------
#define RGBLED_COUNT                    3
#define RGBLED_MAX_BRIGHTNESS           128

// MQTT Definitions -----------------------------------------------------------------------
#define MQTT_SERVER_NL_URL              "rabbitmq.vbitech.com"
#define MQTT_SERVER_NL_PORT             1883

#define MQTT_OTA_CMD_VERSION            "REQUEST_FIRMWARE_VERSION"
#define MQTT_OTA_CMD_UPDATE             "REQUEST_FIRMWARE_UPDATE"
#define MQTT_SRV_CMD_DEVICE_MODEL   	"REQUEST_DEVICE_MODEL"
#define MQTT_SRV_CMD_FIRMWARE_VERSION   "REQUEST_DEVICE_FIRMWARE_VERSION"

// Misc Definitions -----------------------------------------------------------------------

// Enum Definitions -----------------------------------------------------------------------
enum DisplayMode {
	NORMAL = 1,
	MENU = 2,
	FACTORY_RESET_CONFIRM = 3
};

enum LEDIndex
{
	LED_SYSTEM = 0,
	LED_NETWORK = 2,
	LED_MID = 1,
};

enum DeviceStatus {
    DEVICE_STARTED,
    DEVICE_UPDATING,
    DEVICE_UPDATE_FAILED
};

enum NetworkStatus {
    NETWORK_STOPPED,
    NETWORK_STARTED,
    NETWORK_NOT_CONNECTED,
    NETWORK_CONNECTED,
    NETWORK_CONNECTED_IP,
    NETWORK_CONNECTED_INTERNET,
    NETWORK_CONNECTED_SERVICES  // Renamed from MQTT to reflect broader service connectivity
};

enum ServiceStatus {
    SERVICE_DISCONNECTED,
    SERVICE_CONNECTING,
    SERVICE_CONNECTED,
    SERVICE_ERROR
};

// Structure Definitions ------------------------------------------------------------------
typedef struct  {
	const char* version = FIRMWARE_VERSION;
	const char* mac;
	const char* serial;

	DeviceStatus *statusDevice;
	NetworkStatus *statusNetwork;
	ServiceStatus *statusService;
	char status[16]; 
} StatusViewModel;

#endif // __DEFINITIONS_H__