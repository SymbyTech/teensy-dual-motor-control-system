# Testing Guide - Dual Motor Control System

## Pre-Testing Checklist

Before powering on the system, verify:

- [ ] All wiring connections match the WIRING_GUIDE.md
- [ ] Common ground is established between all components
- [ ] DIP switches on drivers are set to 8x microstepping (SW5:ON, SW6:ON, SW7:ON, SW8:OFF)
- [ ] Motor power supply is OFF
- [ ] Single Teensy 4.1 board is connected to Raspberry Pi via USB
- [ ] Firmware is uploaded to Teensy

---

## Phase 1: Logic Testing (No Motor Power)

### Step 1.1: Upload Teensy Firmware

On your computer with Arduino IDE:

```bash
# Open main.cpp in Arduino IDE
# Set Board: Tools > Board > Teensy 4.1
# Set USB Type: Tools > USB Type > Serial
# Upload to Teensy
```

**Expected Result**: Teensy LED blinks 3 times rapidly, then heartbeat (1 blink/second)

### Step 1.2: Identify Serial Port

On Raspberry Pi:

```bash
# List connected devices
ls -l /dev/ttyACM*

# Should show:
# /dev/ttyACM0  <- Single Teensy controlling both motors
```

### Step 1.3: Test Teensy Communication

On Raspberry Pi:

```bash
# Install screen if needed
sudo apt-get install screen

# Connect to Teensy
screen /dev/ttyACM0 115200

# You should see startup message
# Type: STATUS
# Press Enter
```

**Expected Response:**
```
======== DUAL MOTOR STATUS ========
--- Motor 1 (Left/Port) ---
  Running: NO
  Current Speed: 0.00
  Target Speed: 0.00
  Direction: FORWARD
  Position: 0
  Boost Active: NO
--- Motor 2 (Right/Starboard) ---
  Running: NO
  Current Speed: 0.00
  Target Speed: 0.00
  Direction: FORWARD
  Position: 0
  Boost Active: NO
--- Sync Drift: 0 steps ---
===================================
```

**To exit screen**: Press `Ctrl+A` then `K`, then `Y`

### Step 1.4: Test Command Response

Test these commands:

| Command | Expected Response | Notes |
|---------|------------------|-------|
| `STATUS` or `?` | Shows both motors status | Basic communication check |
| `FORWARD` or `F` | "Both motors direction: FORWARD" | Sets both motors direction |
| `M1:FORWARD` | "Motor1 direction: FORWARD" | Sets Motor 1 only |
| `BACKWARD` or `B` | "Both motors direction: BACKWARD" | Sets both motors direction |
| `SPEED:1000` | "Both motors speed set to: 1000" | Sets both motors speed (max 20000 with 8x) |
| `M1:SPEED:500` | "Motor1 speed set to: 500" | Sets Motor 1 speed only |
| `SPIN:LEFT:1000` | "Spinning LEFT at 1000" | Point turn left |
| `SPIN:RIGHT:1000` | "Spinning RIGHT at 1000" | Point turn right |
| `SYNC` | "Motors synchronized" | Reset both positions to 0 |

**✓ Pass Criteria**: All commands respond correctly

---

## Phase 2: Command Testing (No Motor Power)

### Step 2.1: Set Low Speed

```
SPEED:500
FORWARD
RUN
```

**Expected Result**:
- Commands accepted
- No movement (motor power still off)
- Pins 2 and 4 on Teensy should output pulses (can verify with multimeter/oscilloscope)

**Note**: With 8x microstepping, you'll see 8 step pulses per physical motor step

### Step 2.2: Stop Test

```
STOP
```

**Expected Result**: 
- "Both motors stopped" message
- Clean response

**✓ Pass Criteria**: All commands execute without errors

---

## Phase 3: Motor Power Testing

**⚠️ WARNING**: 
- Ensure motors are not mechanically loaded
- Keep hand on emergency power switch
- Start with minimum speed

### Step 3.1: Apply Motor Power

1. Verify Teensy is in STOP mode (both motors stopped)
2. Turn ON motor power supply (24-60V)
3. Check driver status LEDs (both drivers)

**Expected Result**:
- Drivers power up (LED indicators on)
- No smoke, burning smell, or unusual sounds
- Motors may emit holding torque hum (normal)

### Step 3.2: First Movement Test - Motor 1

