#include "modbusMonitor.h"
#include <HardwareSerial.h> // ************* GM INSERTED *****************

extern StatusViewModel displayViewModel;
extern SemaphoreHandle_t displayModelMutex;
extern String getFormattedTime();

extern SemaphoreHandle_t statusModbusMutex;

extern ModbusStatus statusModbus;
extern bool enableModbusDebug;
extern bool enableModbusLoggingToSD;
extern bool enableModbusLoggingToMQTT;
extern bool enableModbusLoggingToSerial;
extern void logByteToSD(byte byteReceived);

extern QueueHandle_t modbusQueueHandle;
extern QueueHandle_t mqttQueueHandle;
extern QueueHandle_t mqttModbusQueueHandle;

HardwareSerial modbusSerial(2);
long lastActivity = 0;
long lastValidActivity = 0;

// Pages to poll: Page 4, 5, 7 - each page is on a 256 byte block
ModbusTarget pollingPageTargets[] = {
  {1024, 65, "Page 4 (65 registers)"}, // Page 4 - total received bytes inc CRC's = 135, each of the 65 registers are  16 bits, plus 5 bytes for Modbus protocol - Poll is: 0A 03 04 00 00 41 85 B1  including crc
  {1290, 2,  "Page 5 (2 registers)"}, // Page 5 - Poll is: 0A 03 05 0A 00 02 E5 BE    including crc - this is fuel consumption 32 bits 2 registers
  {1792, 22, "Page 7 (22 registers)"} // Page 7 - Poll is: 0A 03 07 00 00 16 C4 0B    including crc,  engine run time is reg 6 and 7
};

long lastPollingActivity = 0;
int lastPollingPageIndex = 0;

ModbusState monitorState;
GenSetData_t genSetData;

uint8_t dataBuffer[262]; // Max Modbus frame size + CRC
int bufferIndex = 0;
int bufferIndexMax = sizeof(dataBuffer) - 1; // Max buffer size - 1 for null terminator
int pageMatch = 0;
int pageBytesExpected = 0;
int pageBytesReceived = 0;
const byte *pagePatterns[pagePatternsCount] = {pagePattern4, pagePattern5, pagePattern7};
long lastValidFrameTime = 0;

void TaskModBusMonitor(void *pvParameters)
{
	byte dataReceived;
	long timeSinceLastPoll = 0;

	initModBusSerial();

	for (;;)
	{
		bool bytesProcessed = false;

		timeSinceLastPoll = millis() - lastPollingActivity;
		if (timeSinceLastPoll > MODBUS_POLLING_INTERVAL)
			sendNextPollingCommand();

		while (modbusSerial.available())
		{
			dataReceived = modbusSerial.read();
			bytesProcessed = true;

			if (modbusQueueHandle != NULL && uxQueueSpacesAvailable(modbusQueueHandle) > 0)
			{
				int ret = xQueueSend(modbusQueueHandle, (void *)&dataReceived, 0);
				if (ret == pdTRUE)
				{
					if (enableModbusDebug && enableModbusLoggingToSerial)
						printByte(dataReceived);
				}
				else if (ret == errQUEUE_FULL)
				{
					Serial.println("The `TaskModBusMonitor` was unable to send data into the modbusQueueHandle");
				}
			}
		}

		if (bytesProcessed)
			updateModbusActivityMonitor(true);
		else
			updateModbusActivityMonitor(false);

		vTaskDelay(1 / portTICK_PERIOD_MS);
	}
}

void TaskModBusProcessor(void *pvParameters)
{
	byte dataReceived = false;

	for (;;)
	{
		if (modbusQueueHandle != NULL)
		{
			int ret = xQueueReceive(modbusQueueHandle, &dataReceived, portMAX_DELAY);
			if (ret == pdPASS)
			{
				if (enableModbusLoggingToSD)
					logByteToSD(dataReceived);

				if (enableModbusLoggingToMQTT)
					logByteToMQTT(dataReceived);

				processModbusSerial(dataReceived);
			}
			else if (ret == pdFALSE)
			{
				Serial.println("The `TaskModBusProcessor` was unable to receive data from the Queue");
			}
		}
		vTaskDelay(1 / portTICK_PERIOD_MS);
	}
}

