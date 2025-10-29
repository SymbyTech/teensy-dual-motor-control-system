# Testing Guide - Dual Motor Control System

## Pre-Testing Checklist

Before powering on the system, verify:

- [ ] All wiring connections match the WIRING_GUIDE.md
- [ ] Common ground is established between all components
- [ ] DIP switches on drivers are set correctly (8 microsteps recommended)
- [ ] Motor power supply is OFF
- [ ] Both Teensy 4.1 boards are connected to Raspberry Pi via USB
- [ ] Firmware is uploaded to both Teensy boards

---

## Phase 1: Logic Testing (No Motor Power)

### Step 1.1: Upload Teensy Firmware

On your computer with Arduino IDE:

```bash
# Open teensy_motor_control.ino in Arduino IDE
# Set Board: Tools > Board > Teensy 4.1
# Set USB Type: Tools > USB Type > Serial
# Upload to first Teensy
# Disconnect and repeat for second Teensy
```

**Expected Result**: Teensy LED blinks 3 times rapidly, then heartbeat (1 blink/second)

### Step 1.2: Identify Serial Ports

On Raspberry Pi:

```bash
# List connected devices
ls -l /dev/ttyACM*

# Should show:
# /dev/ttyACM0
# /dev/ttyACM1
```

### Step 1.3: Test Individual Teensy Communication

Open two terminal windows on Raspberry Pi:

**Terminal 1 (Left Motor):**
```bash
# Install screen if needed
sudo apt-get install screen

# Connect to first Teensy
screen /dev/ttyACM0 115200

# You should see startup message
# Type: STATUS
# Press Enter
```

**Expected Response:**
```
--- Motor Status ---
Motor ID: 1
Running: NO
Current Speed: 0.00
Target Speed: 0.00
Direction: FORWARD
Position: 0
-------------------
```

**Terminal 2 (Right Motor):**
```bash
# Connect to second Teensy
screen /dev/ttyACM1 115200

# Type: STATUS
# Press Enter
```

**Expected Response:**
```
--- Motor Status ---
Motor ID: 1
Running: NO
Current Speed: 0.00
Target Speed: 0.00
Direction: FORWARD
Position: 0
-------------------
```

**To exit screen**: Press `Ctrl+A` then `K`, then `Y`

### Step 1.4: Test Command Response

For each Teensy, test these commands:

| Command | Expected Response | Notes |
|---------|------------------|-------|
| `STATUS` or `?` | Shows motor status | Basic communication check |
| `FORWARD` or `F` | "Direction: FORWARD" | Sets direction |
| `BACKWARD` or `B` | "Direction: BACKWARD" | Sets direction |
| `SPEED:1000` or `S:1000` | "Speed set to: 1000.00" | Sets speed |
| `ENABLE` | "Driver enabled" | Enables driver |

**✓ Pass Criteria**: All commands respond correctly

---

## Phase 2: Driver Enable Testing (No Motor Power)

### Step 2.1: Enable Drivers

In each serial terminal:

```
ENABLE
```

**Expected Result**: 
- Teensy LED stays solid or blinks slowly
- Driver status LED turns on (if equipped)
- No error messages

### Step 2.2: Set Low Speed

```
SPEED:500
FORWARD
RUN
```

**Expected Result**:
- Commands accepted
- No movement (motor power still off)
- Pin 2 on Teensy should output pulses (can verify with multimeter/oscilloscope)

### Step 2.3: Stop Test

```
STOP
```

**Expected Result**: 
- "Motor stopped" message
- Clean response

**✓ Pass Criteria**: All commands execute without errors

---

## Phase 3: Motor Power Testing

**⚠️ WARNING**: 
- Ensure motors are not mechanically loaded
- Keep hand on emergency power switch
- Start with minimum speed

### Step 3.1: Apply Motor Power

1. Verify both Teensy are in STOP mode
2. Turn ON motor power supply (24-60V)
3. Check driver status LEDs

**Expected Result**:
- Drivers power up (LED indicators on)
- No smoke, burning smell, or unusual sounds
- Motors may emit holding torque hum (normal)

### Step 3.2: First Movement Test - Motor 1

**Terminal 1 (Left Motor):**
```
RESET
ENABLE
SPEED:100
FORWARD
RUN
```

**Expected Result**:
- Motor rotates slowly clockwise (viewed from shaft end)
- Smooth, consistent rotation
- No grinding, clicking, or skipping

**Observation Check**:
- [ ] Motor rotates smoothly
- [ ] Direction is consistent
- [ ] No unusual vibration
- [ ] No excessive heat immediately