In the serial terminal:
```
M1:RESET
M1:SPEED:100
M1:FORWARD
M1:RUN
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
M1:STOP
```

### Step 3.3: First Movement Test - Motor 2

Test Motor 2:
```
M2:RESET
M2:SPEED:100
M2:FORWARD
M2:RUN
```

**Expected Result**: Same as Motor 1

Stop the motor:
```
M2:STOP
```

### Step 3.4: Direction Test

Test Motor 1 backward:
```
M1:RESET
M1:SPEED:100
M1:BACKWARD
M1:RUN
```

**Expected Result**: Motor 1 rotates in opposite direction

```
M1:STOP
```

Test Motor 2 backward:
```
M2:RESET
M2:SPEED:100
M2:BACKWARD
M2:RUN
```

**Expected Result**: Motor 2 rotates in opposite direction

```
M2:STOP
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

### Step 4.2: Medium Speed Test (2000-8000 steps/sec)

```
SPEED:4000
RUN
# Observe for 5 seconds
STOP

SPEED:8000
RUN
# Observe for 5 seconds
STOP
```

**Observation**:
- Motion should be very smooth with 8x microstepping
- Minimal vibration or resonance
- Check if motors get warmer (normal)

### Step 4.3: High Speed Test (8000-16000 steps/sec)

```
SPEED:12000
RUN
# Observe for 5 seconds
STOP

SPEED:16000
RUN
# Observe for 5 seconds
STOP
```

**Expected**: Smooth operation even at high speeds

### Step 4.4: Maximum Speed Test (20000 steps/sec)

**Note**: System max is 20000 steps/sec with 8x microstepping (~2500 RPM).

```
SPEED:20000
RUN
# Observe for 10 seconds
STOP
```

**Expected Behavior**:
- Very smooth operation at max speed (no pulsing or resonance)
- Motors may generate more heat (normal)
- Better torque than old full-step mode

**Warning Signs to Watch**:
- Grinding or clicking sounds (may indicate insufficient power)
- Excessive vibration (check mechanical coupling)
- Motor getting very hot quickly
- Inconsistent speed

**✓ Pass Criteria**: 
- Smooth operation across full speed range (100-20000 steps/sec)
- No stalling or resonance at any speed
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

### Step 5.2: Configure Port

Edit `motor_controller.py`:

```python
# Line ~230 - Update to match your system
TEENSY_PORT = '/dev/ttyACM0'   # Your Teensy port
```

### Step 5.3: Run Dual Control Test

```bash
python3 motor_controller.py
```

**Expected Output**:
```
============================================================
Dual Motor Control System (Single Teensy)
============================================================
✓ Connected to Teensy at /dev/ttyACM0

✓ System ready!

Commands:
  f - Move forward
  s - Move backward
  a - Spin left (point turn)
  d - Spin right (point turn)
  v - BOOST forward
  x - BOOST backward
  z - BOOST spin left
  c - BOOST spin right
  w - Stop
  e - Emergency stop
  + - Increase speed
  - - Decrease speed
  ? - Get status
  y - Sync motors
  1 - Control Motor 1 only
  2 - Control Motor 2 only
  r - Reset positions
  q - Quit