bool initModBusSerial()
{
	DEBUG_PRINTLN(F("Initializing ModBus Serial Link..."));

#if TARGET_DEVICE==2
	modbusSerial.begin(9600, SERIAL_8N1, BOARD_PIN_RS485_RX, BOARD_PIN_RS485_TX);
#else
	modbusSerial.begin(115200, SERIAL_8N1, BOARD_PIN_RS485_RX, BOARD_PIN_RS485_TX);
#endif

	// Initialize the Modbus serial port to start in receive mode
	pinMode(BOARD_PIN_RS485_DE_RE, OUTPUT);
	digitalWrite(BOARD_PIN_RS485_DE_RE, LOW);
	pinMode(BOARD_PIN_RS485_RX_EN, OUTPUT);
	digitalWrite(BOARD_PIN_RS485_RX_EN, LOW);

	if (!modbusSerial)
	{
		DEBUG_PRINTLN(F("Failed to open Modbus serial port!"));
		return false;
	}
	else
	{
		DEBUG_PRINTLN(F("Modbus serial port opened successfully!"));
		modbusSerial.flush();
		vTaskDelay(500 / portTICK_PERIOD_MS);
		setModbusStatus(MODBUS_NOT_ACTIVE);
		return true;
	}
}

void updateModbusActivityMonitor(bool isModbusActive)
{
	long currentTime = millis();

	if (isModbusActive)
	{
		lastActivity = currentTime;

		if (currentTime - lastValidFrameTime > MODBUS_VALIDITY_INTERVAL)
		{
			setModbusStatus(MODBUS_ACTIVE);
		}
	}
	else
	{
		if (currentTime - lastActivity > MODBUS_ACTIVITY_INTERVAL)
		{
			setModbusStatus(MODBUS_NOT_ACTIVE);
		}
	}
}

void sendNextPollingCommand()
{
	if (enableModbusDebug)
		DEBUG_PRINTLN(F("Sending Next Polling Command..."));

	// Reset the monitor state before sending a new command
	resetMonitorState();

	// Send the polling command
	ModbusTarget &target = pollingPageTargets[lastPollingPageIndex];
	lastPollingPageIndex = (lastPollingPageIndex + 1) % (sizeof(pollingPageTargets) / sizeof(ModbusTarget));

	// Poll Build request, keep in mind that a Page = 256 registers, so Page 4 starts at 1024
	uint8_t request[8];
	request[0] = MODBUS_SLAVE_ID;
	request[1] = MODBUS_FUNCTION_CODE;
	request[2] = highByte(target.startReg);
	request[3] = lowByte(target.startReg);
	request[4] = 0x00;
	request[5] = target.regCount;
	uint16_t crc = modbusCRC(request, 6);
	request[6] = lowByte(crc);
	request[7] = highByte(crc);

	if (enableModbusDebug && enableModbusLoggingToSerial)
	{
		DEBUG_PRINT(F("Sending request for "));
		DEBUG_PRINTLN(target.label);
		for (int i = 0; i < 8; i++)
			printByte(request[i]);
		DEBUG_PRINTLN();
	}

	// Transmit
	digitalWrite(BOARD_PIN_RS485_RX_EN, LOW); // This disables the RX byte, but actually it can stay on
	digitalWrite(BOARD_PIN_RS485_DE_RE, HIGH); // This enables RS485 TX, disable after sending the poll
	delay(2);
	modbusSerial.write(request, 8); // send the 8 bytes
	modbusSerial.flush();			// wait till it is zero
	delay(2);
	digitalWrite(BOARD_PIN_RS485_DE_RE, LOW); // disable TX for RS485
	digitalWrite(BOARD_PIN_RS485_RX_EN, LOW); // enable RX receive

	lastPollingActivity = millis();
}

