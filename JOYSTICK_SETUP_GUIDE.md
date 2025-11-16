# Joystick Control Interface Setup Guide

## 🎮 Overview

This system provides **real-time joystick control** of the dual motor system via a web interface:

```
[Browser + Joystick] --WebSocket--> [Raspberry Pi] --Serial--> [Teensy] --> [Motors]
```

### Components:
1. **HTML Interface** - Web page with joystick visualization
2. **WebSocket Server** - Python server on Raspberry Pi
3. **Motor Controller** - Existing Teensy serial interface

---

## 🔧 Logitech Pro 3DS Joystick Mapping

| Axis | Control | Function |
|------|---------|----------|
| `axes[0]` | X-Axis | Left/Right steering (spin) |
| `axes[1]` | Y-Axis | Forward/Backward movement |
| `axes[3]` | Throttle | Speed control (0-20000 steps/sec) |

### Joystick Behavior:

**Forward/Backward with Steering:**
- Push forward (`Y < 0`) + neutral X = Straight forward
- Push forward + tilt left/right = Gradual turns (differential speed)

**Point Turn (Spin in Place):**
- Pull X-axis hard left (X < -0.5) = Spin left
- Pull X-axis hard right (X > 0.5) = Spin right

**Throttle Control:**
- Throttle at min = 0 steps/sec (stopped)
- Throttle at max = 20000 steps/sec (full speed)
- Smooth interpolation between min and max

---

## 📦 Installation

### Step 1: Install Dependencies on Raspberry Pi

```bash
cd raspberry_pi_control
pip3 install -r requirements.txt
```

This installs:
- `pyserial` - For Teensy communication
- `websockets` - For web interface communication

### Step 2: Setup Web Interface

Option A: **Serve from Raspberry Pi (Recommended)**

```bash
# Install simple HTTP server
sudo apt-get update
sudo apt-get install nginx -y

# Copy HTML file to web root
sudo cp web_interface/joystick_control.html /var/www/html/

# Set permissions
sudo chmod 644 /var/www/html/joystick_control.html
```

Access at: `http://raspberrypi.local/joystick_control.html`

Option B: **Serve from your computer**

```bash
# Navigate to web_interface folder
cd web_interface

# Start simple Python HTTP server
python3 -m http.server 8000
```

Access at: `http://localhost:8000/joystick_control.html`

**Important:** Update `WS_URL` in HTML file to point to your Raspberry Pi IP:
```javascript
const WS_URL = 'ws://192.168.1.100:8765';  // Change to your RPi IP
```

---

## 🚀 Running the System

### Step 1: Start WebSocket Server on Raspberry Pi

```bash
cd raspberry_pi_control
python3 websocket_server.py
```

**Expected Output:**
```
============================================================
Dual Motor WebSocket Control Server
============================================================
INFO - ✓ Connected to Teensy at /dev/ttyACM0
INFO - WebSocket server starting on 0.0.0.0:8765
INFO - ✓ WebSocket server running
INFO - Open the HTML interface in your browser:
INFO -   http://raspberrypi.local/joystick_control.html
INFO - 
INFO - Press Ctrl+C to stop
```

### Step 2: Open Web Interface

1. Open browser (Chrome/Edge recommended for gamepad support)
2. Navigate to:
   - `http://raspberrypi.local/joystick_control.html` (if served from RPi)
   - OR `http://localhost:8000/joystick_control.html` (if local)

### Step 3: Connect Joystick

1. Plug in Logitech Pro 3DS to your computer via USB
2. Move any axis on the joystick
3. Browser will detect it automatically
4. Status should show "Joystick: Connected"

### Step 4: Test Connection

Watch for green status indicator:
- 🟢 **Connected** - WebSocket connected to Raspberry Pi
- 🔴 **Disconnected** - Connection lost

Test with manual controls:
- Click "Stop" button
- Click "Get Status" button
- Should see responses in log panel

---

## 🎮 Using the Joystick

### Basic Movement

1. **Increase Throttle** - Pull throttle axis up
   - Throttle bar should show increasing percentage
   - Speed value updates (0-20000)

2. **Forward** - Push Y-axis forward
   - Both motors move forward
   - Direction shows "FORWARD"