Current speed: 1000 steps/sec
Ready for commands...
```

### Step 5.4: Basic Movement Tests

Test each command:

| Command | Test | Expected Result |
|---------|------|------------------|
| `f` | Forward | Both motors rotate forward at same speed |
| `s` | Backward | Both motors rotate backward at same speed |
| `a` | Spin left | M1 backward, M2 forward (point turn) |
| `d` | Spin right | M1 forward, M2 backward (point turn) |
| `+` | Increase speed | Speed increases by 1000 steps/sec (max 20000) |
| `-` | Decrease speed | Speed decreases by 1000 steps/sec (min 100) |
| `w` | Stop | Both motors stop smoothly |
| `?` | Status | Shows status with boost and sync info |

### Step 5.5: Speed Ramp Test

Starting from stopped:

1. Press `f` (forward)
2. Press `+` five times (increase speed to 7000)
3. Observe smooth acceleration
4. Press `-` five times (decrease back to 2000)
5. Observe smooth deceleration
6. Press `w` (stop)

**✓ Pass Criteria**: 
- Both motors stay synchronized
- Smooth acceleration/deceleration
- No jittering at any speed
- Commands respond immediately

---

## Phase 6: Boost Function Testing

The boost function provides a brief high-speed "kick" to overcome static friction, then smoothly returns to target speed. This is especially useful for spinning maneuvers.

### Step 6.1: Understanding Boost

**What is Boost?**
- Temporarily increases speed by a multiplier (default 1.5x = 50% increase)
- Lasts for a configured duration (default 400ms)
- Automatically returns to target speed after boost expires
- Safely capped at MAX_SPEED (5000 steps/sec)

**Default Configuration:**
```
Boost Multiplier: 1.5     (50% speed increase)
Boost Duration:   400 ms  (0.4 second burst)
Boost Enabled:    true
```

### Step 6.2: Test Normal vs Boost Movement

**Test 1: Normal Spin Left**
```
Command: a
# Observe: Motors accelerate to 2000 steps/sec over ~0.25 seconds
# Wait 2 seconds
Command: w
```

**Test 2: Boosted Spin Left**
```
Command: z
# Observe: Motors quickly jump to 3000 steps/sec, then settle to 2000
# You should see/hear the initial speed burst for 0.4 seconds
# With 8x microstepping, this is incredibly smooth!
# Wait 2 seconds
Command: w
```

**Expected Difference:**
- Boost starts faster and reaches peak speed quickly
- Boost runs at higher speed for 0.4 seconds, then smoothly reduces
- Normal movement has gradual acceleration throughout
- Both end at same target speed

### Step 6.3: Monitor Boost Status

During a boost movement:
```
Command: z
# Immediately press ?
Command: ?
```

**Expected Output:**
```
======== DUAL MOTOR STATUS ========
--- Motor 1 (Left/Port) ---
  Running: YES
  Current Speed: 1450.00
  Target Speed: 1500.00
  Direction: BACKWARD
  Position: 523
  Boost Active: YES        ← Boost is active
--- Motor 2 (Right/Starboard) ---
  Running: YES
  Current Speed: 1450.00
  Target Speed: 1500.00
  Direction: FORWARD
  Position: 525
  Boost Active: YES        ← Boost is active
--- Sync Drift: 2 steps ---
===================================
```

Wait 0.5 seconds and check again:
```
Command: ?
```

**Expected Output:**
```
Boost Active: NO        ← Boost has expired (after 0.4 sec)
Target Speed: 1000.00   ← Returned to normal speed
```

### Step 6.4: Test All Boost Commands

| Command | Test | Expected Behavior |
|---------|------|-------------------|
| `v` | Boost forward | Quick acceleration, both forward |
| `x` | Boost backward | Quick acceleration, both backward |
| `z` | Boost spin left | Quick spin initiation |
| `c` | Boost spin right | Quick spin initiation |

### Step 6.5: Configure Boost Parameters

**Via Direct Serial Command:**
```bash
screen /dev/ttyACM0 115200

# Format: CONFIG:BOOST:multiplier:duration:enabled
CONFIG:BOOST:1.5:400:1
# Response: Boost configuration updated
```

**Via Python:**
```python
# In Python script
controller.configure_boost(
    multiplier=1.5,   # 1.0 - 2.0 (100% - 200%)
    duration=400,     # 50 - 500 milliseconds
    enabled=True      # True/False
)
```

### Step 6.6: Test Different Boost Configurations

**Conservative Boost (gentle):**
```
CONFIG:BOOST:1.2:100:1
z
# Observe: 20% boost for 0.1 seconds
w
```

**Aggressive Boost (strong):**
```
CONFIG:BOOST:1.8:300:1
z
# Observe: 80% boost for 0.3 seconds
w
```

**Disabled Boost:**
```
CONFIG:BOOST:1.0:0:0
z
# Observe: Should behave like normal 'a' command
w
```

**Reset to Default:**
```
CONFIG:BOOST:1.5:400:1
```

### Step 6.7: Tuning Guide

**If spins are sluggish:**
- Increase multiplier: `CONFIG:BOOST:1.7:400:1`
- Increase duration: `CONFIG:BOOST:1.5:500:1`

**If spins are too aggressive:**
- Decrease multiplier: `CONFIG:BOOST:1.3:400:1`
- Decrease duration: `CONFIG:BOOST:1.5:250:1`

**If boost causes motor stalling:**
- Reduce multiplier: `CONFIG:BOOST:1.2:400:1`
- Check power supply capacity
- Verify driver current settings

**If boost doesn't seem helpful:**
- Try longer duration: `CONFIG:BOOST:1.5:500:1`
- Increase multiplier: `CONFIG:BOOST:1.8:400:1`
- Or disable it: `CONFIG:BOOST:1.0:0:0`

### Step 6.8: Safety Verification

Test that boost respects speed limits:
```
# Set very high boost that would exceed 5000
CONFIG:BOOST:10.0:500:1

