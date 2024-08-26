#include <SPI.h>
#include <mcp2515.h>

// MOSFET control pins
#define MOSFET_AH 5  // Phase A High
#define MOSFET_AL 6  // Phase A Low
#define MOSFET_BH 7  // Phase B High
#define MOSFET_BL 8  // Phase B Low
#define MOSFET_CH 9  // Phase C High
#define MOSFET_CL 10 // Phase C Low

// ADC pins for back-EMF sensing
#define BEMF_A A0
#define BEMF_B A1
#define BEMF_C A2

// CAN bus setup, we can make our own board or buy a devboard from https://www.amazon.com/MCP2515-CAN/s?k=MCP2515+CAN:3
MCP2515 mcp2515(15);

// Motor parameters
#define POLE_PAIRS 7
#define MIN_SPEED 100 // Minimum speed for reliable sensorless operation (also needs to be tested lol)

// Control variables
uint8_t commutationStep = 0;
uint16_t motorSpeed = 0; // 0-1000, i dont do memory safety
unsigned long lastCommutationTime = 0;
unsigned long commutationPeriod = 1000000; // in microseconds, duh

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
  
  // Initialize CAN bus
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS);
  mcp2515.setNormalMode();
  
  // Set up ADC
  analogReference(DEFAULT);
}

void loop() {
  // Check for CAN messages
  struct can_frame canMsg;
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    if (canMsg.can_id == 0x123) { // Example CAN ID for speed control
      motorSpeed = (canMsg.data[0] << 8) | canMsg.data[1]; // 0-1000 range
    }
  }
  
  // Sensorless commutation
  if (motorSpeed >= MIN_SPEED) {
    sensorlessCommutation();
  } else {
    allMosfetsOff();
  }
}

void sensorlessCommutation() {
  unsigned long currentTime = micros();
  
  // Check if it's time for the next commutation step
  if (currentTime - lastCommutationTime >= commutationPeriod / 6) {
    commutationStep = (commutationStep + 1) % 6;
    lastCommutationTime = currentTime;
    
    // Apply the new commutation step
    applyCommutationStep();
    
    // Wait for the MOSFET switching transients to settle
    delayMicroseconds(10);
    
    // Detect zero-crossing and adjust timing
    detectZeroCrossing();
  }
  
  // Speed control
  if (motorSpeed < 1000) {
    unsigned long onTime = (commutationPeriod / 6) * motorSpeed / 1000;
    if (currentTime - lastCommutationTime >= onTime) {
      allMosfetsOff();
    }
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

void detectZeroCrossing() {
  int bemf = 0;
  switch(commutationStep) {
    case 0: case 3: bemf = analogRead(BEMF_C); break;
    case 1: case 4: bemf = analogRead(BEMF_B); break;
    case 2: case 5: bemf = analogRead(BEMF_A); break;
  }
  
  // Simple zero-crossing detection
  static int lastBemf = 0;
  if ((lastBemf < 512 && bemf >= 512) || (lastBemf >= 512 && bemf < 512)) {
    // Zero-crossing detected, adjust timing so we don't short the battery
    unsigned long currentPeriod = micros() - lastCommutationTime;
    commutationPeriod = currentPeriod * 6; // Approximate full rotation period idk i probably have to adjust with the t200
  }
  lastBemf = bemf;
}

void allMosfetsOff() {
  digitalWrite(MOSFET_AH, LOW);
  digitalWrite(MOSFET_AL, LOW);
  digitalWrite(MOSFET_BH, LOW);
  digitalWrite(MOSFET_BL, LOW);
  digitalWrite(MOSFET_CH, LOW);
  digitalWrite(MOSFET_CL, LOW);
}