Stop the motor:
```
STOP
```

### Step 3.3: First Movement Test - Motor 2

Repeat Step 3.2 for Motor 2 in Terminal 2

### Step 3.4: Direction Test

For each motor:

```
RESET
SPEED:100
BACKWARD
RUN
```

**Expected Result**: Motor rotates in opposite direction

```
STOP
```

**✓ Pass Criteria**: 
- Both motors rotate smoothly in both directions
- No mechanical issues detected

---

## Phase 4: Speed Range Testing

### Step 4.1: Low Speed Test (100-1000 steps/sec)

For each motor, test incrementally:

```
SPEED:100
RUN
# Observe for 5 seconds
STOP

SPEED:500
RUN
# Observe for 5 seconds
STOP

SPEED:1000
RUN
# Observe for 5 seconds
STOP
```

**Expected Result**:
- Smooth acceleration/deceleration
- No jittering or stalling
- Consistent speed

### Step 4.2: Medium Speed Test (1000-5000 steps/sec)

```
SPEED:2000
RUN
# Observe for 5 seconds
STOP

SPEED:5000
RUN
# Observe for 5 seconds
STOP
```

**Observation**:
- Note any vibration or resonance frequencies
- Check if motors get warmer (normal)

### Step 4.3: High Speed Test (5000-15000 steps/sec)

Incrementally test higher speeds:

```
SPEED:8000
RUN
# Observe
STOP

SPEED:10000
RUN
# Observe
STOP

SPEED:15000
RUN
# Observe
STOP
```

**Warning Signs to Watch**:
- Grinding or clicking sounds (may indicate stalling)
- Excessive vibration (resonance)
- Motor getting very hot quickly
- Inconsistent speed

### Step 4.4: Maximum Speed Test

```
SPEED:20000
RUN
# Observe carefully
STOP
```

**Note**: At maximum speed, motors may:
- Generate more heat
- Have reduced torque
- Show slight vibration

**✓ Pass Criteria**: 
- Smooth operation across full speed range
- No stalling at any speed
- Acceleration/deceleration is gradual

---

## Phase 5: Dual Motor Synchronization

### Step 5.1: Install Python Controller

On Raspberry Pi:

```bash
cd ~/
git clone <your-repo> dual-motor-control
cd dual-motor-control/raspberry_pi_control

# Install dependencies
pip3 install -r requirements.txt

# Make executable
chmod +x motor_controller.py
```

### Step 5.2: Configure Ports

Edit `motor_controller.py`:

```python
# Line ~370 - Update these to match your system
LEFT_MOTOR_PORT = '/dev/ttyACM0'   # Your left Teensy port
RIGHT_MOTOR_PORT = '/dev/ttyACM1'  # Your right Teensy port
```

### Step 5.3: Run Dual Control Test

```bash
python3 motor_controller.py
```

**Expected Output**:
```
============================================================
Dual Motor Control System
============================================================
Motor 1: Connected to /dev/ttyACM0
Motor 2: Connected to /dev/ttyACM1

Motors connected successfully!

Commands:
  w - Move forward
  s - Move backward
  ...
Current speed: 1000 steps/sec
Ready for commands...
```

### Step 5.4: Synchronized Movement Tests

Test each command:

| Command | Test | Expected Result |
|---------|------|-----------------|
| `w` | Forward | Both motors rotate forward at same speed |
| `s` | Backward | Both motors rotate backward at same speed |
| `a` | Turn left | Left motor backward, right forward |
| `d` | Turn right | Left motor forward, right backward |
| `+` | Increase speed | Speed increases by 500 steps/sec |
| `-` | Decrease speed | Speed decreases by 500 steps/sec |
| `x` | Stop | Both motors stop smoothly |
| `?` | Status | Shows status of both motors |

### Step 5.5: Speed Ramp Test

Starting from stopped:

1. Press `w` (forward)
2. Press `+` five times (increase speed to 3500)
3. Observe smooth acceleration
4. Press `-` five times (decrease back to 1000)
5. Observe smooth deceleration
6. Press `x` (stop)

**✓ Pass Criteria**: 
- Both motors stay synchronized
- Smooth acceleration/deceleration
- No jittering at any speed
- Commands respond immediately

---

## Phase 6: Anti-Jitter Tuning

If you experience jittering at certain speeds, try these adjustments:

### 6.1: Adjust Acceleration Rate

In `teensy_motor_control.ino`, line ~36:

```cpp
#define ACCEL_RATE 5000       // Steps/second^2 acceleration
```