SPEED:3000
z
?  # Check status immediately
```

**Expected:**
- Current Speed should NEVER exceed 5000 steps/sec
- System automatically caps at MAX_SPEED
- No damage or instability

**Reset to safe values:**
```
CONFIG:BOOST:1.5:400:1
```

**✓ Pass Criteria**: 
- Boost provides noticeable initial speed increase
- Motors smoothly return to target speed after boost
- Boost respects speed limits (never exceeds 5000)
- Configuration changes take effect immediately
- System remains stable with different boost settings

---

## Phase 7: Synchronization Testing

### Step 7.1: Test Sync Monitoring

Run motors for extended period and monitor drift:
```
Command: f
# Let run for 30 seconds
Command: ?
```

**Check Sync Drift:**
```
--- Sync Drift: 45 steps ---
```

**Acceptable Range:**
- <100 steps: Excellent synchronization
- 100-200 steps: Good, may need adjustment
- >200 steps: Check mechanical issues

### Step 7.2: Test Manual Sync

```
Command: y  (sync motors)
# Response: Synchronizing motors...

Command: ?
```

**Expected:**
```
--- Motor 1 (Left/Port) ---
  Position: 0           ← Reset to 0
--- Motor 2 (Right/Starboard) ---
  Position: 0           ← Reset to 0
--- Sync Drift: 0 steps ---
```

### Step 7.3: Sync Under Load Test

Run aggressive movements and check sync:
```
Command: z  (boost spin left)
# Wait 2 seconds
Command: w
Command: c  (boost spin right)
# Wait 2 seconds
Command: w
Command: f  (forward)
# Wait 5 seconds
Command: w
Command: ?
```

**Check sync drift** - should still be <100 steps

**✓ Pass Criteria**: 
- Sync drift stays below 100 steps during normal operation
- Manual sync (y) resets both positions to 0
- System alerts if drift exceeds 100 steps
- Motors maintain synchronization during aggressive maneuvers

---

## Phase 8: Anti-Jitter Tuning

If you experience jittering at certain speeds, try these adjustments:

### Step 8.1: Adjust Acceleration Rate

In `main.cpp`, line ~34:

```cpp
#define ACCEL_RATE 5000       // Steps/second^2 acceleration
```

**For less jitter**: Decrease to 2000-3000
**For faster response**: Increase to 8000-10000

Re-upload firmware and test.

### Step 8.2: Adjust Microstep Setting

On driver DIP switches:
- **More smooth, less torque**: Increase microsteps (16 or 32)
- **More torque, may jitter**: Decrease microsteps (4 or 2)

### Step 8.3: Adjust Driver Current

If motors are underpowered:
1. Increase driver current setting (refer to driver manual)
2. Should be ~80-90% of motor rated current (4.8-5.4A for 6A motors)

### Step 8.4: Resonance Avoidance

Some speed ranges may cause mechanical resonance. Solutions:
- Avoid specific speed ranges
- Add mechanical damping
- Adjust acceleration profiles

**✓ Pass Criteria**: 
- Smooth operation from 100 to 5000 steps/sec
- No visible jittering
- Consistent performance

---

## Phase 9: Endurance Testing

### Step 9.1: Extended Run Test

Run motors continuously for 10 minutes:

```bash
python3 motor_controller.py

Command: f
# Let run for 2 minutes

Command: +
# Increase speed to 1500

# Let run for 2 minutes

Command: +
# Increase speed to 2000

# Let run for 2 minutes at different speeds
```

**Monitor**:
- Motor temperature (should be warm but not burning hot)
- Driver temperature
- Any changes in sound or vibration
- Position accuracy (STATUS command)

### Step 9.2: Cycle Test

Automate start/stop cycles:

```python
# Create test script: cycle_test.py
import time
from motor_controller import DualMotorController

controller = DualMotorController('/dev/ttyACM0')
controller.connect()

