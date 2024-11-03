#include <SPI.h>
#include <mcp2515.h>
#include <EEPROM.h>

// MOSFET control pins
#define MOSFET_AH 0  // Phase A High
#define MOSFET_AL 2  // Phase A Low
#define MOSFET_BH 1  // Phase B High
#define MOSFET_BL 4  // Phase B Low
#define MOSFET_CH 3  // Phase C High
#define MOSFET_CL 5  // Phase C Low

// EEPROM address for CAN ID
#define CAN_ID_ADDRESS 0

// CAN bus setup
MCP2515 mcp2515(7);

// Motor parameters
#define POLE_PAIRS 7
#define MIN_SPEED 100 // Minimum speed for reliable open-loop operation

// Control variables
uint8_t commutationStep = 0;
uint16_t motorSpeed = 0; // 0-1000
unsigned long lastCommutationTime = 0;
unsigned long commutationPeriod = 1000000; // in microseconds
uint8_t canID = 0x01; // Default CAN ID (0-63), will be loaded from EEPROM

// TUI variables
#define SERIAL_BAUD 115200
unsigned long lastUpdateTime = 0;
#define UPDATE_INTERVAL 250 // ms

void setup() {
  // Initialize MOSFET control pins
  pinMode(MOSFET_AH, OUTPUT);
  pinMode(MOSFET_AL, OUTPUT);
  pinMode(MOSFET_BH, OUTPUT);
  pinMode(MOSFET_BL, OUTPUT);
  pinMode(MOSFET_CH, OUTPUT);
  pinMode(MOSFET_CL, OUTPUT);

  // Turn off all MOSFETs initially
  allMosfetsOff();

  // Load CAN ID from EEPROM
  canID = loadCanID();

  // Initialize CAN bus
  mcp2515.reset();
  mcp2515.setBitrate(CAN_1000KBPS);
  mcp2515.setNormalMode();

  // Initialize Serial for TUI
  Serial.begin(SERIAL_BAUD);
  while (!Serial) {
    ; // Wait for serial port to connect
  }
  printMenu();
}

void loop() {
  // Check for CAN messages
  struct can_frame canMsg;
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    if (canMsg.can_id == canID) { // Use the stored CAN ID
      motorSpeed = (canMsg.data[0] << 8) | canMsg.data[1]; // 0-1000 range
    }
  }

  // Open-loop commutation
  if (motorSpeed >= MIN_SPEED) {
    openLoopCommutation();
  } else {
    allMosfetsOff();
  }

  // Handle serial input
  if (Serial.available()) {
    char input = Serial.read();
    handleSerialInput(input);
  }

  // Update TUI
  unsigned long currentTime = millis();
  if (currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
    updateTUI();
    lastUpdateTime = currentTime;
  }
}

void openLoopCommutation() {
  unsigned long currentTime = micros();

  // Check if it's time for the next commutation step
  if (currentTime - lastCommutationTime >= commutationPeriod / 6) {
    commutationStep = (commutationStep + 1) % 6;
    lastCommutationTime = currentTime;

    // Apply the new commutation step
    applyCommutationStep();
  }
}

