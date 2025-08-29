#pragma once
#ifndef __DEFINITIONS_H__
#define __DEFINITIONS_H__

#define FIRMWARE_VERSION "1.0.7" // Current Firmware Version in format X.YY.ZZ

// Device Name for Identification ---------------------------------------------------------
#define DEVICE_NAME "DL1000"
#define DEVICE_FRIENDLY_ID "NOVALOGIC DL1000"
#define DEVICE_MODEL "DL1000v1"

// Pin Definitions ------------------------------------------------------------------------
#define BOARD_PIN_RGBLED_BOARD 8
#define BOARD_PIN_RGBLED_STRIP 42

#define BOARD_PIN_BUZZER 46

#define BOARD_PIN_BUTTON_MATRIX_R1 35
#define BOARD_PIN_BUTTON_MATRIX_R2 36
#define BOARD_PIN_BUTTON_MATRIX_R3 37
#define BOARD_PIN_BUTTON_MATRIX_C1 38
#define BOARD_PIN_BUTTON_MATRIX_C2 45

#define BOARD_PIN_I2C_SDA 4
#define BOARD_PIN_I2C_SCL 5

#define BOARD_PIN_SCK 11
#define BOARD_PIN_MISO 13
#define BOARD_PIN_MOSI 12
#define BOARD_PIN_SD_CS 2
#define BOARD_PIN_UART_CS 9
#define BOARD_PIN_ADS1256_CS 10
#define BOARD_PIN_ETHERNET_1_CS 7
#define BOARD_PIN_ETHERNET_2_CS 6 // Used by the OLED Screen for Reset
#define BOARD_PIN_ETHERNET_1_INT 8
#define BOARD_PIN_ETHERNET_2_INT 15
#define BOARD_PIN_ETHERNET_RESET 16

#define BOARD_PIN_OLED_SCREEN_RESET 6

#define BOARD_PIN_RS485_TX 17
#define BOARD_PIN_RS485_RX 18
#define BOARD_PIN_RS485_DE_RE 0
#define BOARD_PIN_RS485_RX_EN 1

// I2C Address Definitions ----------------------------------------------------------------
#define I2C_OLED_SCREEN 0x3C
#define I2C_ADDRESS_RTC 0x68

// Display Attributes ---------------------------------------------------------------------
#define LCD_HOR_RES 128
#define LCD_VER_RES 64

// Interval Definitions -------------------------------------------------------------------
#define DISPLAY_UPDATE_INTERVAL 200        // 200 milliseconds
#define LED_UPDATE_INTERVAL 100            // 100 milliseconds
#define KEEP_ALIVE_INTERVAL 10000          // 10 seconds
#define MODBUS_ACTIVITY_INTERVAL 2500      // 2.5 seconds
#define MODBUS_VALIDITY_INTERVAL 250       // 250 milliseconds

// Manager Configuration -------------------------------------------------------------------
// NetworkingManager (Ethernet) Constants
#define NETWORKING_CONNECT_TIMEOUT_MS 30000 // How long to wait for IP after cable plugged (30 seconds)
#define NETWORKING_RETRY_INTERVAL_MS 5000   // Wait before retrying ethernet connection (5 seconds)

// Connectivity Manager Constants ---------------------------------------------------------
#define CONNECTIVITY_PING_TIMEOUT_MS 5000    // 5 seconds ping timeout
#define CONNECTIVITY_PING_RETRY_COUNT 3      // Number of ping retries
#define CONNECTIVITY_PING_INTERVAL_MS 30000  // 30 seconds between connectivity checks
#define CONNECTIVITY_RETRY_INTERVAL_MS 10000 // 10 seconds retry interval on failure
#define CONNECTIVITY_PING_HOST "www.novalogic.io" // Default ping target

// ServicesManager (MQTT) Constants
#define SERVICES_CONNECT_RETRY_INTERVAL_MS 15000 // Wait before retrying MQTT connect (ms)
#define SERVICES_KEEPALIVE_INTERVAL_MS 10000     // MQTT keepalive interval (ms)
#define SERVICES_CONNECTION_TIMEOUT_MS 30000     // MQTT connection timeout (ms)

// TagoIO Service Constants
#define TAGOIO_UPDATE_INTERVAL 60000               // 60 seconds

// RGB LED Definitions --------------------------------------------------------------------
#define RGBLED_COUNT 3
#define RGBLED_MAX_BRIGHTNESS 128

// MQTT Definitions -----------------------------------------------------------------------
#define MQTT_SERVER_NL_URL "rabbitmq.vbitech.com"
#define MQTT_SERVER_NL_PORT 1883

#define MQTT_SERVER_TAGO_URL "mqtt.tago.io"
#define MQTT_SERVER_TAGO_PORT 1883
#define MQTT_SERVER_TAGO_TOPIC "readings"

// MQTT Command Definitions ----------------------------------------------------------------
// Client Commands
#define MQTT_DVC_CMD_VERSION "REQUEST_FIRMWARE_VERSION"
#define MQTT_DVC_CMD_UPDATE "REQUEST_FIRMWARE_UPDATE"

// Server Commands
#define MQTT_SVR_CMD_DEVICE_MODEL "REQUEST_DEVICE_MODEL"
#define MQTT_SVR_CMD_FIRMWARE_VERSION "REQUEST_DEVICE_FIRMWARE_VERSION"

// Misc Definitions -----------------------------------------------------------------------

// Enum Definitions -----------------------------------------------------------------------

enum DisplayMode
{
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

enum DeviceStatus
{
    DEVICE_STARTED,
    DEVICE_UPDATING,
    DEVICE_UPDATE_FAILED
};

enum NetworkStatus
{
    NETWORK_STOPPED,
    NETWORK_STARTED,
    NETWORK_DISCONNECTED,
    NETWORK_LOST_IP,
    NETWORK_CONNECTED,
    NETWORK_CONNECTED_IP
};

enum ConnectivityStatus
{
    CONNECTIVITY_OFFLINE,
    CONNECTIVITY_CHECKING,
    CONNECTIVITY_ONLINE,
};

enum ServicesStatus
{
    SERVICES_STOPPED,
    SERVICES_STARTING,
    SERVICES_CONNECTING,
    SERVICES_CONNECTED,
    SERVICES_ERROR,
    SERVICES_NOT_CONNECTED
};

enum MenuState
{
    MENU_MAIN,
    MENU_DEVICE_INFO,
    MENU_ETHERNET_INFO,
    MENU_SETTINGS,
    MENU_SETTINGS_LOGGING
};

// Structure Definitions ------------------------------------------------------------------

struct AppSettings
{
    bool logToFile = true;
    bool logToMQTT = true;
};

#endif // __DEFINITIONS_H__