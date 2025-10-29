/*
 * Teensy 4.1 Dual Motor Control System
 * For Wantai 85BYGH450C-060 Stepper Motor with DQ860HA-V3.3 Driver
 * 
 * Hardware Connections:
 * - Pin 0: PWM to driver (speed control)
 * - Pin 1: DIR to driver (direction control)
 * - Serial1 (RX1=Pin 0, TX1=Pin 1) for Raspberry Pi communication
 * 
 * Motor Specifications:
 * - Wantai 85BYGH450C-060: 1.8° step angle, 200 steps/rev
 * - DQ860HA-V3.3: Supports up to 400kHz pulse frequency
 */

#include <Arduino.h>

// Pin Definitions
#define PWM_PIN 2     // Changed from 0 to avoid conflict with Serial
#define DIR_PIN 3     // Changed from 1 to avoid conflict with Serial
#define ENABLE_PIN 4  // Optional enable pin for driver

// Motor Parameters
#define STEPS_PER_REV 200
#define MICROSTEPS 8          // Adjust based on driver DIP switch settings
#define MAX_SPEED 20000       // Maximum steps/second
#define MIN_SPEED 100         // Minimum steps/second
#define ACCEL_RATE 5000       // Steps/second^2 acceleration

// Serial Communication
#define SERIAL_BAUD 115200
#define MOTOR_ID 1            // Set to 1 for left motor, 2 for right motor

// Motion Control Variables
volatile long targetPosition = 0;
volatile long currentPosition = 0;
volatile float currentSpeed = 0;
volatile float targetSpeed = 0;
volatile bool isRunning = false;
volatile int direction = 1;  // 1 = forward, -1 = backward

// Acceleration/Deceleration
IntervalTimer stepTimer;
unsigned long lastAccelUpdate = 0;
const unsigned long accelUpdateInterval = 10; // Update speed every 10ms

// Command Buffer
String inputBuffer = "";
bool commandReady = false;

// Function Prototypes
void stepISR();
void updateSpeed();
void processCommand(String cmd);
void setSpeed(float speed);
void setDirection(int dir);
void stopMotor();
void emergencyStop();

void setup() {
  // Initialize pins
  pinMode(PWM_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  
  digitalWrite(PWM_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  digitalWrite(ENABLE_PIN, HIGH); // Enable driver (active high)
  
  // Initialize Serial Communication
  Serial.begin(SERIAL_BAUD);
  while (!Serial && millis() < 3000); // Wait up to 3 seconds for USB serial
  
  Serial.println("=================================");
  Serial.print("Teensy 4.1 Motor Controller #");
  Serial.println(MOTOR_ID);
  Serial.println("Ready for commands");
  Serial.println("=================================");
  
  // Reserve buffer space
  inputBuffer.reserve(64);
  
  // Blink LED to indicate ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
}

void loop() {
  // Read Serial Commands
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    
    if (inChar == '\n' || inChar == '\r') {
      if (inputBuffer.length() > 0) {
        commandReady = true;
      }
    } else {
      inputBuffer += inChar;
    }
  }
  
  // Process Command
  if (commandReady) {
    processCommand(inputBuffer);
    inputBuffer = "";
    commandReady = false;
  }
  
  // Update Speed (Acceleration/Deceleration)
  if (millis() - lastAccelUpdate >= accelUpdateInterval) {
    updateSpeed();
    lastAccelUpdate = millis();
  }
  
  // Status LED heartbeat
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 1000) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    lastBlink = millis();
  }
}

void stepISR() {
  // Generate step pulse
  digitalWrite(PWM_PIN, HIGH);
  delayMicroseconds(5); // Minimum pulse width for DQ860HA
  digitalWrite(PWM_PIN, LOW);
  
  // Update position
  currentPosition += direction;
}

