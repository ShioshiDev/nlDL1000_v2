#include "networkManager.h"

W5500Driver ethernetDriver1(BOARD_PIN_ETHERNET_1_CS, BOARD_PIN_ETHERNET_1_INT, BOARD_PIN_ETHERNET_RESET);
EthernetClass Ethernet1;
const char *ethHostname1 = "nlDL1000-eth1";

#define PORT_RECYCLE_INTERVAL 5000			// 5 seconds
#define PORT_RECYCLE_TIMEOUT (15 * 1000000) // 15 seconds
static esp_timer_handle_t tmrWaitForIP = NULL;
static bool isWaitingForIP = false;

#define INTERNET_PING_TIMEOUT (10 * 1000000) // 10 seconds
static esp_timer_handle_t tmrWaitForPing = NULL;
static bool isWaitingForPing = false;
int pingRetry = 0;
int pingMaxRetry = 3;

IPAddress routerIP;
EthernetUDP udp;
unsigned long lastKeepAlive = 0;
const unsigned long keepAliveInterval = 10000; // 10 seconds
unsigned long lastConnectivityPing = 0;
const unsigned long connectivityPingInterval = 10000; // 10 seconds

extern DeviceStatus statusDevice;
extern NetworkStatus statusNetwork;

extern StatusViewModel displayViewModel;
extern SemaphoreHandle_t displayModelMutex;
extern String getSerialNumber();
static String deviceSerial = getSerialNumber();
const char *MQTT_DEVICE_ID = deviceSerial.c_str();

PicoMQTT::Client mqttClientNovaLogic(MQTT_SERVER_NL_URL, MQTT_SERVER_NL_PORT, MQTT_DEVICE_ID, MQTT_SERVER_NL_USERNAME, MQTT_SERVER_NL_PASSWORD);
bool hasInitializedMQTT = false;

void TaskNetworkManager(void *pvParameters)
{
	unsigned long current_time = 0;

	for (;;)
	{
		if (statusNetwork == NETWORK_CONNECTED_INTERNET)
		{
			keepAliveRouterUDP();
		}
		taskYIELD();   // Yield to other tasks
		vTaskDelay(10); // Yield to other tasks
	}
}

void initEthernetPorts()
{
	SPI.begin(BOARD_PIN_SCK, BOARD_PIN_MISO, BOARD_PIN_MOSI);

	Network.onEvent(onNetworkEvent);

	Ethernet1.init(ethernetDriver1);

	Ethernet1.begin(5000);

	udp.begin(8888);
}

void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info)
{
	arduino_event_info_t eventInfo = arduino_event_info_t(info);
	// DEBUG_PRINTLN("Network Event: " + String(event));
	switch (event)
	{
	case ARDUINO_EVENT_ETH_START:
		if (eventInfo.eth_connected == Ethernet1.getEthHandle())
		{
			DEBUG_PRINTLN("Ethernet 1 Started");
			setNeworkStatus(NETWORK_STARTED);
			Ethernet1.setHostname(ethHostname1);
		}
		break;
	case ARDUINO_EVENT_ETH_CONNECTED:
		if (eventInfo.eth_connected == Ethernet1.getEthHandle())
		{
			DEBUG_PRINTLN("Ethernet 1 Connected");
			setNeworkStatus(NETWORK_CONNECTED);
			startIPReceivedChecker();
		}
		break;
	case ARDUINO_EVENT_ETH_GOT_IP:
		if (strcmp(esp_netif_get_desc(eventInfo.got_ip.esp_netif), "eth1") == 0)
		{
			DEBUG_PRINTLN("Ethernet 1 Got IP:");
			DEBUG_PRINTLN(Ethernet1);
			routerIP = Ethernet1.gatewayIP();
			setNeworkStatus(NETWORK_CONNECTED_IP);
			startPingChecker();
		}
		break;
	case ARDUINO_EVENT_ETH_LOST_IP:
		if (statusNetwork >= NETWORK_CONNECTED_IP)
		{
			DEBUG_PRINTLN("Ethernet 1 Lost IP");
			if (Ethernet1.linkStatus() == LinkON)
				setNeworkStatus(NETWORK_CONNECTED);
			else
				setNeworkStatus(NETWORK_NOT_CONNECTED);
			startIPReceivedChecker();
		}
		break;
	case ARDUINO_EVENT_ETH_DISCONNECTED:
		DEBUG_PRINTLN("Ethernet 1 Disconnected");
		setNeworkStatus(NETWORK_NOT_CONNECTED);
		startIPReceivedChecker();
		break;
	case ARDUINO_EVENT_ETH_STOP:
		DEBUG_PRINTLN("Ethernet 1 Stopped");
		setNeworkStatus(NETWORK_STOPPED);
		break;
	default:
		break;
	}
}

