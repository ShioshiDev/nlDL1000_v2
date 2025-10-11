#ifndef __MODBUSDATA_H__
#define __MODBUSDATA_H__

#include <Arduino.h>

/*
 * DSE GenSet Controller Emulator- Modbus RTU Data Structures
 * 
 * These data structures match the DSE GenComm register map specification
 * and maintain the correct field order and types for proper data extraction.
 * 
 * DSE Register Pages:
 * - Page 4: Basic Instrumentation (Inner Page Addresses 0000-0065)
 * - Page 5: Extended Instrumentation (Inner Page Addresses 0010-0011)
 * - Page 6: Derived Instrumentation (Inner Page Addresses 0000-0033)
 * - Page 7: Accumulated Instrumentation (Inner Page Addresses 0006-0007)
 */

// DSE Register Size Constants -----------------------------------------------------------
const uint16_t MODBUS_PAGE4_SIZE = 66;    // Page 4 Basic Instrumentation (66 registers)
const uint16_t MODBUS_PAGE5_SIZE = 02;    // Page 5 Extended Instrumentation (02 registers)
const uint16_t MODBUS_PAGE6_SIZE = 34;    // Page 6 Derived Instrumentation (34 registers)
const uint16_t MODBUS_PAGE7_SIZE = 02;    // Page 7 Accumulated Instrumentation (02 registers)

const uint16_t MODBUS_PAGE4_ADDRESS = 1024;    // Starting Address for Page 4 = 4 * 256 = 1024
const uint16_t MODBUS_PAGE5_ADDRESS = 1280;    // Starting Address for Page 5 = 5 * 256 = 1280
const uint16_t MODBUS_PAGE6_ADDRESS = 1536;    // Starting Address for Page 6 = 6 * 256 = 1536
const uint16_t MODBUS_PAGE7_ADDRESS = 1792;    // Starting Address for Page 7 = 7 * 256 = 1792

// Page 4 - Basic Instrumentation (66 registers - offsets 0-65)
struct DSEPage4_BasicInstrumentation {
    uint16_t oilPressure;                  // 0: Oil pressure (0-10000 Kpa)
    int16_t  coolantTemp;                  // 1: Coolant temperature (-50 to 200°C, signed)
    int16_t  oilTemp;                      // 2: Oil temperature (-50 to 200°C, signed)
    uint16_t fuelLevel;                    // 3: Fuel level (0-130%)
    uint16_t chargeAlternatorVoltage;      // 4: Charge alternator voltage (0-40V, 0.1V scale)
    uint16_t engineBatteryVoltage;         // 5: Engine battery voltage (0-40V, 0.1V scale)
    uint16_t engineSpeed;                  // 6: Engine speed (0-6000 RPM)
    uint16_t generatorFrequency;           // 7: Generator frequency (0-70Hz, 0.1Hz scale)
    uint32_t generatorL1NVoltage;          // 8-9: Generator L1-N voltage (0-18000, 0.1V scale, 32-bit)
    uint32_t generatorL2NVoltage;          // 10-11: Generator L2-N voltage (0-18000, 0.1V scale, 32-bit)
    uint32_t generatorL3NVoltage;          // 12-13: Generator L3-N voltage (0-18000, 0.1V scale, 32-bit)
    uint32_t generatorL1L2Voltage;         // 14-15: Generator L1-L2 voltage (0-30000, 0.1V scale, 32-bit)
    uint32_t generatorL2L3Voltage;         // 16-17: Generator L2-L3 voltage (0-30000, 0.1V scale, 32-bit)
    uint32_t generatorL3L1Voltage;         // 18-19: Generator L3-L1 voltage (0-30000, 0.1V scale, 32-bit)
    uint32_t generatorL1Current;           // 20-21: Generator L1 current (0-999999, 0.1A scale, 32-bit)
    uint32_t generatorL2Current;           // 22-23: Generator L2 current (0-999999, 0.1A scale, 32-bit)
    uint32_t generatorL3Current;           // 24-25: Generator L3 current (0-999999, 0.1A scale, 32-bit)
    uint32_t generatorEarthCurrent;        // 26-27: Generator earth current (0-999999, 0.1A scale, 32-bit)
    int32_t  generatorL1Watts;             // 28-29: Generator L1 watts (-99999999 to 99999999W, signed 32-bit)
    int32_t  generatorL2Watts;             // 30-31: Generator L2 watts (-99999999 to 99999999W, signed 32-bit)
    int32_t  generatorL3Watts;             // 32-33: Generator L3 watts (-99999999 to 99999999W, signed 32-bit)
    int16_t  generatorCurrentLagLead;      // 34: Generator current lag/lead (-180 to +180 degrees, signed)
    uint16_t mainsFrequency;               // 35: Mains frequency (0-70Hz, 0.1Hz scale)
    uint32_t mainsL1NVoltage;              // 36-37: Mains L1-N voltage (0-18000, 0.1V scale, 32-bit)
    uint32_t mainsL2NVoltage;              // 38-39: Mains L2-N voltage (0-18000, 0.1V scale, 32-bit)
    uint32_t mainsL3NVoltage;              // 40-41: Mains L3-N voltage (0-18000, 0.1V scale, 32-bit)
    uint32_t mainsL1L2Voltage;             // 42-43: Mains L1-L2 voltage (0-30000, 0.1V scale, 32-bit)
    uint32_t mainsL2L3Voltage;             // 44-45: Mains L2-L3 voltage (0-30000, 0.1V scale, 32-bit)
    uint32_t mainsL3L1Voltage;             // 46-47: Mains L3-L1 voltage (0-30000, 0.1V scale, 32-bit)
    int16_t  mainsVoltagePhaseLagLead;     // 48: Mains voltage phase lag/lead (-180 to +180 degrees, signed)
    uint16_t generatorPhaseRotation;       // 49: Generator phase rotation (0-3)
    uint16_t mainsPhaseRotation;           // 50: Mains phase rotation (0-3)
    int16_t  mainsCurrentLagLead;          // 51: Mains current lag/lead (-180 to +180 degrees, signed)
    uint32_t mainsL1Current;               // 52-53: Mains L1 current (0-999999, 0.1A scale, 32-bit)
    uint32_t mainsL2Current;               // 54-55: Mains L2 current (0-999999, 0.1A scale, 32-bit)
    uint32_t mainsL3Current;               // 56-57: Mains L3 current (0-999999, 0.1A scale, 32-bit)
    uint32_t mainsEarthCurrent;            // 58-59: Mains earth current (0-999999, 0.1A scale, 32-bit)
    int32_t  mainsL1Watts;                 // 60-61: Mains L1 watts (-99999999 to 99999999W, signed 32-bit)
    int32_t  mainsL2Watts;                 // 62-63: Mains L2 watts (-99999999 to 99999999W, signed 32-bit)
    int32_t  mainsL3Watts;                 // 64-65: Mains L3 watts (-99999999 to 99999999W, signed 32-bit)
};