void updateSpeed() {
  if (!isRunning) {
    return;
  }
  
  float speedDiff = targetSpeed - currentSpeed;
  float accelStep = (ACCEL_RATE * accelUpdateInterval) / 1000.0;
  
  // Smooth acceleration/deceleration
  if (abs(speedDiff) > accelStep) {
    if (speedDiff > 0) {
      currentSpeed += accelStep;
    } else {
      currentSpeed -= accelStep;
    }
  } else {
    currentSpeed = targetSpeed;
  }
  
  // Constrain speed
  currentSpeed = constrain(currentSpeed, 0, MAX_SPEED);
  
  // Update timer frequency
  if (currentSpeed > 0) {
    float stepFrequency = currentSpeed;
    float timerPeriod = 1000000.0 / stepFrequency; // Period in microseconds
    
    stepTimer.end();
    stepTimer.begin(stepISR, timerPeriod);
  } else {
    stepTimer.end();
  }
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  
  // Parse command format: COMMAND:VALUE
  int separatorIndex = cmd.indexOf(':');
  String command = "";
  String value = "";
  
  if (separatorIndex > 0) {
    command = cmd.substring(0, separatorIndex);
    value = cmd.substring(separatorIndex + 1);
  } else {
    command = cmd;
  }
  
  // Process Commands
  if (command == "SPEED" || command == "S") {
    float speed = value.toFloat();
    setSpeed(speed);
    Serial.print("Speed set to: ");
    Serial.println(speed);
    
  } else if (command == "FORWARD" || command == "FWD" || command == "F") {
    setDirection(1);
    Serial.println("Direction: FORWARD");
    
  } else if (command == "BACKWARD" || command == "BACK" || command == "B") {
    setDirection(-1);
    Serial.println("Direction: BACKWARD");
    
  } else if (command == "STOP" || command == "X") {
    stopMotor();
    Serial.println("Motor stopped");
    
  } else if (command == "ESTOP" || command == "E") {
    emergencyStop();
    Serial.println("EMERGENCY STOP");
    
  } else if (command == "RUN" || command == "R") {
    isRunning = true;
    Serial.println("Motor running");
    
  } else if (command == "STATUS" || command == "?") {
    Serial.println("--- Motor Status ---");
    Serial.print("Motor ID: ");
    Serial.println(MOTOR_ID);
    Serial.print("Running: ");
    Serial.println(isRunning ? "YES" : "NO");
    Serial.print("Current Speed: ");
    Serial.println(currentSpeed);
    Serial.print("Target Speed: ");
    Serial.println(targetSpeed);
    Serial.print("Direction: ");
    Serial.println(direction == 1 ? "FORWARD" : "BACKWARD");
    Serial.print("Position: ");
    Serial.println(currentPosition);
    Serial.println("-------------------");
    
  } else if (command == "RESET" || command == "RST") {
    currentPosition = 0;
    targetPosition = 0;
    stopMotor();
    Serial.println("Motor reset");
    
  } else if (command == "ENABLE" || command == "EN") {
    digitalWrite(ENABLE_PIN, HIGH);
    Serial.println("Driver enabled");
    
  } else if (command == "DISABLE" || command == "DIS") {
    stopMotor();
    digitalWrite(ENABLE_PIN, LOW);
    Serial.println("Driver disabled");
    
  } else {
    Serial.print("Unknown command: ");
    Serial.println(cmd);
    Serial.println("Available commands:");
    Serial.println("  SPEED:value or S:value - Set speed (0-20000)");
    Serial.println("  FORWARD or F - Set forward direction");
    Serial.println("  BACKWARD or B - Set backward direction");
    Serial.println("  RUN or R - Start motor");
    Serial.println("  STOP or X - Stop motor");
    Serial.println("  ESTOP or E - Emergency stop");
    Serial.println("  STATUS or ? - Get motor status");
    Serial.println("  RESET - Reset position to zero");
    Serial.println("  ENABLE - Enable driver");
    Serial.println("  DISABLE - Disable driver");
  }
}

void setSpeed(float speed) {
  // Constrain speed to valid range
  speed = constrain(speed, 0, MAX_SPEED);
  targetSpeed = speed;
  
  if (speed > 0 && !isRunning) {
    isRunning = true;
  } else if (speed == 0) {
    isRunning = false;
  }
}

void setDirection(int dir) {
  // Update direction
  direction = (dir >= 0) ? 1 : -1;
  digitalWrite(DIR_PIN, direction == 1 ? LOW : HIGH);
}

void stopMotor() {
  // Gradual stop
  targetSpeed = 0;
  // Wait for deceleration
  while (currentSpeed > 1) {
    updateSpeed();
    delay(accelUpdateInterval);
  }
  isRunning = false;
  stepTimer.end();
  currentSpeed = 0;
}

void emergencyStop() {
  // Immediate stop
  stepTimer.end();
  isRunning = false;
  currentSpeed = 0;
  targetSpeed = 0;
  digitalWrite(PWM_PIN, LOW);
}