void startIPReceivedChecker()
{
	// Start timer to wait for IP address
	if (!isWaitingForIP)
	{
		if (!tmrWaitForIP)
		{
			const esp_timer_create_args_t timer_args = {
				.callback = &onWaitForIPTimeout,
				.name = "eth0_ip_wait"};
			ESP_ERROR_CHECK(esp_timer_create(&timer_args, &tmrWaitForIP));
		}
		ESP_ERROR_CHECK(esp_timer_start_once(tmrWaitForIP, PORT_RECYCLE_TIMEOUT));
		isWaitingForIP = true;
	}
}

static void onWaitForIPTimeout(void *arg)
{
	if (statusNetwork == NETWORK_CONNECTED)
	{
		ESP_LOGW(TAG, "No IP address on eth0 after 15s, restarting Ethernet port 0");
		restartEthernetPort();
	}
	isWaitingForIP = false;
}

void startPingChecker()
{
	if (statusNetwork >= NETWORK_CONNECTED_IP)
	{
		if (!isWaitingForPing)
		{
			if (!tmrWaitForPing)
			{
				const esp_timer_create_args_t timer_args = {
					.callback = &onWaitForPingTimeout,
					.name = "eth0_ping_wait"};
				ESP_ERROR_CHECK(esp_timer_create(&timer_args, &tmrWaitForPing));
			}
			ESP_ERROR_CHECK(esp_timer_start_once(tmrWaitForPing, INTERNET_PING_TIMEOUT));
			isWaitingForPing = true;

			if (testNetworkConnection("www.novalogic.io"))
			{
				if (Ethernet1.hasIP())
					setNeworkStatus(NETWORK_CONNECTED_INTERNET);
			}
			else
			{
				if (Ethernet1.hasIP())
					setNeworkStatus(NETWORK_CONNECTED_IP);
				else
					setNeworkStatus(NETWORK_NOT_CONNECTED);
			}
		}
	}
}

static void onWaitForPingTimeout(void *arg)
{
	isWaitingForPing = false;
	if (statusNetwork <= NETWORK_CONNECTED_IP)
	{
		DEBUG_PRINTLN("No ping received after 10s, restarting ping test.");

		if (pingRetry < pingMaxRetry)
		{
			startPingChecker();
			pingRetry++;
			DEBUG_PRINTF("Ping test retry #%d\n", pingRetry);
		}
		else
		{
			DEBUG_PRINTF("Ping test failed after %d retries, restarting Ethernet port 1.\n", pingMaxRetry);
			restartEthernetPort();
			pingRetry = 0;
		}
	}
}

void printEthernetStatus(EthernetClass &eth)
{
	byte mac[6];
	eth.MACAddress(mac);
	Serial.print("MAC: ");
	Serial.println(MacAddress(mac));
	if (mac[0] & 1)
	{ // unicast bit is set
		Serial.println("\t is the ordering of the MAC address bytes reversed?");
	}

	Serial.print("IP Address: ");
	Serial.println(eth.localIP());

	Serial.print("gateway IP Address: ");
	Serial.println(eth.gatewayIP());

	Serial.print("subnet IP mask: ");
	Serial.println(eth.subnetMask());

	Serial.print("DNS server: ");
	IPAddress dns = eth.dnsServerIP();
	if (dns == INADDR_NONE)
	{
		Serial.println("not set");
	}
	else
	{
		Serial.println(dns);
	}
}