for i in range(50):
    print(f"Cycle {i+1}/50")
    controller.move_forward(2000)
    time.sleep(2)
    controller.stop_all()
    time.sleep(1)
    controller.move_backward(2000)
    time.sleep(2)
    controller.stop_all()
    time.sleep(1)

controller.disconnect()
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
| Max smooth speed | 5000 steps/sec | __________ |
| Min stable speed | 100 steps/sec | __________ |
| Acceleration time (0-5000) | < 1.5 seconds | __________ |
| Position accuracy (100 rev) | ±5 steps | __________ |
| Synchronization drift | < 100 steps | __________ |
| Boost effectiveness | Noticeable | __________ |

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

## Quick Command Reference

### Python CLI Commands

| Key | Action | Key | Action |
|-----|--------|-----|--------|
| `f` | Forward | `v` | BOOST forward |
| `s` | Backward | `x` | BOOST backward |
| `a` | Spin left | `z` | BOOST spin left |
| `d` | Spin right | `c` | BOOST spin right |
| `w` | Stop | `e` | Emergency stop |
| `+` | Speed up | `-` | Speed down |
| `?` | Status | `y` | Sync motors |
| `r` | Reset | `1` | Motor 1 mode |
| `2` | Motor 2 mode | `q` | Quit |

### Direct Serial Commands

| Command | Example | Description |
|---------|---------|-------------|
| `SPEED:X` | `SPEED:1000` | Set both motors speed (0-5000) |
| `M1:SPEED:X` | `M1:SPEED:500` | Set Motor 1 speed only |
| `M2:SPEED:X` | `M2:SPEED:1500` | Set Motor 2 speed only |
| `FORWARD` or `F` | `FORWARD` | Both motors forward |
| `BACKWARD` or `B` | `BACKWARD` | Both motors backward |
| `SPIN:LEFT:X` | `SPIN:LEFT:1000` | Point turn left at X steps/sec |
| `SPIN:RIGHT:X` | `SPIN:RIGHT:1000` | Point turn right at X steps/sec |
| `BOOST:LEFT:X` | `BOOST:LEFT:1000` | Boosted spin left |
| `BOOST:RIGHT:X` | `BOOST:RIGHT:1000` | Boosted spin right |
| `BOOST:FORWARD:X` | `BOOST:FORWARD:2000` | Boosted forward |
| `BOOST:BACKWARD:X` | `BOOST:BACKWARD:2000` | Boosted backward |
| `RUN` or `R` | `RUN` | Start motor(s) |
| `STOP` or `X` | `STOP` | Stop motor(s) smoothly |
| `ESTOP` or `E` | `ESTOP` | Emergency stop all |
| `STATUS` or `?` | `STATUS` | Get full status |
| `SYNC` | `SYNC` | Synchronize positions |
| `RESET` | `RESET` | Reset positions to 0 |
| `CONFIG:BOOST:M:D:E` | `CONFIG:BOOST:1.5:200:1` | Configure boost (multiplier:duration:enabled) |

### Boost Configuration

**Format:** `CONFIG:BOOST:multiplier:duration:enabled`

| Parameter | Range | Example | Description |
|-----------|-------|---------|-------------|
| multiplier | 1.0 - 2.0 | 1.5 | Speed multiplier (1.5 = 50% increase) |
| duration | 50 - 500 | 200 | Boost duration in milliseconds |
| enabled | 0 or 1 | 1 | 1=enabled, 0=disabled |

**Examples:**
- Default: `CONFIG:BOOST:1.5:400:1`
- Conservative: `CONFIG:BOOST:1.2:200:1`
- Aggressive: `CONFIG:BOOST:1.8:500:1`
- Disabled: `CONFIG:BOOST:1.0:0:0`

### System Specifications

- **Max Speed**: 20000 steps/sec with 8x microstepping (~2500 RPM)
- **Min Speed**: 100 steps/sec (~4 RPM)
- **Acceleration**: 8000 steps/sec²
- **Step Mode**: 8x microstepping (1600 microsteps/rev)
- **Sync Threshold**: 100 steps drift alert
- **Default Boost**: 1.5x for 400ms
- **DIP Switches**: SW5:ON, SW6:ON, SW7:ON, SW8:OFF

---

**Safety First**: Always keep motor power switch within reach during testing!