void applyCommutationStep() {
  switch(commutationStep) {
    case 0: // A-B
      digitalWrite(MOSFET_AH, HIGH);
      digitalWrite(MOSFET_BL, HIGH);
      digitalWrite(MOSFET_AL, LOW);
      digitalWrite(MOSFET_BH, LOW);
      digitalWrite(MOSFET_CH, LOW);
      digitalWrite(MOSFET_CL, LOW);
      break;
    case 1: // A-C
      digitalWrite(MOSFET_AH, HIGH);
      digitalWrite(MOSFET_CL, HIGH);
      digitalWrite(MOSFET_AL, LOW);
      digitalWrite(MOSFET_BH, LOW);
      digitalWrite(MOSFET_BL, LOW);
      digitalWrite(MOSFET_CH, LOW);
      break;
    case 2: // B-C
      digitalWrite(MOSFET_BH, HIGH);
      digitalWrite(MOSFET_CL, HIGH);
      digitalWrite(MOSFET_AL, LOW);
      digitalWrite(MOSFET_AH, LOW);
      digitalWrite(MOSFET_BL, LOW);
      digitalWrite(MOSFET_CH, LOW);
      break;
    case 3: // B-A
      digitalWrite(MOSFET_BH, HIGH);
      digitalWrite(MOSFET_AL, HIGH);
      digitalWrite(MOSFET_AH, LOW);
      digitalWrite(MOSFET_BL, LOW);
      digitalWrite(MOSFET_CH, LOW);
      digitalWrite(MOSFET_CL, LOW);
      break;
    case 4: // C-A
      digitalWrite(MOSFET_CH, HIGH);
      digitalWrite(MOSFET_AL, HIGH);
      digitalWrite(MOSFET_AH, LOW);
      digitalWrite(MOSFET_BH, LOW);
      digitalWrite(MOSFET_BL, LOW);
      digitalWrite(MOSFET_CL, LOW);
      break;
    case 5: // C-B
      digitalWrite(MOSFET_CH, HIGH);
      digitalWrite(MOSFET_BL, HIGH);
      digitalWrite(MOSFET_AL, LOW);
      digitalWrite(MOSFET_AH, LOW);
      digitalWrite(MOSFET_BH, LOW);
      digitalWrite(MOSFET_CL, LOW);
      break;
  }
}

void allMosfetsOff() {
  digitalWrite(MOSFET_AH, LOW);
  digitalWrite(MOSFET_AL, LOW);
  digitalWrite(MOSFET_BH, LOW);
  digitalWrite(MOSFET_BL, LOW);
  digitalWrite(MOSFET_CH, LOW);
  digitalWrite(MOSFET_CL, LOW);
}

void printMenu() {
  Serial.println(F("\n--- not rev hardware clientTeam1157 ---"));
  Serial.println(F("Commands:"));
  Serial.println(F("  '+' : Increase speed"));
  Serial.println(F("  '-' : Decrease speed"));
  Serial.println(F("  's' : Stop motor"));
  Serial.println(F("  'i' : Show this info"));
  Serial.println(F("  'c' : Set CAN ID (0-63)"));
  Serial.println(F("--------------------------------"));
}

void handleSerialInput(char input) {
  switch (input) {
    case '+':
      motorSpeed = min(1000, motorSpeed + 50);
      break;
    case '-':
      motorSpeed = max(0, motorSpeed - 50);
      break;
    case 's':
      motorSpeed = 0;
      break;
    case 'i':
      printMenu();
      break;
    case 'c':
      setCanID();
      break;
    default:
      Serial.println(F("Invalid command. Press 'i' for info."));
  }
}

void setCanID() {
  Serial.print(F("Enter new CAN ID (0-63): "));
  while (!Serial.available()) {
    ; // Wait for input
  }
  int id = Serial.parseInt();
  if (id >= 0 && id <= 63) { // Validate range
    canID = id;
    saveCanID(canID);
    Serial.print(F("CAN ID set to "));
    Serial.println(canID);
  } else {
    Serial.println(F("Invalid CAN ID. Please enter a value between 0 and 63."));
  }
}

void saveCanID(uint8_t id) {
  EEPROM.put(CAN_ID_ADDRESS, id);
}

uint8_t loadCanID() {
  uint8_t id;
  EEPROM.get(CAN_ID_ADDRESS, id);
  if (id > 63) { // EEPROM is empty or has invalid data
    id = 1; // Default CAN ID
    saveCanID(id);
  }
  return id;
}

void updateTUI() {
  Serial.print(F("\033[2J\033[H")); // Clear screen and move cursor to home
  Serial.println(F("--- not rev hardware client ---"));
  Serial.print(F("Motor Speed: "));
  Serial.print(motorSpeed);
  Serial.println(F(" / 1000"));
  Serial.print(F("CAN ID: "));
  Serial.println(canID);
  Serial.print(F("Commutation Step: "));
  Serial.println(commutationStep);
  Serial.print(F("Commutation Period: "));
  Serial.print(commutationPeriod);
  Serial.println(F(" us"));
  Serial.println(F("--------------------------------------"));
  Serial.println(F("Press 'i' for command info"));
}
