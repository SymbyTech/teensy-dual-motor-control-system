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
#define MICROSTEPS 8          // 8x microstepping (smoother, less resonance)
#define MAX_SPEED 20000       // Maximum steps/second with 8x microstepping (2500 RPM)
#define MIN_SPEED 100         // Minimum steps/second
#define ACCEL_RATE 8000       // Steps/second^2 acceleration (scaled for 8x microstepping)

// Boost Parameters
#define BOOST_MULTIPLIER 1.5  // 50% speed boost
#define BOOST_DURATION 800    // Boost duration in milliseconds (longer for 8x microstepping acceleration)

// Sync Parameters
#define SYNC_CHECK_INTERVAL 1000  // Check sync every 1 second
#define SYNC_THRESHOLD 100        // Alert if motors drift >100 steps

// Serial Communication
#define SERIAL_BAUD 115200

// Boost Configuration
struct BoostConfig {
  float multiplier;     // Boost speed multiplier
  uint16_t duration;    // Boost duration in ms
  bool enabled;         // Boost enable/disable
};

BoostConfig boostConfig = {BOOST_MULTIPLIER, BOOST_DURATION, true};

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
  // Boost parameters
  bool boostActive;
  unsigned long boostStartTime;
  float boostSpeed;
  float normalSpeed;
};

// Create two motor instances
Motor motor1 = {M1_PWM_PIN, M1_DIR_PIN, 0, 0, 0, false, 1, IntervalTimer(), "Motor1", false, 0, 0, 0};
Motor motor2 = {M2_PWM_PIN, M2_DIR_PIN, 0, 0, 0, false, 1, IntervalTimer(), "Motor2", false, 0, 0, 0};

// Acceleration/Deceleration
unsigned long lastAccelUpdate = 0;
const unsigned long accelUpdateInterval = 10; // Update speed every 10ms

// Sync Tracking
unsigned long lastSyncCheck = 0;

// Command Buffer
String inputBuffer = "";
bool commandReady = false;

// Function Prototypes
void stepISR_M1();
void stepISR_M2();
void updateSpeed(Motor &m);
void updateTimers();
void processCommand(String cmd);
void setSpeed(Motor &m, float speed);
void setDirection(Motor &m, int dir);
void stopMotor(Motor &m);
void emergencyStop();
void printStatus();
void applyBoost(Motor &m, float targetSpeed);
void checkSync();

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
    // Calculate both motor speeds first
    updateSpeed(motor1);
    updateSpeed(motor2);
    // Then update timers simultaneously
    updateTimers();
    lastAccelUpdate = millis();
  }
  
  // Check Sync (every second)
  if (millis() - lastSyncCheck >= SYNC_CHECK_INTERVAL) {
    checkSync();
    lastSyncCheck = millis();
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
    m.timer.end();
    m.currentSpeed = 0;
    return;
  }
  
  // Check if boost has expired
  if (m.boostActive && (millis() - m.boostStartTime >= boostConfig.duration)) {
    m.boostActive = false;
    m.targetSpeed = m.normalSpeed;  // Return to normal speed
    Serial.print(m.name);
    Serial.println(" boost complete - returning to normal speed");
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
}

