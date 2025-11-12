# 8x Microstepping Hardware Setup Guide

## ⚠️ IMPORTANT: Complete Before Testing

The firmware is now configured for **8x microstepping**. You **MUST** update both motor drivers before uploading and testing.

---

## DIP Switch Configuration

### DQ860HA-V3.3 Driver Settings

Located on the side of each driver, you'll find 8 DIP switches (SW1-SW8).

**For 8x Microstepping:**

| Switch | Position | Purpose |
|--------|----------|---------|
| SW1 | OFF | Current setting |
| SW2 | OFF | Current setting |
| SW3 | OFF | Current setting |
| SW4 | OFF | Current setting |
| **SW5** | **ON** | **Microstep config** |
| **SW6** | **ON** | **Microstep config** |
| **SW7** | **ON** | **Microstep config** |
| **SW8** | **OFF** | **Microstep config** |

**SW5-SW8 Lookup Table:**

| SW5 | SW6 | SW7 | SW8 | Microsteps | Use Case |
|-----|-----|-----|-----|------------|----------|
| OFF | OFF | OFF | OFF | 1 (full-step) | Old config - caused pulsing |
| ON  | OFF | OFF | OFF | 2 | - |
| OFF | ON  | OFF | OFF | 4 | - |
| ON  | ON  | OFF | OFF | 8 | - |
| OFF | OFF | ON  | OFF | 16 | - |
| **ON**  | **ON**  | **ON**  | **OFF** | **8** | **← NEW CONFIG (recommended)** |
| OFF | ON  | ON  | OFF | 32 | Extreme smoothness, low torque |

---

## Setup Procedure

### Step 1: Power Down Everything
```
1. Turn OFF motor power supply (48V DC)
2. Disconnect Teensy USB from Raspberry Pi
3. Wait 30 seconds for capacitors to discharge
```

### Step 2: Configure Driver 1 (Motor 1 - Left)
```
1. Locate DIP switches on Driver 1
2. Set switches:
   - SW5: ON
   - SW6: ON
   - SW7: ON
   - SW8: OFF
3. Double-check all switch positions
```

### Step 3: Configure Driver 2 (Motor 2 - Right)
```
1. Locate DIP switches on Driver 2
2. Set switches (MUST match Driver 1):
   - SW5: ON
   - SW6: ON
   - SW7: ON
   - SW8: OFF
3. Double-check all switch positions
```

### Step 4: Verify Configuration
```
✓ Both drivers have IDENTICAL switch settings
✓ SW5, SW6, SW7 are ON
✓ SW8 is OFF
✓ All other switches remain in their original position
```

### Step 5: Upload New Firmware
```
1. Connect Teensy to your computer via USB
2. Open main.cpp in Arduino IDE
3. Verify these settings in code:
   - MICROSTEPS 8
   - MAX_SPEED 20000
   - ACCEL_RATE 8000
4. Upload to Teensy 4.1
```

### Step 6: Power Up and Test
```
1. Connect Teensy to Raspberry Pi
2. Turn ON motor power supply
3. Run Python controller
4. Test at low speeds first (2000-4000 steps/sec)
5. Gradually increase to verify smooth operation
```

---

## What Changed vs Old Configuration

### Old Config (Full-Step, 1x)
```
DIP Switches: SW5:OFF, SW6:OFF, SW7:OFF, SW8:OFF
Max Speed: 5000 steps/sec
Issues: Pulsing above 2000, resonance, direction change problems
```

### New Config (8x Microstepping)
```
DIP Switches: SW5:ON, SW6:ON, SW7:ON, SW8:OFF
Max Speed: 20000 steps/sec
Benefits: Smooth operation, no resonance, 4x speed capability
```

---

## Speed Conversion Reference

To achieve the same RPM in the new configuration, multiply your old step rate by 8:

| Old Speed (1x) | New Speed (8x) | RPM | Use Case |
|---------------|---------------|-----|----------|
| 500 steps/sec | 4000 steps/sec | 150 | Slow, precise |
| 1000 steps/sec | 8000 steps/sec | 300 | Medium |
| 2000 steps/sec | 16000 steps/sec | 600 | Fast (old max smooth) |
| 5000 steps/sec | 40000 steps/sec | 1500 | Very fast (would pulse in old config) |

**Current system max:** 20000 steps/sec = ~750 RPM (still faster than old 2000 limit!)

---

## Verification Tests

After setup, run these tests:

### Test 1: Low Speed Smooth Operation
```python
python3 motor_controller.py
Command: f
Command: +  # 3000 steps/sec
# Should be perfectly smooth, no noise
Command: w
```

### Test 2: High Speed Test
```
Command: f
Command: + + + + +  # 7000 steps/sec
# Should still be smooth
Command: w
```

### Test 3: Direction Change
```
Command: f
Command: + + +  # 5000 steps/sec
Command: s  # Change to backward
# Should auto-slow, change, then speed up
Command: w
```

### Test 4: Boost Function
```
Command: z  # Boosted spin
# Should reach 3000 steps/sec instantly, then settle to 2000
Command: w
```

---

## Troubleshooting

### Motors not moving after DIP switch change
- **Cause:** Drivers need power cycle after configuration change
- **Fix:** Turn motor power OFF for 10 seconds, then back ON

### Motors moving 8x slower than expected
- **Cause:** DIP switches set correctly but firmware not updated
- **Fix:** Re-upload firmware with `MICROSTEPS 8`

### One motor smooth, other pulsing
- **Cause:** DIP switches don't match between drivers
- **Fix:** Verify both drivers have IDENTICAL SW5-SW8 settings

### Still getting resonance at high speeds
- **Cause:** May need different microstepping level
- **Try:** 16x microstepping (SW5:OFF, SW6:OFF, SW7:ON, SW8:OFF)

### Motors losing steps or stalling
- **Cause:** Insufficient current setting
- **Check:** Driver current potentiometer set to 4.8-5.4A (80-90% of motor rating)

---

## Why 8x Microstepping?

### Benefits
✅ **Eliminates resonance** - No more pulsing sounds  
✅ **Smoother motion** - 8 steps instead of 1 large jump  
✅ **Higher usable speeds** - Can run faster without issues  
✅ **Better direction changes** - Less mechanical stress  
✅ **Quieter operation** - Significantly reduced noise  

### Trade-offs
⚠️ **Slightly lower torque** - But still more than adequate for your application  
⚠️ **Higher step rates needed** - But Teensy can easily handle this  

---

## Quick Reference Card

**Print this and keep near your setup:**

```
╔══════════════════════════════════════╗
║  8x MICROSTEPPING CONFIGURATION      ║
╠══════════════════════════════════════╣
║  DIP Switches (SW5-SW8):             ║
║    SW5: ON                           ║
║    SW6: ON                           ║
║    SW7: ON                           ║
║    SW8: OFF                          ║
╠══════════════════════════════════════╣
║  Firmware Settings:                  ║
║    MICROSTEPS: 8                     ║
║    MAX_SPEED: 20000                  ║
║    ACCEL_RATE: 8000                  ║
╠══════════════════════════════════════╣
║  Speed Range:                        ║
║    Min: 100 steps/sec                ║
║    Max: 20000 steps/sec              ║
║    Recommended: 2000-16000           ║
╚══════════════════════════════════════╝
```

---

**✅ Once both drivers are configured and firmware is uploaded, you're ready to test!**