void printEthernetStatus()
{
	byte mac[6];
	Ethernet1.MACAddress(mac);
	Serial.print("MAC: ");
	Serial.println(MacAddress(mac));
	if (mac[0] & 1)
	{ // unicast bit is set
		Serial.println("\t is the ordering of the MAC address bytes reversed?");
	}

	Serial.print("IP Address: ");
	Serial.println(Ethernet1.localIP());

	Serial.print("gateway IP Address: ");
	Serial.println(Ethernet1.gatewayIP());

	Serial.print("subnet IP mask: ");
	Serial.println(Ethernet1.subnetMask());

	Serial.print("DNS server: ");
	IPAddress dns = Ethernet1.dnsServerIP();
	if (dns == INADDR_NONE)
	{
		Serial.println("not set");
	}
	else
	{
		Serial.println(dns);
	}
}

bool testNetworkConnection(const char *host)
{
	bool isConnected = false;

	if (statusNetwork < NETWORK_CONNECTED_IP)
	{
		DEBUG_PRINTLN(F("No network connection, skipping test."));
		return false;
	}

	// DEBUG_PRINT(F("Testing network connection to: "));
	// DEBUG_PRINTLN(host);

	if (Ping.ping(host))
	{
		// DEBUG_PRINTLN(F("Ping successful!"));
		isConnected = true;
	}
	else
	{
		DEBUG_PRINTLN(F("Ping failed!"));
		isConnected = false;
	}

	return isConnected;
}

void mqttServiceLoop()
{
	if (statusNetwork == NETWORK_CONNECTED_INTERNET)
	{
		if (hasInitializedMQTT == false)
		{
			DEBUG_PRINTLN("Initializing MQTT Service...");
			mqttServiceInit();
			hasInitializedMQTT = true;
		}

		if (!mqttClientNovaLogic.connected())
		{
			DEBUG_PRINTLN("Connecting to NovaLogic MQTT broker...");
			mqttClientNovaLogic.begin();
			vTaskDelay(1000); // Wait for 1 second to allow connection to establish
		}

		mqttClientNovaLogic.loop();
	}
}

// MQTT Service Init ----------------------------------------------------------------------

void mqttServiceInit()
{
	initNovaLogicServices();
}

// OTA Related Functions ------------------------------------------------------------------

void initNovaLogicServices()
{
	char mqttTopic[128];

	mqttClientNovaLogic.connected_callback = []
	{
		DEBUG_PRINTLN("MQTT connected to NovaLogic broker!");
		char mqttTopic[128];
		snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/connected", MQTT_DEVICE_ID);
		mqttClientNovaLogic.publish(mqttTopic, "true", 1, true);
		snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/version", MQTT_DEVICE_ID);
		mqttClientNovaLogic.publish(mqttTopic, FIRMWARE_VERSION, 1);
	};

	mqttClientNovaLogic.disconnected_callback = []
	{
		DEBUG_PRINTLN("MQTT disconnected from NovaLogic broker!");
	};

	snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/messages", MQTT_DEVICE_ID);
	DEBUG_PRINTLN("Subscribing to topic: " + String(mqttTopic));
	mqttClientNovaLogic.subscribe(mqttTopic, [](const char *topic, const char *payload)
								  { parseMQTTMessage(topic, payload); });

	snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/ota/version", MQTT_DEVICE_ID);
	DEBUG_PRINTLN("Subscribing to topic: " + String(mqttTopic));
	mqttClientNovaLogic.subscribe(mqttTopic, [](const char *topic, const char *payload)
								  { DEBUG_PRINTLN("MQTT message received on topic: " + String(topic) + " with payload: " + String(payload));
		if(checkOTAVersionNewer((const char *)payload))
			requestOTAUpdate(); });

	snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/ota/md5", MQTT_DEVICE_ID);
	DEBUG_PRINTLN("Subscribing to topic: " + String(mqttTopic));
	mqttClientNovaLogic.subscribe(mqttTopic, [](const char *topic, const char *payload)
								  { DEBUG_PRINTLN("MQTT message received on topic: " + String(topic) + " with payload: " + String(payload));
		Update.setMD5((const char *)payload); });

	snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/ota/update", MQTT_DEVICE_ID);
	DEBUG_PRINTLN("Subscribing to topic: " + String(mqttTopic));
	mqttClientNovaLogic.subscribe(mqttTopic, [](const char *topic, PicoMQTT::IncomingPacket &packets)
								  { DEBUG_PRINTLN("MQTT message received on topic: " + String(topic));
		handleOTAUpdate(packets); });

	snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/connected", MQTT_DEVICE_ID);
	mqttClientNovaLogic.will.topic = String(mqttTopic);
	mqttClientNovaLogic.will.payload = "false";
	mqttClientNovaLogic.will.qos = 1;
	mqttClientNovaLogic.will.retain = true;

	mqttClientNovaLogic.begin();
}