3. **Backward** - Pull Y-axis back
   - Both motors move backward
   - Direction shows "BACKWARD"

4. **Stop** - Return joystick to center or reduce throttle to min

### Turning

**Gradual Turn (While Moving):**
```
1. Push Y-axis forward (moving forward)
2. Tilt X-axis left/right gently (< 0.5)
3. Inside wheel slows down, outside speeds up
4. Result: Smooth arc turn
```

**Sharp Turn (Point Turn):**
```
1. Keep Y-axis centered
2. Push X-axis hard left or right (> 0.5)
3. Motors spin in opposite directions
4. Result: Spin in place
```

### Speed Control

- **Low Speed Precision** (Throttle 0-25%):
  - 0 - 5000 steps/sec
  - Good for careful maneuvering

- **Medium Speed** (Throttle 25-50%):
  - 5000 - 10000 steps/sec
  - Normal operation range

- **High Speed** (Throttle 50-75%):
  - 10000 - 15000 steps/sec
  - Fast movement

- **Maximum Speed** (Throttle 75-100%):
  - 15000 - 20000 steps/sec
  - Full power (use carefully!)

---

## 🔍 Interface Features

### Status Panel

- **Speed** - Current motor speed in steps/sec
- **Direction** - Current movement direction
- **Joystick** - Connection status
- **Motor Sync Drift** - Position difference between motors

### Joystick Display

Real-time visualization of:
- X-Axis position (-1.0 to 1.0)
- Y-Axis position (-1.0 to 1.0)
- Throttle position (0% to 100%)
- Calculated speed (0 to 20000)

### Manual Controls

- **Stop** - Smooth stop (normal deceleration)
- **E-Stop** - Emergency stop (0.5 sec ramp)
- **Sync Motors** - Reset position counters
- **Get Status** - Request full status from Teensy

### Log Panel

Shows:
- Connection events
- Commands sent
- Responses received
- Errors and warnings

---

## ⚙️ Configuration

### Adjust Deadzone

In `joystick_control.html`, line ~245:
```javascript
const DEADZONE = 0.1;  // Increase if joystick is too sensitive
```

**Values:**
- `0.05` - Very sensitive (slight touch triggers)
- `0.1` - Default (recommended)
- `0.2` - Less sensitive (requires more movement)

### Adjust Update Rate

In `joystick_control.html`, line ~247:
```javascript
const UPDATE_RATE = 50;  // Update every 50ms (20Hz)
```

**Values:**
- `25ms` - 40Hz (very responsive, more CPU)
- `50ms` - 20Hz (balanced - recommended)
- `100ms` - 10Hz (less responsive, less CPU)

### Adjust Max Speed

In `joystick_control.html`, line ~246:
```javascript
const MAX_SPEED = 20000;  // Maximum motor speed
```

Set to your system's safe maximum speed.

---

## 🐛 Troubleshooting

### Joystick Not Detected

**Problem:** "Joystick: Not Connected" status

**Solutions:**
1. Check USB connection
2. Move any axis to wake up gamepad
3. Try different USB port
4. Check browser console (F12) for errors
5. Verify browser has gamepad support:
   - Chrome/Edge: ✅ Full support
   - Firefox: ✅ Full support
   - Safari: ⚠️ Limited support

### WebSocket Won't Connect

**Problem:** Red status indicator, "Disconnected from Raspberry Pi"

**Solutions:**
1. Check WebSocket server is running on RPi:
   ```bash
   ps aux | grep websocket_server
   ```
2. Verify firewall allows port 8765:
   ```bash
   sudo ufw allow 8765
   ```
3. Check RPi IP address is correct in HTML file
4. Try direct IP instead of `raspberrypi.local`
5. Check network connectivity:
   ```bash
   ping raspberrypi.local
   ```

### Motors Don't Respond

**Problem:** Joystick detected, WebSocket connected, but motors don't move

**Solutions:**
1. Check Teensy is connected to RPi:
   ```bash
   ls -l /dev/ttyACM0
   ```
2. Check motor power supply is ON
3. Check throttle is above minimum (> 0%)
4. Check emergency stop wasn't triggered
5. Check server logs for errors:
   ```bash
   # In terminal running websocket_server.py
   # Look for error messages
   ```

### Motors Move Erratically

**Problem:** Motors jerk, stop/start unexpectedly