void updateTimers() {
  // Update both motor timers simultaneously for perfect sync
  // Motor 1
  if (motor1.currentSpeed > 0 && motor1.isRunning) {
    float timerPeriod1 = 1000000.0 / motor1.currentSpeed;
    motor1.timer.end();
    motor1.timer.begin(stepISR_M1, timerPeriod1);
  } else {
    motor1.timer.end();
  }
  
  // Motor 2 - start immediately after Motor 1
  if (motor2.currentSpeed > 0 && motor2.isRunning) {
    float timerPeriod2 = 1000000.0 / motor2.currentSpeed;
    motor2.timer.end();
    motor2.timer.begin(stepISR_M2, timerPeriod2);
  } else {
    motor2.timer.end();
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
    
  } else if (command == "SPIN") {
    // SPIN:LEFT:speed or SPIN:RIGHT:speed
    int colonPos = value.indexOf(':');
    String direction = value.substring(0, colonPos);
    float speed = value.substring(colonPos + 1).toFloat();
    
    if (direction == "LEFT" || direction == "L") {
      setDirection(motor1, -1);  // M1 backward
      setDirection(motor2, 1);   // M2 forward
      setSpeed(motor1, speed);
      setSpeed(motor2, speed);
      motor1.isRunning = true;
      motor2.isRunning = true;
      Serial.print("Spinning LEFT at ");
      Serial.println(speed);
    } else if (direction == "RIGHT" || direction == "R") {
      setDirection(motor1, 1);   // M1 forward
      setDirection(motor2, -1);  // M2 backward
      setSpeed(motor1, speed);
      setSpeed(motor2, speed);
      motor1.isRunning = true;
      motor2.isRunning = true;
      Serial.print("Spinning RIGHT at ");
      Serial.println(speed);
    } else {
      Serial.println("Invalid SPIN direction. Use LEFT or RIGHT");
    }
    
  } else if (command == "BOOST") {
    // BOOST:LEFT:speed or BOOST:RIGHT:speed or BOOST:FORWARD:speed
    int colonPos = value.indexOf(':');
    String direction = value.substring(0, colonPos);
    float speed = value.substring(colonPos + 1).toFloat();
    
    if (direction == "LEFT" || direction == "L") {
      setDirection(motor1, -1);
      setDirection(motor2, 1);
      applyBoost(motor1, speed);
      applyBoost(motor2, speed);
      motor1.isRunning = true;
      motor2.isRunning = true;
      Serial.print("BOOST Spin LEFT at ");
      Serial.println(speed);
    } else if (direction == "RIGHT" || direction == "R") {
      setDirection(motor1, 1);
      setDirection(motor2, -1);
      applyBoost(motor1, speed);
      applyBoost(motor2, speed);
      motor1.isRunning = true;
      motor2.isRunning = true;
      Serial.print("BOOST Spin RIGHT at ");
      Serial.println(speed);
    } else if (direction == "FORWARD" || direction == "F") {
      setDirection(motor1, 1);
      setDirection(motor2, 1);
      applyBoost(motor1, speed);
      applyBoost(motor2, speed);
      motor1.isRunning = true;
      motor2.isRunning = true;
      Serial.print("BOOST Forward at ");
      Serial.println(speed);
    } else if (direction == "BACKWARD" || direction == "B") {
      setDirection(motor1, -1);
      setDirection(motor2, -1);
      applyBoost(motor1, speed);
      applyBoost(motor2, speed);
      motor1.isRunning = true;
      motor2.isRunning = true;
      Serial.print("BOOST Backward at ");
      Serial.println(speed);
    } else {
      Serial.println("Invalid BOOST direction");
    }
    
  } else if (command == "SYNC") {
    // Reset both motor positions simultaneously
    noInterrupts();
    motor1.position = 0;
    motor2.position = 0;
    interrupts();
    Serial.println("Motors synchronized - positions reset");
    
  } else if (command == "CONFIG") {
    // CONFIG:BOOST:multiplier:duration:enabled
    // Example: CONFIG:BOOST:1.5:200:1
    if (value.startsWith("BOOST:")) {
      String params = value.substring(6);  // Remove "BOOST:"
      int colon1 = params.indexOf(':');
      int colon2 = params.indexOf(':', colon1 + 1);
      
      boostConfig.multiplier = params.substring(0, colon1).toFloat();
      boostConfig.duration = params.substring(colon1 + 1, colon2).toInt();
      boostConfig.enabled = params.substring(colon2 + 1).toInt() == 1;
      
      Serial.println("Boost configuration updated:");
      Serial.print("  Multiplier: ");
      Serial.println(boostConfig.multiplier);
      Serial.print("  Duration: ");
      Serial.print(boostConfig.duration);
      Serial.println(" ms");
      Serial.print("  Enabled: ");
      Serial.println(boostConfig.enabled ? "YES" : "NO");
    } else {
      Serial.println("CONFIG:BOOST:multiplier:duration:enabled");
      Serial.println("Example: CONFIG:BOOST:1.5:200:1");
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
    Serial.println("  SPIN:LEFT:speed - Spin left (point turn)");
    Serial.println("  SPIN:RIGHT:speed - Spin right (point turn)");
    Serial.println("  BOOST:LEFT:speed - Boosted spin left");
    Serial.println("  BOOST:RIGHT:speed - Boosted spin right");
    Serial.println("  SYNC - Synchronize motor positions");
    Serial.println("  CONFIG:BOOST:mult:dur:enabled - Configure boost");
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
  int newDir = (dir >= 0) ? 1 : -1;
  
  // If changing direction at high speed, slow down first
  if (newDir != m.direction && m.currentSpeed > 500) {
    Serial.print(m.name);
    Serial.println(" slowing for direction change...");
    
    // Reduce speed before direction change
    float originalTarget = m.targetSpeed;
    m.targetSpeed = 200;  // Slow to safe speed
    
    while (m.currentSpeed > 300) {
      updateSpeed(m);
      delay(accelUpdateInterval);
    }
    
    // Now safe to change direction
    m.direction = newDir;
    digitalWrite(m.dirPin, m.direction == 1 ? LOW : HIGH);
    
    // Restore target speed
    m.targetSpeed = originalTarget;
  } else {
    // Safe to change directly
    m.direction = newDir;
    digitalWrite(m.dirPin, m.direction == 1 ? LOW : HIGH);
  }
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
  // Quick ramp-down stop (0.5 second) to prevent mechanical stress
  Serial.println("EMERGENCY STOP - Ramping down...");
  
  // Set both motors to decelerate quickly
  motor1.targetSpeed = 0;
  motor2.targetSpeed = 0;
  
  // Quick deceleration over 0.5 seconds
  unsigned long stopStartTime = millis();
  while ((motor1.currentSpeed > 1 || motor2.currentSpeed > 1) && (millis() - stopStartTime < 500)) {
    updateSpeed(motor1);
    updateSpeed(motor2);
    delay(accelUpdateInterval);
  }
  
  // Force complete stop
  motor1.timer.end();
  motor2.timer.end();
  motor1.isRunning = false;
  motor2.isRunning = false;
  motor1.currentSpeed = 0;
  motor2.currentSpeed = 0;
  digitalWrite(M1_PWM_PIN, LOW);
  digitalWrite(M2_PWM_PIN, LOW);
  
  Serial.println("Motors stopped safely.");
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
  Serial.print("  Boost Active: ");
  Serial.println(motor1.boostActive ? "YES" : "NO");
  
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
  Serial.print("  Boost Active: ");
  Serial.println(motor2.boostActive ? "YES" : "NO");
  
  // Sync status
  long posDiff = abs(motor1.position - motor2.position);
  Serial.print("--- Sync Drift: ");
  Serial.print(posDiff);
  Serial.println(" steps ---");
  
  Serial.println("===================================");
}

void applyBoost(Motor &m, float targetSpeed) {
  if (!boostConfig.enabled) {
    // Boost disabled - use normal speed
    setSpeed(m, targetSpeed);
    return;
  }
  
  // Calculate boost speed
  float boostSpeed = targetSpeed * boostConfig.multiplier;
  
  // Safety cap - never exceed absolute max
  if (boostSpeed > MAX_SPEED) {
    boostSpeed = MAX_SPEED;
  }
  
  // Store normal speed for later
  m.normalSpeed = targetSpeed;
  m.boostSpeed = boostSpeed;
  
  // Activate boost
  m.boostActive = true;
  m.boostStartTime = millis();
  m.targetSpeed = boostSpeed;  // Start with boosted speed
  
  Serial.print(m.name);
  Serial.print(" boost activated: ");
  Serial.print(boostSpeed);
  Serial.print(" steps/sec for ");
  Serial.print(boostConfig.duration);
  Serial.println(" ms");
}

void checkSync() {
  // Calculate position difference
  long posDiff = abs(motor1.position - motor2.position);
  
  // Alert if drift exceeds threshold
  if (posDiff > SYNC_THRESHOLD && (motor1.isRunning || motor2.isRunning)) {
    Serial.print("⚠️  SYNC WARNING: Position drift = ");
    Serial.print(posDiff);
    Serial.println(" steps");
    Serial.print("   Motor1: ");
    Serial.print(motor1.position);
    Serial.print(" | Motor2: ");
    Serial.println(motor2.position);
  }
}