void sendDeviceModel()
{
	DEBUG_PRINTLN("Sending device model to MQTT broker...");
	char mqttTopic[128];
	snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/model", MQTT_DEVICE_ID);
	mqttClientNovaLogic.publish(mqttTopic, DEVICE_MODEL, 1);
}

void sendFirmwareVersion()
{
	char mqttTopic[128];
	snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/version", MQTT_DEVICE_ID);
	mqttClientNovaLogic.publish(mqttTopic, FIRMWARE_VERSION, 1);
}

void checkOTAVersion()
{
	char mqttTopic[128];

	snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/messages", MQTT_DEVICE_ID);
	DEBUG_PRINTLN("Publlishing to topic: " + String(mqttTopic));
	mqttClientNovaLogic.publish(mqttTopic, MQTT_OTA_CMD_VERSION);
}

bool checkOTAVersionNewer(const char *version)
{
	bool isNewer = false;

	String currentVersion = FIRMWARE_VERSION; // In format X.YY.ZZ
	String newVersion = String(version);	  // In format X.YY.ZZ

	DEBUG_PRINTLN("Current Firmware Version: " + currentVersion);
	DEBUG_PRINTLN("Latest OTA version: " + newVersion);

	DEBUG_PRINTLN("Comparing versions...");
	int currentMajor = currentVersion.substring(0, currentVersion.indexOf('.')).toInt();
	int currentMinor = currentVersion.substring(currentVersion.indexOf('.') + 1, currentVersion.lastIndexOf('.')).toInt();
	int currentPatch = currentVersion.substring(currentVersion.lastIndexOf('.') + 1).toInt();
	int newMajor = newVersion.substring(0, newVersion.indexOf('.')).toInt();
	int newMinor = newVersion.substring(newVersion.indexOf('.') + 1, newVersion.lastIndexOf('.')).toInt();
	int newPatch = newVersion.substring(newVersion.lastIndexOf('.') + 1).toInt();

	if (newMajor > currentMajor)
	{
		isNewer = true;
	}
	else if (newMajor == currentMajor)
	{
		if (newMinor > currentMinor)
		{
			isNewer = true;
		}
		else if (newMinor == currentMinor)
		{
			if (newPatch > currentPatch)
			{
				isNewer = true;
			}
		}
	}

	if (isNewer)
		DEBUG_PRINTLN("OTA version is newer than current version.");
	else
		DEBUG_PRINTLN("OTA version is not newer than current version.");

	return isNewer;
}

void requestOTAUpdate()
{
	char mqttTopic[128];

	snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/messages", MQTT_DEVICE_ID);
	DEBUG_PRINTLN("Publlishing to topic: " + String(mqttTopic));
	mqttClientNovaLogic.publish(mqttTopic, MQTT_OTA_CMD_UPDATE);
}

