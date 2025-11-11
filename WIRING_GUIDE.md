# Wiring Guide - Dual Motor Control System

## System Overview

This system controls two Wantai 85BYGH450C-060 stepper motors using:
- **Controller**: Raspberry Pi 4
- **Motor Controller**: 1x Teensy 4.1 board (controls both motors)
- **Drivers**: 2x DQ860HA-V3.3 stepper drivers

---

## Component Specifications

### Stepper Motor: Wantai 85BYGH450C-060
- **Type**: Hybrid stepper motor
- **Step Angle**: 1.8° (200 steps/revolution)
- **Holding Torque**: 4.5 Nm
- **Current**: 6.0A
- **Voltage**: 24-60V DC recommended

### Driver: DQ860HA-V3.3
- **Input Voltage**: 24-80V DC
- **Output Current**: 0.5-8.0A (adjustable)
- **Max Pulse Frequency**: 400 kHz
- **Control Signals**: PUL (pulse/PWM), DIR (direction), ENA (enable)
- **Microstep Settings**: 1, 2, 4, 8, 16, 32, 64, 128 (DIP switches)

---

## Pin Connections

### Single Teensy 4.1 (Controls Both Motors)

| Teensy Pin | Function | Connects To |
|------------|----------|-------------|
| **Motor 1 (Left/Port)** |
| Pin 2 | PWM/STEP Output | Driver 1 - PUL+ |
| Pin 3 | DIR Output | Driver 1 - DIR+ |
| **Motor 2 (Right/Starboard)** |
| Pin 4 | PWM/STEP Output | Driver 2 - PUL+ |
| Pin 5 | DIR Output | Driver 2 - DIR+ |
| **Common Connections** |
| GND | Ground | Both drivers PUL-, DIR- |
| USB | Serial to RPi | Raspberry Pi USB port |
| VIN | Power Input | 5V supply |
| GND | Ground | Common ground |

### DQ860HA-V3.3 Driver Connections

#### Driver 1 Control Signals (Motor 1 - Left)
| Driver Terminal | Signal | Source |
|-----------------|--------|--------|
| PUL+ | Step Pulse | Teensy Pin 2 |
| PUL- | Ground | Teensy GND |
| DIR+ | Direction | Teensy Pin 3 |
| DIR- | Ground | Teensy GND |
| ENA+ | (Not used) | Tie per driver manual for always-enabled |
| ENA- | (Not used) | Tie per driver manual |

#### Driver 2 Control Signals (Motor 2 - Right)
| Driver Terminal | Signal | Source |
|-----------------|--------|--------|
| PUL+ | Step Pulse | Teensy Pin 4 |
| PUL- | Ground | Teensy GND |
| DIR+ | Direction | Teensy Pin 5 |
| DIR- | Ground | Teensy GND |
| ENA+ | (Not used) | Tie per driver manual for always-enabled |
| ENA- | (Not used) | Tie per driver manual |

#### Driver Power Connections (Both Drivers)
| Driver Terminal | Connects To |
|-----------------|-------------|
| VCC | +24V to +60V DC (motor power supply) |
| GND | Power supply ground |

#### Motor Connections (4-wire bipolar)
| Driver Terminal | Motor Wire |
|-----------------|------------|
| A+ | Motor Phase A+ (typically red) |
| A- | Motor Phase A- (typically green) |
| B+ | Motor Phase B+ (typically blue) |
| B- | Motor Phase B- (typically black) |

**Note**: Wire colors may vary. Refer to your motor datasheet or test for correct phase pairing.

---

## Power Supply Requirements

### Logic Power (5V)
- **Source**: USB power from Raspberry Pi or external 5V supply
- **Load**: 
  - Single Teensy 4.1: ~100mA
  - **Total**: ~150mA minimum
- **Recommended**: 5V 500mA supply (USB from RPi is sufficient)

### Motor Power (24-60V DC)
- **Source**: Dedicated high-current DC power supply
- **Requirements**:
  - Voltage: 24-60V DC (48V recommended for best performance)
  - Current: Minimum 12A (6A per motor at full load)
  - **Recommended**: 48V 15A switching power supply