void processModbusSerial(byte dataReceived)
{
	switch (monitorState)
	{
	case MODBUS_STATE_START:
		if (dataReceived == 0x0a) // ******* GM changed from 0x04 to 0x0a to match GM modified Modbus Page 4
		{
			if (enableModbusDebug)
				DEBUG_PRINTLN("Received Start Byte: 0x0A");
			dataBuffer[bufferIndex++] = dataReceived;
			monitorState = MODBUS_STATE_HEADER;
		}
		// setModbusStatus(MODBUS_ACTIVE);
		break;

	case MODBUS_STATE_HEADER:
		if (bufferIndex < bufferIndexMax)
		{
			dataBuffer[bufferIndex++] = dataReceived;
			if (bufferIndex == (pagePatternsLength)) // GM
			{
				pageMatch = matchPageHeader();
				if (pageMatch == 0)
				{
					if (enableModbusDebug)
						DEBUG_PRINTLN(" No page match found, resetting state.");
					resetMonitorState();
				}
				else
				{
					if (enableModbusDebug)
						DEBUG_PRINTF("✅  Page Match Found: %02d\n", pageMatch);
					pageBytesExpected = getPageByteSize(pageMatch);
					bufferIndex = 0;
					monitorState = MODBUS_STATE_DATA;
				}
			}
		}
		// setModbusStatus(MODBUS_ACTIVE);
		break;

	case MODBUS_STATE_DATA:
		if (bufferIndex < bufferIndexMax)
		{
			dataBuffer[bufferIndex++] = dataReceived;
			if (bufferIndex == pageBytesExpected)
			{
				if (validateFrame())
				{
					if (enableModbusDebug)
						DEBUG_PRINTLN("✅  Valid Frame Received");

					setModbusStatus(MODBUS_ACTIVE_VALID_DATA);
					parseFrameData();
				}
				else
				{
					if (enableModbusDebug)
						DEBUG_PRINTLN("❌  Invalid Frame");
					setModbusStatus(MODBUS_ACTIVE_INVALID_DATA);
				}
				resetMonitorState();
			}
			else if (bufferIndex > pageBytesExpected)
			{
				if (enableModbusDebug)
					DEBUG_PRINTLN("❌  Buffer overflow, resetting state");
				setModbusStatus(MODBUS_ACTIVE_INVALID_DATA);
				resetMonitorState();
			}
		}
		else
		{
			if (enableModbusDebug)
				DEBUG_PRINTLN("❌  Buffer overflow, resetting state");
			setModbusStatus(MODBUS_ACTIVE_INVALID_DATA);
			resetMonitorState();
		}
		break;
	}
}

void resetMonitorState()
{
	monitorState = MODBUS_STATE_START;
	bufferIndex = 0;
	pageMatch = 0;
	pageBytesExpected = 0;
}

int matchPageHeader()
{
	int matchID = 0;

	if (enableModbusDebug)
		DEBUG_PRINT(F("Matching Page Header..."));

	for (int p = 0; p < pagePatternsCount; p++)
	{
		bool match = true;
		for (int b = 0; b < pagePatternsLength; b++)
		{
			if (dataBuffer[b] != pagePatterns[p][b])
			{
				match = false;
				break;
			}
		}
		if (match)
		{
			switch (p)
			{
			case 0:
				matchID = 4;
				break;
			case 1:
				matchID = 5;
				break;
			case 2:
				matchID = 7;
				break;
			}
		}
	}
	return matchID;
}

int getPageByteSize(int matchID)
{
	int pageBytes = 0;
	switch (matchID)
	{
	case 4:
		pageBytes = PAGE_LENGTH_4;
		break;
	case 5:
		pageBytes = PAGE_LENGTH_5;
		break;
	case 7:
		pageBytes = PAGE_LENGTH_7;
		break;
	default:
		pageBytes = 0; // Invalid match ID
		break;
	}
	return pageBytes;
}