// Page 5 - Extended Instrumentation (02 registers - offsets 10-11)
struct DSEPage5_ExtendedInstrumentation {
    uint32_t fuelConsumption;              // 10-11: Fuel consumption (0-10000, 0.01 L/hour scale, 32-bit)
};

// Page 6 - Derived Instrumentation (23 registers - offsets 0-33)
struct DSEPage6_DerivedInstrumentation {
    int32_t  generatorTotalWatts;          // 0-1: Generator total watts (-99,999,999 to 99,999,999W, signed 32-bit)
    uint32_t generatorL1VA;                // 2-3: Generator L1 VA (0-99,999,999VA, 32-bit)
    uint32_t generatorL2VA;                // 4-5: Generator L2 VA (0-99,999,999VA, 32-bit)
    uint32_t generatorL3VA;                // 6-7: Generator L3 VA (0-99,999,999VA, 32-bit)
    uint32_t generatorTotalVA;             // 8-9: Generator total VA (0-99,999,999VA, signed 32-bit)
    int32_t  generatorL1VAR;               // 10-11: Generator L1 Var (-99,999,999 to 99,999,999Var, signed 32-bit)
    int32_t  generatorL2VAR;               // 12-13: Generator L2 Var (-99,999,999 to 99,999,999Var, signed 32-bit)
    int32_t  generatorL3VAR;               // 14-15: Generator L3 Var (-99,999,999 to 99,999,999Var, signed 32-bit)
    int32_t  generatorTotalVAR;            // 16-17: Generator total Var (-99,999,999 to 99,999,999Var, signed 32-bit)
    int16_t  generatorPowerFactorL1;       // 18: Generator power factor L1 (-1 to 1, 0.01 scale, signed)
    int16_t  generatorPowerFactorL2;       // 19: Generator power factor L2 (-1 to 1, 0.01 scale, signed)
    int16_t  generatorPowerFactorL3;       // 20: Generator power factor L3 (-1 to 1, 0.01 scale, signed)
    int16_t  generatorAveragePowerFactor;  // 21: Generator average power factor (-1 to 1, 0.01 scale, signed)
    int16_t  generatorPercentageFullPower; // 22: Generator percentage of full power (-999.9 to +999.9%, 0.1 scale, signed)
    int16_t  generatorPercentageFullVar;   // 23: Generator percentage of full Var (-999.9 to +999.9%, 0.1 scale, signed)
    int32_t  mainsTotalWatts;              // 24-25: Mains total watts (-99,999,999 to 999,999,999W, signed 32-bit)
    uint32_t mainsL1VA;                    // 26-27: Mains L1 VA (0-99,999,999VA, 32-bit)
    uint32_t mainsL2VA;                    // 28-29: Mains L2 VA (0-99,999,999VA, 32-bit)
    uint32_t mainsL3VA;                    // 30-31: Mains L3 VA (0-99,999,999VA, 32-bit)
    uint32_t mainsTotalVA;                 // 32-33: Mains total VA (0-999,999,999VA, 32-bit)
};

// Page 7 - Accumulated Instrumentation (02 registers - offsets 6-7)
struct DSEPage7_AccumulatedInstrumentation {
    uint32_t engineRunTime;                // 6-7: Engine run time (0-4.29x10⁹ seconds, 32-bit)
};

#endif // __MODBUSDATA_H__