**For less jitter**: Decrease to 2000-3000
**For faster response**: Increase to 8000-10000

Re-upload firmware and test.

### 6.2: Adjust Microstep Setting

On driver DIP switches:
- **More smooth, less torque**: Increase microsteps (16 or 32)
- **More torque, may jitter**: Decrease microsteps (4 or 2)

### 6.3: Adjust Driver Current

If motors are underpowered:
1. Increase driver current setting (refer to driver manual)
2. Should be ~80-90% of motor rated current (4.8-5.4A for 6A motors)

### 6.4: Resonance Avoidance

Some speed ranges may cause mechanical resonance. Solutions:
- Avoid specific speed ranges
- Add mechanical damping
- Adjust acceleration profiles

**✓ Pass Criteria**: 
- Smooth operation from 100 to 20000 steps/sec
- No visible jittering
- Consistent performance

---

## Phase 7: Endurance Testing

### Step 7.1: Extended Run Test

Run motors continuously for 10 minutes:

```bash
python3 motor_controller.py

Command: w
# Let run for 2 minutes

Command: +
# Increase speed

# Let run for 2 minutes each at different speeds
```

**Monitor**:
- Motor temperature (should be warm but not burning hot)
- Driver temperature
- Any changes in sound or vibration
- Position accuracy (STATUS command)

### Step 7.2: Cycle Test

Automate start/stop cycles:

```python
# Create test script: cycle_test.py
import time
from motor_controller import DualMotorController

controller = DualMotorController('/dev/ttyACM0', '/dev/ttyACM1')
controller.connect_all()

for i in range(50):
    print(f"Cycle {i+1}/50")
    controller.move_forward(5000)
    time.sleep(2)
    controller.stop_all()
    time.sleep(1)
    controller.move_backward(5000)
    time.sleep(2)
    controller.stop_all()
    time.sleep(1)

controller.disconnect_all()
print("Cycle test complete!")
```

Run:
```bash
python3 cycle_test.py
```

**✓ Pass Criteria**:
- Consistent performance over 50 cycles
- No degradation in smoothness
- No overheating

---

## Troubleshooting

### Issue: Motors jitter at low speeds

**Solutions**:
1. Increase microstep resolution (DIP switches)
2. Reduce acceleration rate in firmware
3. Check motor power supply ripple

### Issue: Motors stall at high speeds

**Solutions**:
1. Reduce maximum speed
2. Increase motor power supply voltage (within limits)
3. Reduce mechanical load
4. Check driver current setting

### Issue: Inconsistent speed between motors

**Solutions**:
1. Verify identical DIP switch settings on both drivers
2. Check power supply capacity
3. Verify ground connections
4. Swap motors to isolate issue

### Issue: Communication errors

**Solutions**:
1. Check USB cable quality
2. Try different USB ports on Raspberry Pi
3. Reduce USB cable length
4. Add ferrite beads to USB cables

### Issue: Emergency Stop doesn't work

**Solutions**:
1. Verify emergency stop command reaches Teensy
2. Check driver enable pin wiring
3. Use hardware emergency switch on power supply

---

## Performance Benchmarks

After successful testing, record your system's performance:

| Metric | Target | Your Result |
|--------|--------|-------------|
| Max smooth speed | 15000+ steps/sec | __________ |
| Min stable speed | 100 steps/sec | __________ |
| Acceleration time (0-10000) | < 3 seconds | __________ |
| Position accuracy (100 rev) | ±5 steps | __________ |
| Synchronization error | < 1% | __________ |

---

## Final Validation

- [ ] Both motors operate smoothly across full speed range
- [ ] No jittering at any speed
- [ ] Synchronized dual motor control works correctly
- [ ] Emergency stop functions properly
- [ ] System runs for 10+ minutes without issues
- [ ] All safety measures are in place

**✅ System is ready for deployment!**

---

## Next Steps

1. Integrate into your application
2. Implement additional features (position control, profiles, etc.)
3. Add safety interlocks for your specific application
4. Create backup configuration files
5. Document any custom modifications

---

## Emergency Procedures

### If Motor Runs Away:
1. Press `e` for emergency stop in software
2. If no response, kill power immediately
3. Check wiring and direction settings

### If Driver Overheats:
1. Stop motor immediately
2. Check current settings
3. Improve cooling/ventilation
4. Verify motor is not mechanically bound

### If Communication Lost:
1. Stop motors via power switch
2. Check USB connections
3. Restart Raspberry Pi
4. Re-establish connections

---

**Safety First**: Always keep motor power switch within reach during testing!