**⚠️ IMPORTANT**: Keep logic power (5V) and motor power (24-60V) completely separate!

---

## Ground Connections

All grounds must be connected together:
- Raspberry Pi ground
- Teensy ground
- Both driver grounds (PUL-, DIR-)
- Motor power supply ground

This creates a common ground reference for all signals.

---

## DIP Switch Configuration (DQ860HA-V3.3)

The driver has 8 DIP switches for microstep configuration:

### Recommended Setting: Full Step (1×)
| SW1 | SW2 | SW3 | SW4 | SW5 | SW6 | SW7 | SW8 |
|-----|-----|-----|-----|-----|-----|-----|-----|
| ON  | ON  | ON  | x   | Set current per motor | Set current per motor | Set current per motor | x |

**Microstep options** (SW1-SW3):
- SW1=ON, SW2=ON, SW3=ON: 1× (full step, 200 steps/rev) ← **Recommended**
- SW1=OFF, SW2=ON, SW3=ON: 8× (1600 steps/rev)
- SW1=ON, SW2=OFF, SW3=ON: 16× (3200 steps/rev)
- SW1=ON, SW2=ON, SW3=OFF: 32× (6400 steps/rev)

**Current Setting** (SW5-SW8): See driver manual for your motor's current rating.

---

## Serial Port Identification

### On Raspberry Pi

After connecting the Teensy board via USB:

```bash
# List all connected USB serial devices
ls -l /dev/ttyACM*

# Typically will show:
# /dev/ttyACM0  <- Teensy 4.1 (controls both motors)

# To verify the connection:
udevadm info -a -n /dev/ttyACM0 | grep serial
```

---

## Safety Considerations

### ⚠️ CRITICAL SAFETY RULES

1. **Power Sequence**:
   - Connect all wiring FIRST
   - Connect logic power (5V) SECOND
   - Connect motor power (24-60V) LAST
   
2. **Emergency Stop**:
   - Keep motor power supply switch easily accessible
   - Software emergency stop available in control program
   
3. **Current Limiting**:
   - Set driver current limit to motor rating (6.0A for 85BYGH450C-060)
   - Start with lower current and increase if needed
   
4. **Heat Management**:
   - Drivers and motors will get hot during operation
   - Ensure adequate ventilation
   - Consider heatsinks on drivers
   
5. **Electrical Isolation**:
   - Never hot-plug motor connections
   - Discharge capacitors before handling

---

## Common Ground Connection Diagram

```
Raspberry Pi GND ──┬── Teensy GND ──┬── Driver 1 (PUL-, DIR-)
                   │                │
                   │                └── Driver 2 (PUL-, DIR-)
                   │
                   └── Motor Power Supply GND
```

---

## Troubleshooting Wiring Issues

### Motors not moving
- Check motor power supply voltage and connections
- If using ENA, ensure the driver's enable lines are tied for always-enabled per the driver manual (in this design ENA is not controlled by the Teensy)
- Check PUL/DIR signal connections
- Verify DIP switch settings

### Erratic movement / jitters
- Check ground connections (most common issue)
- Reduce pulse frequency (speed)
- Check motor power supply capacity
- Verify driver current settings

### One motor works, other doesn't
- Check individual driver connections (Pin 2/3 vs Pin 4/5)
- Verify both drivers are powered
- Test each motor individually using M1: or M2: commands

### No serial communication
- Check USB cable connections
- Verify correct /dev/ttyACM port
- Check baud rate settings (115200)
- Try reconnecting USB cables

---

## Next Steps

After completing wiring:
1. Double-check all connections against this guide
2. Set driver DIP switches to recommended settings
3. Connect logic power only (no motor power yet)
4. Upload firmware to both Teensy boards
5. Test serial communication from Raspberry Pi
6. Connect motor power
7. Follow TESTING_GUIDE.md for calibration

---

## Additional Resources

- [Teensy 4.1 Pinout](https://www.pjrc.com/store/teensy41.html)
- [DQ860HA Driver Manual](https://www.wantmotor.com/)
- [Stepper Motor Basics](https://www.motioncontroltips.com/)