void handleOTAUpdate(PicoMQTT::IncomingPacket &packets)
{
	DEBUG_PRINTLN("Performing OTA update...");

	size_t payload_size = packets.get_remaining_size();
	DEBUG_PRINTLN("OTA Update Size: " + String(payload_size));

	statusDevice = DEVICE_UPDATING;

	sprintf(displayViewModel.status, "Updating...");

	if (!Update.begin(packets.available()))
	{
		publishOTAStatus("Error: Not enough space for update");
		sprintf(displayViewModel.status, "Error Space...");
		statusDevice = DEVICE_UPDATE_FAILED;
		delay(2500);
		statusDevice = DEVICE_STARTED;
		return;
	}

	publishOTAStatus("Beginning OTA update, this may take a minute...");
	size_t written = Update.write(packets);
	if (written == payload_size)
	{
		DEBUG_PRINTLN("Written : " + String(written) + " successfully");
	}
	else
	{
		DEBUG_PRINTLN("Written only : " + String(written) + "/" + String(payload_size));
	}

	if (Update.size() == payload_size)
	{
		DEBUG_PRINTLN("Update size matches payload size: " + String(payload_size));
		publishOTAStatus("OTA update received, preparing to install...");
	}
	else
	{
		DEBUG_PRINTLN("Update size does not match payload size: " + String(payload_size));
		publishOTAStatus("OTA update receive failed...");
	}

	if (Update.hasError())
	{
		DEBUG_PRINTLN("Update error: " + String(Update.getError()));
		publishOTAStatus("Error: Update failed!");
		sprintf(displayViewModel.status, "Update error!");
		statusDevice = DEVICE_UPDATE_FAILED;
		delay(2500);
		statusDevice = DEVICE_STARTED;
		return;
	}

	if (Update.end(true))
	{
		if (Update.isFinished())
		{
			DEBUG_PRINTLN("Update successfully completed. Rebooting...");
			publishOTAStatus("Update successfully completed.");
			sprintf(displayViewModel.status, "Update complete");
			Ethernet1.end();
			ethernetDriver1.end();
			statusDevice = DEVICE_STARTED;
			delay(2500);
			sprintf(displayViewModel.status, "Restarting...");
			delay(2500);
			// Restart the Ethernet port and ESP
			ESP.restart();
		}
		else
		{
			DEBUG_PRINTLN("Update not finished.");
			publishOTAStatus("Update not finished.");
			sprintf(displayViewModel.status, "Update failed");
			statusDevice = DEVICE_UPDATE_FAILED;
			delay(2500);
			statusDevice = DEVICE_STARTED;
		}
	}
	else
	{
		DEBUG_PRINTLN("Update error!");
		publishOTAStatus("Update error!");
		sprintf(displayViewModel.status, "Update error!");
		statusDevice = DEVICE_UPDATE_FAILED;
		delay(2500);
		statusDevice = DEVICE_STARTED;
	}
	statusDevice = DEVICE_STARTED;
}

void publishOTAStatus(const char *message)
{
	if (mqttClientNovaLogic.connected())
	{
		char mqttTopic[128];

		snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/ota/status", MQTT_DEVICE_ID);
		DEBUG_PRINTLN("Publlishing to topic: " + String(mqttTopic));
	}
	DEBUG_PRINTLN(message);
}

// Miscellaneous Functions ----------------------------------------------------------------

void keepAliveNovaLogic()
{
	// DEBUG_PRINTLN("Sending keep-alive message...");
	if (!mqttClientNovaLogic.connected())
		mqttClientNovaLogic.begin();

	char mqttTopic[128];
	snprintf(mqttTopic, sizeof(mqttTopic), "devices/%s/connected", MQTT_DEVICE_ID);
	mqttClientNovaLogic.publish(mqttTopic, "true", 1, true);
}

void keepAliveRouterUDP()
{
	if (millis() - lastKeepAlive > keepAliveInterval)
	{
		if (statusNetwork >= NETWORK_CONNECTED_IP)
		{
			// Send UDP keepalive message to router
			udp.beginPacket(routerIP, 8888);
			udp.write((uint8_t *)"keepalive", 9);
			udp.endPacket();
			// DEBUG_PRINTLN("UDP keepalive sent to router: " + routerIP.toString());
		}
		else
		{
			DEBUG_PRINTLN("Ethernet 1 is not connected, skipping UDP keepalive.");
		}
		lastKeepAlive = millis();
	}
}

void checkInternetConnectivity()
{
	if (millis() - lastConnectivityPing > connectivityPingInterval)
	{
		if (statusNetwork >= NETWORK_CONNECTED_IP)
		{
			if (testNetworkConnection("www.novalogic.io"))
				setNeworkStatus(NETWORK_CONNECTED_INTERNET);
			else
			{
				setNeworkStatus(NETWORK_CONNECTED_IP);
				startPingChecker();
			}
		}
		lastConnectivityPing = millis();
	}
}

void setNeworkStatus(NetworkStatus status)
{
	statusNetwork = status;
}

void restartEthernetPort()
{
	if (statusNetwork >= NETWORK_CONNECTED_IP)
	{
		Ethernet1.end();
		ethernetDriver1.end();
		vTaskDelay(PORT_RECYCLE_INTERVAL / portTICK_PERIOD_MS);
		ethernetDriver1.begin();
		Ethernet1.init(ethernetDriver1);
		Ethernet1.begin(5000);
	}
}