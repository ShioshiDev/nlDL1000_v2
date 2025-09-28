#pragma once
#ifndef __MODBUSMONITOR_H__
#define __MODBUSMONITOR_H__

#include <Arduino.h>
#include <freertos/semphr.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

#include "definitions.h"
#include "networkManager.h"

#define PAGE_LENGTH_4					          0x87 // Page 4 length=  5 modbus frame protocol  bytes + 0x43 = 67 registers byte count = 0x87  in total
#define PAGE_LENGTH_5					          0x09 // Page 5 length 5  modbus frame bytes + 2 registers = 0x2C + 5
#define PAGE_LENGTH_7 					        0x31 // Page 7 5 modbus frame bytes + 2 registers ( 2 x 2)
#define PAGE_BYTE_COUNT_4 				      0x82 // Valid Byte Counts, Page 4, basically 65 registers
#define PAGE_BYTE_COUNT_5 				      0x04 // Valid Byte Counts, Page 5, basically 2 registers
#define PAGE_BYTE_COUNT_7 				      0x2C // Valid Byte Counts, Page 7, basically 
#define MODBUS_SLAVE_ID 				        0x0A // Slave ID we are looking for - DSE default is 10, they set to 4
#define MODBUS_FUNCTION_CODE 			      0x03 // Function Code for Read Holding Registers

const int frameDataStartOffset =        3; // Start of data in the frame

const int oilPressure_offset =          0;
const int coolantTemp_offset =          1;
const int oilTemp_offset =              2;
const int fuelLevel_offset =            3;
const int engineBatteryV_offset =       5;
const int engineRPM_offset =            6;
const int genFreq_offset =              7;
const int voltageL1_offset =            8;
const int voltageL2_offset =            10;
const int voltageL3_offset =            12;
const int currentL1_offset =            20;
const int currentL2_offset =            22;
const int currentL3_offset =            24;
const int generatorOutputL1_offset =    28;
const int generatorOutputL2_offset =    30;
const int generatorOutputL3_offset =    32;
const int gridVoltageL1_offset =        36;
const int gridVoltageL2_offset =        38;
const int gridVoltageL3_offset =        40;

const int fuelConsumption_offset =      10;
const int timeElapsed_offset =          6;

const int pagePatternsLength = 8;
const int pagePatternsCount = 3;
const byte pagePattern4[pagePatternsLength] = {0x0a, 0x03, 0x04, 0x00, 0x00, 0x41, 0x85, 0xb1}; // GM modified Modbus Page 4 65 registers from 1024
const byte pagePattern5[pagePatternsLength] = {0x0a, 0x03, 0x05, 0x0A, 0x00, 0x02, 0xe5, 0xbe}; // Modbus Page 5 2 registers from 1290
const byte pagePattern7[pagePatternsLength] = {0x0a, 0x03, 0x07, 0x00, 0x00, 0x16, 0xC4, 0x0b}; // Modbus Page 7 22 registers from 1792

struct ModbusTarget { 
  uint16_t startReg;
  uint8_t regCount;
  const char* label;
};

typedef enum {
    MODBUS_STATE_START,
    MODBUS_STATE_HEADER,
    MODBUS_STATE_DATA,
} ModbusState;

void TaskModBusProcessor(void *pvParameters);
void TaskModBusMonitor(void *pvParameters);

bool initModBusSerial();
void updateModbusActivityMonitor(bool isModbusActive);

void sendNextPollingCommand();

void processModbusSerial(byte dataReceived);
void resetMonitorState();
int matchPageHeader();
int getPageByteSize(int matchID);
void parseFrame(int matchID);
bool validateFrame();
void parseFrameData();
void parseRegistersPage4();
void parseRegistersPage5();
void parseRegistersPage7();

bool validateCRC(uint8_t *data, uint8_t length);
uint16_t calculateCRC(uint8_t *data, uint8_t length);
long convertTo32bits(byte b1, byte b2, byte b3, byte b4);

void printByte(byte byteReceived);

void logByteToMQTT(byte byteReceived);

String getDataRecord();

void setModbusStatus(ModbusStatus status);

uint16_t modbusCRC(uint8_t *data, uint8_t len);

#endif // __MODBUSMONITOR_H__