#include <Arduino.h>
#include <IntervalTimer.h>

// Simple Joystick Firmware - WITH COMPREHENSIVE DEBUG LOGGING
// Logs every command received and response sent

// ================= PINS =================
const int stepPin = 0;
const int dirPin  = 1;

// ================= TIMER / ISR STATE =================
IntervalTimer stepTimer;
volatile bool stepState = false;
volatile bool pulseEnable = false;

// ================= CONFIGURABLE PARAMETERS =================
float fStart = 100.0;
float targetFreq = 3000.0;
float rampTimeMs = 1000.0;
float holdStationaryMs = 100.0;

// ================= CURRENT STATE =================
float currentFreq = 0.0;
float interval_us = 0.0;

// ================= RAMPING STATE =================
unsigned long rampStartTime = 0;
unsigned long rampDurationMs = 0;
float rampFromFreq = 0.0;
float rampToFreq = 0.0;
bool rampActive = false;

// ================= DIRECTION STATE =================
bool curDirFwd = true;
bool reqDirFwd = true;
bool dirChangeRequested = false;
unsigned long dirHoldStartMs = 0;
bool inDirectionHold = false;

// ================= LOGGING / STATISTICS =================
unsigned long cmdCount = 0;
unsigned long responseCount = 0;
unsigned long feedbackCount = 0;
unsigned long errorCount = 0;
volatile unsigned long stepsExecuted = 0;  // Total steps counted
unsigned long lastStatsMs = 0;
const unsigned long statsIntervalMs = 10000;  // Stats every 10 seconds

// ================= FEEDBACK =================
unsigned long lastFeedbackMs = 0;
const unsigned long feedbackIntervalMs = 500;

// ================= DEBUG LOGGING =================
void logCommand(const char* cmd) {
  cmdCount++;
  Serial.print("DBG RX [");
  Serial.print(cmdCount);
  Serial.print("] ");
  Serial.println(cmd);
}

void logResponse(const char* response) {
  responseCount++;
  Serial.print("DBG TX [");
  Serial.print(responseCount);
  Serial.print("] ");
  Serial.println(response);
}

void logState() {
  Serial.print("DBG STATE dir=");
  Serial.print(curDirFwd ? "FWD" : "BWD");
  Serial.print(" freq=");
  Serial.print((int)currentFreq);
  Serial.print(" ramp=");
  Serial.print(rampActive ? "Y" : "N");
  Serial.print(" dirChange=");
  Serial.println(dirChangeRequested ? "Y" : "N");
}

void printStats() {
  Serial.println("========== STATISTICS ==========");
  Serial.print("Commands received: ");
  Serial.println(cmdCount);
  Serial.print("Responses sent: ");
  Serial.println(responseCount);
  Serial.print("Feedback sent: ");
  Serial.println(feedbackCount);
  Serial.print("Errors: ");
  Serial.println(errorCount);
  Serial.print("Steps executed: ");
  Serial.println(stepsExecuted);
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  Serial.println("================================");
}

// ================= ISR =================
void stepISR() {
  if (!pulseEnable) return;
  stepState = !stepState;
  digitalWrite(stepPin, stepState ? HIGH : LOW);
  
  // Count steps on rising edge
  if (stepState) {
    stepsExecuted++;
  }
}

// ================= TIMER UPDATE =================
void updateTimerInterval(float freq) {
  if (freq < 0.0) freq = 0.0;
  if (freq > targetFreq) freq = targetFreq;
  
  if (freq < 1.0) {
    pulseEnable = false;
    currentFreq = 0.0;
    digitalWrite(stepPin, LOW);
    return;
  }
  
  currentFreq = freq;
  interval_us = 1000000.0 / (freq * 2.0);
  stepTimer.update(interval_us);
  pulseEnable = true;
}

