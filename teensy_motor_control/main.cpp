/*
 * Teensy 4.1 Dual Motor Control System (Single Board)
 * For Wantai 85BYGH450C-060 Stepper Motor with DQ860HA-V3.3 Driver
 * 
 * Hardware Connections:
 * Motor 1 (Left):
 *   - Pin 2: PWM/STEP to Driver 1 PUL+
 *   - Pin 3: DIR to Driver 1 DIR+
 * Motor 2 (Right):
 *   - Pin 4: PWM/STEP to Driver 2 PUL+
 *   - Pin 5: DIR to Driver 2 DIR+
 * 
 * Motor Specifications:
 * - Wantai 85BYGH450C-060: 1.8° step angle, 200 steps/rev
 * - DQ860HA-V3.3: Supports up to 400kHz pulse frequency
 * - Full-step mode (200 steps/rev)
 */

#include <Arduino.h>

// Motor 1 Pin Definitions (Left/Port)
#define M1_PWM_PIN 2
#define M1_DIR_PIN 3

// Motor 2 Pin Definitions (Right/Starboard)
#define M2_PWM_PIN 4
#define M2_DIR_PIN 5

// Motor Parameters
#define STEPS_PER_REV 200
#define MICROSTEPS 1          // Full-step mode
#define MAX_SPEED 20000       // Maximum steps/second
#define MIN_SPEED 100         // Minimum steps/second
#define ACCEL_RATE 5000       // Steps/second^2 acceleration

// Serial Communication
#define SERIAL_BAUD 115200

// Motor Structure
struct Motor {
  uint8_t pwmPin;
  uint8_t dirPin;
  volatile long position;
  volatile float currentSpeed;
  volatile float targetSpeed;
  volatile bool isRunning;
  volatile int direction;  // 1 = forward, -1 = backward
  IntervalTimer timer;
  const char* name;
};

// Create two motor instances
Motor motor1 = {M1_PWM_PIN, M1_DIR_PIN, 0, 0, 0, false, 1, IntervalTimer(), "Motor1"};
Motor motor2 = {M2_PWM_PIN, M2_DIR_PIN, 0, 0, 0, false, 1, IntervalTimer(), "Motor2"};

// Acceleration/Deceleration
unsigned long lastAccelUpdate = 0;
const unsigned long accelUpdateInterval = 10; // Update speed every 10ms

// Command Buffer
String inputBuffer = "";
bool commandReady = false;

// Function Prototypes
void stepISR_M1();
void stepISR_M2();
void updateSpeed(Motor &m);
void processCommand(String cmd);
void setSpeed(Motor &m, float speed);
void setDirection(Motor &m, int dir);
void stopMotor(Motor &m);
void emergencyStop();
void printStatus();

void setup() {
  // Initialize Motor 1 pins
  pinMode(M1_PWM_PIN, OUTPUT);
  pinMode(M1_DIR_PIN, OUTPUT);
  digitalWrite(M1_PWM_PIN, LOW);
  digitalWrite(M1_DIR_PIN, LOW);
  
  // Initialize Motor 2 pins
  pinMode(M2_PWM_PIN, OUTPUT);
  pinMode(M2_DIR_PIN, OUTPUT);
  digitalWrite(M2_PWM_PIN, LOW);
  digitalWrite(M2_DIR_PIN, LOW);
  
  // Initialize LED
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Initialize Serial Communication
  Serial.begin(SERIAL_BAUD);
  while (!Serial && millis() < 3000); // Wait up to 3 seconds for USB serial
  
  Serial.println("==========================================");
  Serial.println("Teensy 4.1 Dual Motor Controller");
  Serial.println("Single board controlling 2 motors");
  Serial.println("Ready for commands");
  Serial.println("==========================================");
  
  // Reserve buffer space
  inputBuffer.reserve(128);
  
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
    updateSpeed(motor1);
    updateSpeed(motor2);
    lastAccelUpdate = millis();
  }
  
  // Status LED heartbeat
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 1000) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    lastBlink = millis();
  }
}

