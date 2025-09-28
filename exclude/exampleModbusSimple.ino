#include <SoftwareSerial.h>

// -------------------- RS-485 / Modbus wiring --------------------
#define RS485_TX_ENABLE 0
#define RS485_RX_ENABLE 1
#define TXD 17
#define RXD 18

// -------------------- Modbus constants --------------------
#define MODBUS_ID       0x0A   // Slave ID 10
#define FUNC_READ_HR    0x03
#define START_REG_PG4   1024   // Page 4 start address
#define REG_COUNT       10     // Number of registers to read

// -------------------- Timings --------------------
#define TIMEOUT_MS        1000  // Max wait for response
#define POLL_PERIOD_MS    3000  // Poll every 3 seconds
// At 115200 baud, one char ≈ 87 µs; 3.5 chars ≈ ~0.3 ms. Use margin:
#define PRE_TX_SILENT_MS    1   // Guard before TX
#define POST_TX_SILENT_MS   1   // Guard after TX before listening

// -------------------- Globals --------------------
SoftwareSerial rs485Serial(RXD, TXD); // RX, TX
volatile int modbus_crc_status = 0;   // 1 = CRC OK, 0 = NOT OK

// -------------------- Forward declarations --------------------
uint16_t modbusCRC(uint8_t *data, uint16_t len);
void rs485FlushInput();
int pollPage4_10();  // Poll page 4, 10 registers

void setup() {
  Serial.begin(115200);

  pinMode(RS485_TX_ENABLE, OUTPUT);
  pinMode(RS485_RX_ENABLE, OUTPUT);
  digitalWrite(RS485_TX_ENABLE, LOW); // Disable TX initially
  digitalWrite(RS485_RX_ENABLE, LOW); // Enable RX

  rs485Serial.begin(115200);          // Now using 115200 baud
  rs485Serial.listen();

  Serial.println("Modbus Master: Poll Page4 (10 regs) every 3s at 115200 baud");
}

void loop() {
  int ok = pollPage4_10();
  Serial.print("Poll result: ");
  Serial.print(ok ? "CRC OK" : "NOT OK");
  Serial.print("  | modbus_crc_status=");
  Serial.println(modbus_crc_status);

  delay(POLL_PERIOD_MS);
}

// ------------------------------------------------------------
// Poll Page 4 (start=1024), 10 registers, ID=10
// Sets modbus_crc_status = 1 if CRC OK, else 0
// ------------------------------------------------------------
int pollPage4_10() {
  delay(PRE_TX_SILENT_MS);

  // Build Modbus RTU request
  uint8_t request[8];
  request[0] = MODBUS_ID;
  request[1] = FUNC_READ_HR;
  request[2] = highByte(START_REG_PG4);
  request[3] = lowByte(START_REG_PG4);
  request[4] = 0x00;
  request[5] = REG_COUNT;
  uint16_t crc = modbusCRC(request, 6);
  request[6] = lowByte(crc);
  request[7] = highByte(crc);

  rs485FlushInput();  // Clear any stale bytes

  // Enable TX
  digitalWrite(RS485_RX_ENABLE, HIGH);
  digitalWrite(RS485_TX_ENABLE, HIGH);
  delayMicroseconds(500);

  rs485Serial.write(request, 8);
  rs485Serial.flush();

  // Turnaround: disable TX, enable RX
  digitalWrite(RS485_TX_ENABLE, LOW);
  digitalWrite(RS485_RX_ENABLE, LOW);
  delay(POST_TX_SILENT_MS);
  rs485Serial.listen();

  // Expected reply: ID+FUNC+BYTECNT+DATA(20)+CRC(2) = 25 bytes
  const uint8_t  expectedDataBytes = 2 * REG_COUNT;
  const uint16_t expectedLen = 3 + expectedDataBytes + 2;

  uint8_t response[64];
  uint16_t index = 0;
  unsigned long start = millis();

  while ((millis() - start) < TIMEOUT_MS && index < expectedLen) {
    if (rs485Serial.available()) {
      int b = rs485Serial.read();
      if (b >= 0) response[index++] = (uint8_t)b;
    }
  }

  modbus_crc_status = 0;  // Default to NOT OK

  if (index < expectedLen) {
    return 0; // Timeout or incomplete frame
  }

  // Basic header validation
  if (response[0] != MODBUS_ID || response[1] != FUNC_READ_HR || response[2] != expectedDataBytes) {
    return 0;
  }

  // CRC check
  uint16_t recvCRC = (response[index - 1] << 8) | response[index - 2];
  uint16_t calcCRC = modbusCRC(response, index - 2);

  if (recvCRC == calcCRC) {
    modbus_crc_status = 1;
    return 1;
  }
  return 0;
}

// ------------------------------------------------------------
void rs485FlushInput() {
  while (rs485Serial.available()) (void)rs485Serial.read();
}

// Modbus RTU CRC-16
uint16_t modbusCRC(uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
    }
  }
  return crc;
}