// ================= RAMPING =================
void startRamp(float toFreq) {
  rampFromFreq = currentFreq;
  rampToFreq = toFreq;
  rampStartTime = millis();
  
  float freqChange = abs(rampToFreq - rampFromFreq);
  float freqRange = targetFreq - fStart;
  if (freqRange > 0) {
    rampDurationMs = (unsigned long)(rampTimeMs * (freqChange / freqRange));
    rampDurationMs = max(rampDurationMs, 100UL);
  } else {
    rampDurationMs = (unsigned long)rampTimeMs;
  }
  
  rampActive = true;
  
  Serial.print("DBG RAMP from=");
  Serial.print((int)rampFromFreq);
  Serial.print(" to=");
  Serial.print((int)rampToFreq);
  Serial.print(" duration=");
  Serial.print(rampDurationMs);
  Serial.println("ms");
}

void updateRamp() {
  if (!rampActive) return;
  
  unsigned long now = millis();
  unsigned long elapsed = now - rampStartTime;
  
  if (elapsed >= rampDurationMs) {
    updateTimerInterval(rampToFreq);
    rampActive = false;
    Serial.print("DBG RAMP complete at ");
    Serial.print((int)rampToFreq);
    Serial.println(" Hz");
    return;
  }
  
  float t = (float)elapsed / (float)rampDurationMs;
  float sCurve = (1.0 - cos(PI * t)) / 2.0;
  float interpFreq = rampFromFreq + (rampToFreq - rampFromFreq) * sCurve;
  
  updateTimerInterval(interpFreq);
}

// ================= DIRECTION CHANGE HANDLING =================
void handleDirectionChange() {
  if (!dirChangeRequested) return;
  
  unsigned long now = millis();
  
  if (currentFreq > fStart && !inDirectionHold) {
    if (!rampActive) {
      Serial.println("DBG DIR ramping down for direction change");
      startRamp(fStart);
    }
    return;
  }
  
  if (rampActive) return;
  
  if (!inDirectionHold) {
    inDirectionHold = true;
    dirHoldStartMs = now;
    updateTimerInterval(fStart);
    Serial.print("DBG DIR holding at fStart for ");
    Serial.print((int)holdStationaryMs);
    Serial.println("ms");
    return;
  }
  
  if (now - dirHoldStartMs >= holdStationaryMs) {
    curDirFwd = reqDirFwd;
    digitalWrite(dirPin, curDirFwd ? HIGH : LOW);
    
    dirChangeRequested = false;
    inDirectionHold = false;
    
    Serial.print("OK DIR ");
    Serial.println(curDirFwd ? "FWD" : "BWD");
    logResponse(curDirFwd ? "OK DIR FWD" : "OK DIR BWD");
  }
}

// ================= COMMAND HANDLERS =================
void setDirection(bool forward) {
  reqDirFwd = forward;
  
  if (curDirFwd != reqDirFwd) {
    dirChangeRequested = true;
    Serial.print("DBG DIR change requested to ");
    Serial.println(reqDirFwd ? "FWD" : "BWD");
  } else {
    Serial.print("OK DIR ");
    Serial.println(curDirFwd ? "FWD" : "BWD");
    logResponse(curDirFwd ? "OK DIR FWD" : "OK DIR BWD");
  }
}

void setSpeed(float hz) {
  if (hz <= 0) {
    startRamp(0.0);
    Serial.println("OK SPEED 0");
    logResponse("OK SPEED 0");
    return;
  }
  
  hz = constrain(hz, fStart, targetFreq);
  startRamp(hz);
  
  char resp[32];
  snprintf(resp, sizeof(resp), "OK SPEED %d", (int)hz);
  Serial.println(resp);
  logResponse(resp);
}

void stopMotor() {
  startRamp(0.0);
  dirChangeRequested = false;
  inDirectionHold = false;
  Serial.println("OK STOP");
  logResponse("OK STOP");
}