// Motor 1 Step ISR
void stepISR_M1() {
  digitalWrite(M1_PWM_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(M1_PWM_PIN, LOW);
  motor1.position += motor1.direction;
}

// Motor 2 Step ISR
void stepISR_M2() {
  digitalWrite(M2_PWM_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(M2_PWM_PIN, LOW);
  motor2.position += motor2.direction;
}

void updateSpeed(Motor &m) {
  if (!m.isRunning) {
    return;
  }
  
  float speedDiff = m.targetSpeed - m.currentSpeed;
  float accelStep = (ACCEL_RATE * accelUpdateInterval) / 1000.0;
  
  // Smooth acceleration/deceleration
  if (abs(speedDiff) > accelStep) {
    if (speedDiff > 0) {
      m.currentSpeed += accelStep;
    } else {
      m.currentSpeed -= accelStep;
    }
  } else {
    m.currentSpeed = m.targetSpeed;
  }
  
  // Constrain speed
  m.currentSpeed = constrain(m.currentSpeed, 0, MAX_SPEED);
  
  // Update timer frequency
  if (m.currentSpeed > 0) {
    float timerPeriod = 1000000.0 / m.currentSpeed;
    m.timer.end();
    
    // Select correct ISR based on motor
    if (m.pwmPin == M1_PWM_PIN) {
      m.timer.begin(stepISR_M1, timerPeriod);
    } else {
      m.timer.begin(stepISR_M2, timerPeriod);
    }
  } else {
    m.timer.end();
  }
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  
  // Parse command format: COMMAND:VALUE or MOTOR:COMMAND:VALUE
  int firstColon = cmd.indexOf(':');
  String command = "";
  String value = "";
  Motor *targetMotor = nullptr;
  
  // Check if command starts with M1 or M2
  if (cmd.startsWith("M1:") || cmd.startsWith("1:")) {
    targetMotor = &motor1;
    cmd = cmd.substring(cmd.indexOf(':') + 1);
  } else if (cmd.startsWith("M2:") || cmd.startsWith("2:")) {
    targetMotor = &motor2;
    cmd = cmd.substring(cmd.indexOf(':') + 1);
  }
  
  // Parse remaining command
  int separatorIndex = cmd.indexOf(':');
  if (separatorIndex > 0) {
    command = cmd.substring(0, separatorIndex);
    value = cmd.substring(separatorIndex + 1);
  } else {
    command = cmd;
  }
  
  // Process Commands
  if (command == "SPEED" || command == "S") {
    float speed = value.toFloat();
    if (targetMotor) {
      setSpeed(*targetMotor, speed);
      Serial.print(targetMotor->name);
      Serial.print(" speed set to: ");
      Serial.println(speed);
    } else {
      // Set both motors
      setSpeed(motor1, speed);
      setSpeed(motor2, speed);
      Serial.print("Both motors speed set to: ");
      Serial.println(speed);
    }
    
  } else if (command == "FORWARD" || command == "FWD" || command == "F") {
    if (targetMotor) {
      setDirection(*targetMotor, 1);
      Serial.print(targetMotor->name);
      Serial.println(" direction: FORWARD");
    } else {
      setDirection(motor1, 1);
      setDirection(motor2, 1);
      Serial.println("Both motors direction: FORWARD");
    }
    
  } else if (command == "BACKWARD" || command == "BACK" || command == "B") {
    if (targetMotor) {
      setDirection(*targetMotor, -1);
      Serial.print(targetMotor->name);
      Serial.println(" direction: BACKWARD");
    } else {
      setDirection(motor1, -1);
      setDirection(motor2, -1);
      Serial.println("Both motors direction: BACKWARD");
    }
    
  } else if (command == "STOP" || command == "X") {
    if (targetMotor) {
      stopMotor(*targetMotor);
      Serial.print(targetMotor->name);
      Serial.println(" stopped");
    } else {
      stopMotor(motor1);
      stopMotor(motor2);
      Serial.println("Both motors stopped");
    }
    
  } else if (command == "ESTOP" || command == "E") {
    emergencyStop();
    Serial.println("EMERGENCY STOP - ALL MOTORS");
    
  } else if (command == "RUN" || command == "R") {
    if (targetMotor) {
      targetMotor->isRunning = true;
      Serial.print(targetMotor->name);
      Serial.println(" running");
    } else {
      motor1.isRunning = true;
      motor2.isRunning = true;
      Serial.println("Both motors running");
    }
    
  } else if (command == "STATUS" || command == "?") {
    printStatus();
    
  } else if (command == "RESET" || command == "RST") {
    if (targetMotor) {
      targetMotor->position = 0;
      stopMotor(*targetMotor);
      Serial.print(targetMotor->name);
      Serial.println(" reset");
    } else {
      motor1.position = 0;
      motor2.position = 0;
      stopMotor(motor1);
      stopMotor(motor2);
      Serial.println("Both motors reset");
    }
    
  } else {
    Serial.print("Unknown command: ");
    Serial.println(cmd);
    Serial.println("Available commands:");
    Serial.println("  SPEED:value or S:value - Set both motors speed");
    Serial.println("  M1:SPEED:value - Set Motor 1 speed");
    Serial.println("  M2:SPEED:value - Set Motor 2 speed");
    Serial.println("  FORWARD or F - Both motors forward");
    Serial.println("  M1:FORWARD - Motor 1 forward");
    Serial.println("  M2:BACKWARD - Motor 2 backward");
    Serial.println("  RUN or R - Start motor(s)");
    Serial.println("  STOP or X - Stop motor(s)");
    Serial.println("  ESTOP or E - Emergency stop all");
    Serial.println("  STATUS or ? - Get status");
    Serial.println("  RESET - Reset position(s) to zero");
  }
}

void setSpeed(Motor &m, float speed) {
  speed = constrain(speed, 0, MAX_SPEED);
  m.targetSpeed = speed;
  
  if (speed > 0 && !m.isRunning) {
    m.isRunning = true;
  } else if (speed == 0) {
    m.isRunning = false;
  }
}

void setDirection(Motor &m, int dir) {
  m.direction = (dir >= 0) ? 1 : -1;
  digitalWrite(m.dirPin, m.direction == 1 ? LOW : HIGH);
}

void stopMotor(Motor &m) {
  // Gradual stop
  m.targetSpeed = 0;
  // Wait for deceleration
  while (m.currentSpeed > 1) {
    updateSpeed(m);
    delay(accelUpdateInterval);
  }
  m.isRunning = false;
  m.timer.end();
  m.currentSpeed = 0;
}

void emergencyStop() {
  // Immediate stop both motors
  motor1.timer.end();
  motor2.timer.end();
  motor1.isRunning = false;
  motor2.isRunning = false;
  motor1.currentSpeed = 0;
  motor2.currentSpeed = 0;
  motor1.targetSpeed = 0;
  motor2.targetSpeed = 0;
  digitalWrite(M1_PWM_PIN, LOW);
  digitalWrite(M2_PWM_PIN, LOW);
}

void printStatus() {
  Serial.println("======== DUAL MOTOR STATUS ========");
  
  Serial.println("--- Motor 1 (Left/Port) ---");
  Serial.print("  Running: ");
  Serial.println(motor1.isRunning ? "YES" : "NO");
  Serial.print("  Current Speed: ");
  Serial.println(motor1.currentSpeed);
  Serial.print("  Target Speed: ");
  Serial.println(motor1.targetSpeed);
  Serial.print("  Direction: ");
  Serial.println(motor1.direction == 1 ? "FORWARD" : "BACKWARD");
  Serial.print("  Position: ");
  Serial.println(motor1.position);
  
  Serial.println("--- Motor 2 (Right/Starboard) ---");
  Serial.print("  Running: ");
  Serial.println(motor2.isRunning ? "YES" : "NO");
  Serial.print("  Current Speed: ");
  Serial.println(motor2.currentSpeed);
  Serial.print("  Target Speed: ");
  Serial.println(motor2.targetSpeed);
  Serial.print("  Direction: ");
  Serial.println(motor2.direction == 1 ? "FORWARD" : "BACKWARD");
  Serial.print("  Position: ");
  Serial.println(motor2.position);
  
  Serial.println("===================================");
}