bool validateFrame()
{
	// Check Slave ID
	if (dataBuffer[0] != MODBUS_SLAVE_ID)
	{
		if (enableModbusDebug)
		{
			Serial.print("❌ Invalid Slave ID (Expected: ");
			Serial.print(MODBUS_SLAVE_ID, HEX);
			Serial.print(", Received: ");
			Serial.print(dataBuffer[0], HEX);
			Serial.println(")");
		}
		return false;
	}

	// Check Function Code
	if (dataBuffer[1] != MODBUS_FUNCTION_CODE)
	{
		if (enableModbusDebug)
			Serial.println("❌ Invalid Function Code");
		return false;
	}

	// Check Byte Count
	if (pageMatch == 4)
	{
		if (dataBuffer[2] != PAGE_BYTE_COUNT_4)
		{
			if (enableModbusDebug)
			{
				Serial.println("❌ Invalid Byte Count for Page 4");
				Serial.print("Expected: ");
				Serial.print(PAGE_BYTE_COUNT_4, HEX);
				Serial.print(", Received: ");
				Serial.print(dataBuffer[2], HEX);
				Serial.println(")");
			}
			return false;
		}
	}
	else if (pageMatch == 5)
	{
		if (dataBuffer[2] != PAGE_BYTE_COUNT_5)
		{
			if (enableModbusDebug)
			{
				Serial.println("❌ Invalid Byte Count for Page 5");
				Serial.print("Expected: ");
				Serial.print(PAGE_BYTE_COUNT_5, HEX);
				Serial.print(", Received: ");
				Serial.print(dataBuffer[2], HEX);
				Serial.println(")");
			}
			return false;
		}
	}
	else if (pageMatch == 7)
	{
		if (dataBuffer[2] != PAGE_BYTE_COUNT_7)
		{
			if (enableModbusDebug)
			{
				Serial.println("❌ Invalid Byte Count for Page 7");
				Serial.print("Expected: ");
				Serial.print(PAGE_BYTE_COUNT_7, HEX);
				Serial.print(", Received: ");
				Serial.print(dataBuffer[2], HEX);
				Serial.println(")");
			}
			return false;
		}
	}

	// Check CRC
	uint8_t packetLength = bufferIndex;
	if (validateCRC(dataBuffer, packetLength))
	{
		lastValidFrameTime = millis();
		if (enableModbusDebug)
		{
			Serial.print("✅ Valid Modbus Packet, Count=");
			Serial.println(dataBuffer[2]);
		}
		return true;
	}
	else
	{
		if (enableModbusDebug)
			Serial.println("❌ CRC Error");
		return false;
	}
}

void parseFrameData()
{
	switch (dataBuffer[2])
	{
	case PAGE_BYTE_COUNT_4:
		parseRegistersPage4();
		break;
	case PAGE_BYTE_COUNT_5:
		parseRegistersPage5();
		break;
	case PAGE_BYTE_COUNT_7:
		parseRegistersPage7();
		break;
	}

	displayViewModel.genStatus = &genSetData.generatorRunning;
	displayViewModel.batVoltage = &genSetData.engineBatteryV;
}