**Solutions:**
1. **Increase deadzone** - Joystick may be noisy
2. **Reduce update rate** - Too many commands
3. **Check power supply** - Insufficient current
4. **Verify DIP switches** - Both drivers must match
5. **Check sync drift** - If >10000, one motor losing steps

### One Motor Delayed

**Problem:** One motor starts later than the other

**This should be fixed!** Verify you uploaded the latest firmware with:
- `updateTimers()` function (line 231)
- Sequential calculation, simultaneous timer update (line 163-168)

If still occurs:
1. Re-upload firmware
2. Power cycle Teensy
3. Check timer implementation

---

## 🔐 Security Considerations

### Current Setup

- WebSocket server accepts connections from **any IP** (`0.0.0.0`)
- No authentication or encryption
- **Only use on trusted local network!**

### For Production Use

Add authentication:
```python
# In websocket_server.py
async def handle_client(self, websocket, path):
    # Verify auth token
    auth_header = websocket.request_headers.get('Authorization')
    if auth_header != 'Bearer YOUR_SECRET_TOKEN':
        await websocket.close(1008, "Unauthorized")
        return
    # ... rest of code
```

Or use SSH tunnel:
```bash
# On your computer
ssh -L 8765:localhost:8765 pi@raspberrypi.local

# Access via: ws://localhost:8765
```

---

## 📊 Performance Metrics

### Expected Performance

- **Latency:** 50-100ms (joystick to motor response)
- **Update Rate:** 20Hz (50ms between commands)
- **WebSocket Bandwidth:** ~500 bytes/sec
- **CPU Usage (RPi):** 5-10%
- **Motor Sync Accuracy:** < 100 steps drift

### Monitoring

Check server logs for:
```
DEBUG - Forward at 5000 steps/sec
DEBUG - Differential forward: L=4500, R=5500
DEBUG - Spin left at 3000 steps/sec
```

Check web interface log panel for:
- Connection status
- Commands sent
- Responses received

---

## 🔄 Starting on Boot (Optional)

### Create Systemd Service

```bash
sudo nano /etc/systemd/system/motor-websocket.service
```

Add:
```ini
[Unit]
Description=Motor WebSocket Server
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/motor_control/raspberry_pi_control
ExecStart=/usr/bin/python3 /home/pi/motor_control/raspberry_pi_control/websocket_server.py
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Enable:
```bash
sudo systemctl daemon-reload
sudo systemctl enable motor-websocket.service
sudo systemctl start motor-websocket.service
```

Check status:
```bash
sudo systemctl status motor-websocket.service
```

---

## 📝 Quick Reference

### Start System
```bash
# On Raspberry Pi
cd raspberry_pi_control
python3 websocket_server.py

# On your computer
# Open browser to http://raspberrypi.local/joystick_control.html
# Plug in joystick
```

### Stop System
```bash
# Press Ctrl+C in terminal running websocket_server.py
# Motors will automatically stop
```

### Check Connection
```bash
# Test WebSocket
wscat -c ws://raspberrypi.local:8765

# Test Teensy serial
screen /dev/ttyACM0 115200
STATUS
```

---

## 🎯 Next Steps

1. **Test basic connectivity** - Start server, open interface
2. **Test joystick detection** - Plug in, verify connection
3. **Test manual controls** - Use buttons first
4. **Test joystick control** - Start with low throttle
5. **Fine-tune settings** - Adjust deadzone, update rate
6. **Fix sync drift** - Address >10000 steps issue first!

---

## ✅ System Ready Checklist

- [ ] Dependencies installed (`pip3 install -r requirements.txt`)
- [ ] Web interface accessible (http://raspberrypi.local/joystick_control.html)
- [ ] WebSocket server running (port 8765)
- [ ] Teensy connected (/dev/ttyACM0)
- [ ] Motor power supply ON
- [ ] DIP switches set to 8x microstepping (both drivers)
- [ ] Joystick connected via USB
- [ ] Browser detects gamepad (status shows "Connected")
- [ ] WebSocket shows green indicator
- [ ] Manual controls work (Stop, Status buttons)
- [ ] Sync drift < 100 steps (or investigate issue)

---

**Everything is set up! You now have real-time joystick control! 🎮🚀**