void applyConfig(String param, float value) {
  param.toUpperCase();
  
  if (param == "FSTART") {
    fStart = constrain(value, 50.0, 500.0);
    Serial.print("OK CONFIG FSTART ");
    Serial.println((int)fStart);
    logResponse("OK CONFIG FSTART");
  }
  else if (param == "MAXHZ" || param == "TARGETFREQ") {
    targetFreq = constrain(value, 100.0, 5750.0);
    Serial.print("OK CONFIG MAXHZ ");
    Serial.println((int)targetFreq);
    logResponse("OK CONFIG MAXHZ");
  }
  else if (param == "RAMP" || param == "RAMPMS") {
    rampTimeMs = constrain(value, 100.0, 10000.0);
    Serial.print("OK CONFIG RAMP ");
    Serial.println((int)rampTimeMs);
    logResponse("OK CONFIG RAMP");
  }
  else if (param == "HOLD" || param == "HOLDMS" || param == "STATIONARYMS") {
    holdStationaryMs = constrain(value, 0.0, 1000.0);
    Serial.print("OK CONFIG HOLD ");
    Serial.println((int)holdStationaryMs);
    logResponse("OK CONFIG HOLD");
  }
  else {
    errorCount++;
    Serial.print("ERR CONFIG UNKNOWN: ");
    Serial.println(param);
    logResponse("ERR CONFIG UNKNOWN");
  }
}

void sendFeedback() {
  feedbackCount++;
  Serial.print("FB DIR:");
  Serial.print(curDirFwd ? "FWD" : "BWD");
  Serial.print(" FREQ:");
  Serial.print((int)currentFreq);
  Serial.print(" STEPS:");
  Serial.println(stepsExecuted);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}  // Wait up to 3 seconds for serial
  
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  digitalWrite(stepPin, LOW);
  digitalWrite(dirPin, HIGH);
  
  updateTimerInterval(1.0);
  stepTimer.begin(stepISR, interval_us);
  
  Serial.println("========================================");
  Serial.println("READY SIMPLE DEBUG MODE");
  Serial.println("========================================");
  Serial.print("CONFIG FSTART:");
  Serial.print((int)fStart);
  Serial.print(" MAXHZ:");
  Serial.print((int)targetFreq);
  Serial.print(" RAMP:");
  Serial.print((int)rampTimeMs);
  Serial.print(" HOLD:");
  Serial.println((int)holdStationaryMs);
  Serial.println("DEBUG: All RX/TX will be logged");
  Serial.println("========================================");
}

// ================= MAIN LOOP =================
void loop() {
  unsigned long now = millis();
  
  // Handle serial commands
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    
    if (line.length() == 0) return;
    
    logCommand(line.c_str());
    
    line.toUpperCase();
    
    if (line.startsWith("DIR:")) {
      String dir = line.substring(4);
      if (dir == "FWD" || dir == "FORWARD") {
        setDirection(true);
      } else if (dir == "BWD" || dir == "BACKWARD" || dir == "BACK") {
        setDirection(false);
      } else {
        errorCount++;
        Serial.println("ERR DIR (use FWD or BWD)");
        logResponse("ERR DIR");
      }
    }
    else if (line.startsWith("SPEED:")) {
      float hz = line.substring(6).toFloat();
      setSpeed(hz);
    }
    else if (line == "STOP") {
      stopMotor();
    }
    else if (line.startsWith("CONFIG:")) {
      int idx = line.indexOf(':', 7);
      if (idx > 0) {
        String param = line.substring(7, idx);
        float value = line.substring(idx + 1).toFloat();
        applyConfig(param, value);
      } else {
        errorCount++;
        Serial.println("ERR CONFIG FORMAT (use CONFIG:PARAM:VALUE)");
        logResponse("ERR CONFIG FORMAT");
      }
    }
    else if (line == "STATUS") {
      sendFeedback();
      logState();
    }
    else if (line == "STATS") {
      printStats();
    }
    else {
      errorCount++;
      Serial.print("ERR UNKNOWN: ");
      Serial.println(line);
      logResponse("ERR UNKNOWN");
    }
  }
  
  // Update ramp if active
  updateRamp();
  
  // Handle direction changes
  handleDirectionChange();
  
  // Send periodic feedback
  if (now - lastFeedbackMs >= feedbackIntervalMs) {
    lastFeedbackMs = now;
    sendFeedback();
  }
  
  // Print stats periodically
  if (now - lastStatsMs >= statsIntervalMs) {
    lastStatsMs = now;
    printStats();
  }
}