void parseRegistersPage4()
{
	int startOffset = frameDataStartOffset;
	long long32bit = 0;

	// Oil Pressure, typical 275 to 550 kPa
	bufferIndex = startOffset + (oilPressure_offset * 2);
	genSetData.oilPressure = ((dataBuffer[bufferIndex] << 8) | (dataBuffer[bufferIndex + 1]));

	// Fuel level
	bufferIndex = startOffset + (fuelLevel_offset * 2);
	genSetData.fuelLevel = ((dataBuffer[bufferIndex] << 8) | (dataBuffer[bufferIndex + 1]));

	// Battery Voltage
	bufferIndex = startOffset + (engineBatteryV_offset * 2);
	genSetData.engineBatteryV = ((dataBuffer[bufferIndex] << 8) | (dataBuffer[bufferIndex + 1]));
	genSetData.engineBatteryV = genSetData.engineBatteryV / 10.0;

	// Engine Speed
	bufferIndex = startOffset + (engineRPM_offset * 2);
	genSetData.engineSpeed = ((dataBuffer[bufferIndex] << 8) | (dataBuffer[bufferIndex + 1]));

	// Genset Mains V L1
	bufferIndex = startOffset + (voltageL1_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.voltageL1 = long32bit / 10.0;

	// GenSet Mains V L2
	bufferIndex = startOffset + (voltageL2_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.voltageL2 = long32bit / 10.0;

	// GenSet Mains V L3
	bufferIndex = startOffset + (voltageL3_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.voltageL3 = long32bit / 10.0;

	// GenSet Current L1
	bufferIndex = startOffset + (currentL1_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.currentL1 = long32bit / 10.0;

	// GenSet Current L2
	bufferIndex = startOffset + (currentL2_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.currentL2 = long32bit / 10.0;

	// GenSet Current L3
	bufferIndex = startOffset + (currentL3_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.currentL3 = long32bit / 10.0;

	// GenSet Output W L1
	bufferIndex = startOffset + (generatorOutputL1_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.generatorOutputL1 = long32bit;
	// genSetData.generatorOutputL1 = long32bit / 10.0;

	// GenSet Output W L2
	bufferIndex = startOffset + (generatorOutputL2_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.generatorOutputL2 = long32bit;
	// genSetData.generatorOutputL2 = long32bit / 10.0;

	// GenSet Output W L3
	bufferIndex = startOffset + (generatorOutputL3_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.generatorOutputL3 = long32bit;
	// genSetData.generatorOutputL3 = long32bit / 10.0;

	// AC Grid Mains V L1
	bufferIndex = startOffset + (gridVoltageL1_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.gridVoltageL1 = long32bit / 10.0;

	// AC Grid Mains V L2
	bufferIndex = startOffset + (gridVoltageL2_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.gridVoltageL2 = long32bit / 10.0;

	// AC Grid Mains V L3
	bufferIndex = startOffset + (gridVoltageL3_offset * 2);
	long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	genSetData.gridVoltageL3 = long32bit / 10.0;

	// Now SANITIZE  data - determine if generator is running, if not, sanitize relevant fields with zero values
	// base it simply on generator voltage L1, if < 100 then it is not running
	if (genSetData.voltageL1 < 100)
	{
		genSetData.generatorRunning = false;
		genSetData.generatorMode = "Stopped";
		genSetData.generatorFrequency = 0.0;
		genSetData.powerFactor = 0.0;
		genSetData.oilPressure = 0.0;
		genSetData.coolantTemperature = 0.0;
		genSetData.fuelConsumption = 0.0;
		genSetData.engineSpeed = 0.0;
		genSetData.generatorOverload = false;
	}
	else
	{
		genSetData.generatorRunning = true;
		genSetData.generatorMode = "Running";
	}
}

void parseRegistersPage5()
{
	bufferIndex = (fuelConsumption_offset * 2) + frameDataStartOffset; // 2 bytes per register
	long long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	float scaled = round((long32bit / 10.0) * 10) / 10.0;
	genSetData.fuelConsumption = scaled;

	// Sanitize data if not generator not running
	if (genSetData.voltageL1 < 100)
	{
		genSetData.fuelConsumption = 0;
	}
}

void parseRegistersPage7()
{
	bufferIndex = (timeElapsed_offset * 2) + frameDataStartOffset; // 2 bytes per register
	long long32bit = convertTo32bits(dataBuffer[bufferIndex], dataBuffer[bufferIndex + 1], dataBuffer[bufferIndex + 2], dataBuffer[bufferIndex + 3]);
	long elapsedTime = long32bit / 3600; // Convert to hours
	genSetData.generatorRunningTime = elapsedTime;
}

bool validateCRC(uint8_t *data, uint8_t length)
{
	uint16_t receivedCRC = (data[length - 2]) | (data[length - 1] << 8); // Correct byte order
	uint16_t calculatedCRC = calculateCRC(data, length - 2);			 // Compute CRC for data (excluding CRC bytes)

	if (enableModbusDebug)
	{
		Serial.print("Received CRC: ");
		Serial.print(receivedCRC, HEX);
		Serial.print(" | Calculated CRC: ");
		Serial.println(calculatedCRC, HEX);
	}

	return receivedCRC == calculatedCRC;
}

uint16_t calculateCRC(uint8_t *data, uint8_t length)
{
	uint16_t crc = 0xFFFF;
	for (uint8_t i = 0; i < length; i++)
	{
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; j++)
		{
			if (crc & 1)
			{
				crc = (crc >> 1) ^ 0xA001;
			}
			else
			{
				crc >>= 1;
			}
		}
	}
	return crc;
}

long convertTo32bits(byte b1, byte b2, byte b3, byte b4)
{
	long long32bit = 0;
	long32bit = ((uint32_t)b1 << 24) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 8) | ((uint32_t)b4); // Convert the 4 bytes into a 32-bit integer using bit shifts
	return long32bit;
}

void printByte(byte byteReceived)
{
	if (byteReceived <= 0x0F)
	{
		Serial.print("0");
	}
	Serial.print(byteReceived, HEX);
	Serial.print(" ");
}

void logByteToMQTT(byte byteReceived)
{
	if (mqttModbusQueueHandle != NULL && uxQueueSpacesAvailable(mqttModbusQueueHandle) > 0)
	{
		int ret = xQueueSend(mqttModbusQueueHandle, (void *)&byteReceived, 0);
		if (ret == errQUEUE_FULL)
		{
			Serial.println("The `TaskModBusMonitor` was unable to send data into the mqttModbusQueueHandle");
		}
	}
}

String getDataRecord()
{
	JsonDocument dataEvent;
	dataEvent["variable"] = "event";
	dataEvent["value"] = "dataUpdate";

	JsonObject dataRecord = dataEvent["metadata"].to<JsonObject>();
	dataRecord["timeStamp"] = getFormattedTime();
	dataRecord["generatorRunning"] = genSetData.generatorRunning;
	dataRecord["generatorMode"] = genSetData.generatorMode;
	dataRecord["generatorRunningTime"] = genSetData.generatorRunningTime;
	dataRecord["gridVoltageL1"] = genSetData.gridVoltageL1;
	dataRecord["gridVoltageL2"] = genSetData.gridVoltageL2;
	dataRecord["gridVoltageL3"] = genSetData.gridVoltageL3;
	dataRecord["voltageL1"] = genSetData.voltageL1;
	dataRecord["voltageL2"] = genSetData.voltageL2;
	dataRecord["voltageL3"] = genSetData.voltageL3;
	dataRecord["voltageL1_L2"] = genSetData.voltageL1_L2;
	dataRecord["voltageL2_L3"] = genSetData.voltageL2_L3;
	dataRecord["voltageL3_L1"] = genSetData.voltageL3_L1;
	dataRecord["currentL1"] = genSetData.currentL1;
	dataRecord["currentL2"] = genSetData.currentL2;
	dataRecord["currentL3"] = genSetData.currentL3;
	dataRecord["generatorOutputL1"] = genSetData.generatorOutputL1;
	dataRecord["generatorOutputL2"] = genSetData.generatorOutputL2;
	dataRecord["generatorOutputL3"] = genSetData.generatorOutputL3;
	dataRecord["generatorOutputTotal"] = genSetData.generatorOutputTotal;
	dataRecord["generatorFrequency"] = genSetData.generatorFrequency;
	dataRecord["powerFactor"] = 0;
	dataRecord["oilPressure"] = genSetData.oilPressure;
	dataRecord["coolantTemperature"] = genSetData.coolantTemperature;
	dataRecord["fuelLevel"] = genSetData.fuelLevel;
	dataRecord["fuelConsumption"] = genSetData.fuelConsumption;
	dataRecord["engineBatteryV"] = genSetData.engineBatteryV;
	dataRecord["engineSpeed"] = genSetData.engineSpeed;
	dataRecord["maintenanceDue"] = false;
	dataRecord["batteryLow"] = false;
	dataRecord["generatorOverload"] = false;

	String sOutput;
	serializeJsonPretty(dataEvent, sOutput);
	return sOutput;
}

void setModbusStatus(ModbusStatus status)
{
	// if (xSemaphoreTake(statusModbusMutex, 10) == pdTRUE)
	// {
	statusModbus = status;
	// 	xSemaphoreGive(statusModbusMutex);
	// }
}

uint16_t modbusCRC(uint8_t *data, uint8_t len)
{
	uint16_t crc = 0xFFFF;
	for (uint8_t i = 0; i < len; i++)
	{
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; j++)
		{
			crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
		}
	}
	return crc; // LSB first, MSB second
}
