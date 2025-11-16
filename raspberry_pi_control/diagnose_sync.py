#!/usr/bin/env python3
"""
Motor Synchronization Diagnostic Tool
Helps identify why motors have >10000 steps drift

Run this to diagnose the sync issue before using joystick interface
"""

import serial
import time
import sys

TEENSY_PORT = '/dev/ttyACM0'
BAUD_RATE = 115200

def connect_teensy():
    """Connect to Teensy"""
    try:
        ser = serial.Serial(TEENSY_PORT, BAUD_RATE, timeout=2)
        time.sleep(2)  # Wait for connection
        print("✓ Connected to Teensy")
        return ser
    except Exception as e:
        print(f"✗ Failed to connect: {e}")
        return None

def send_command(ser, command):
    """Send command and get response"""
    ser.write(f"{command}\n".encode())
    time.sleep(0.1)
    response = []
    while ser.in_waiting:
        line = ser.readline().decode().strip()
        if line:
            response.append(line)
    return response

def get_motor_positions(ser):
    """Get current motor positions"""
    response = send_command(ser, "STATUS")
    
    motor1_pos = None
    motor2_pos = None
    
    for line in response:
        if "Motor 1" in line:
            reading_m1 = True
        if "Motor 2" in line:
            reading_m1 = False
        if "Position:" in line:
            pos = int(line.split(":")[1].strip())
            if reading_m1:
                motor1_pos = pos
            else:
                motor2_pos = pos
    
    return motor1_pos, motor2_pos

def run_diagnostic():
    """Run comprehensive sync diagnostic"""
    print("="*60)
    print("MOTOR SYNCHRONIZATION DIAGNOSTIC")
    print("="*60)
    print()
    
    # Connect
    ser = connect_teensy()
    if not ser:
        return
    
    try:
        # Test 1: Initial positions
        print("TEST 1: Initial Position Check")
        print("-" * 40)
        send_command(ser, "SYNC")
        time.sleep(0.5)
        m1, m2 = get_motor_positions(ser)
        print(f"Motor 1 Position: {m1}")
        print(f"Motor 2 Position: {m2}")
        print(f"Drift: {abs(m1 - m2)} steps")
        
        if abs(m1 - m2) > 10:
            print("⚠️  WARNING: Motors not synced after SYNC command!")
        else:
            print("✓ Motors synced successfully")
        print()
        
        # Test 2: Short run test
        print("TEST 2: Short Run Test (5 seconds at 2000 steps/sec)")
        print("-" * 40)
        send_command(ser, "SYNC")
        time.sleep(0.5)
        
        print("Starting motors...")
        send_command(ser, "SPEED:2000")
        send_command(ser, "FORWARD")
        send_command(ser, "RUN")
        
        # Track positions over time
        print("Monitoring positions every second:")
        for i in range(5):
            time.sleep(1)
            m1, m2 = get_motor_positions(ser)
            drift = abs(m1 - m2)
            print(f"  Second {i+1}: M1={m1:6d} | M2={m2:6d} | Drift={drift:5d} steps")
            
            if drift > 100:
                print(f"  ⚠️  Drift exceeded 100 steps at second {i+1}!")
        
        send_command(ser, "STOP")
        time.sleep(0.5)
        
        m1_final, m2_final = get_motor_positions(ser)
        final_drift = abs(m1_final - m2_final)
        print(f"\nFinal Drift: {final_drift} steps")
        
        if final_drift > 500:
            print("🔴 FAIL: Large drift detected (>500 steps)")
        elif final_drift > 100:
            print("⚠️  WARNING: Moderate drift (100-500 steps)")
        else:
            print("✓ PASS: Drift within acceptable range (<100 steps)")
        print()
        
        # Test 3: Direction change test
        print("TEST 3: Direction Change Test")
        print("-" * 40)
        send_command(ser, "SYNC")
        time.sleep(0.5)
        
        print("Forward 2 sec...")
        send_command(ser, "SPEED:3000")
        send_command(ser, "FORWARD")
        send_command(ser, "RUN")
        time.sleep(2)
        
        m1, m2 = get_motor_positions(ser)
        print(f"  After forward: M1={m1} | M2={m2} | Drift={abs(m1-m2)}")
        
        print("Backward 2 sec...")
        send_command(ser, "BACKWARD")
        time.sleep(2)
        
        m1, m2 = get_motor_positions(ser)
        dir_change_drift = abs(m1 - m2)
        print(f"  After backward: M1={m1} | M2={m2} | Drift={dir_change_drift}")
        
        send_command(ser, "STOP")
        
        if dir_change_drift > final_drift * 2:
            print("⚠️  Drift increases significantly during direction changes!")
        print()
        
        # Diagnosis
        print("="*60)
        print("DIAGNOSIS")
        print("="*60)
        
        if final_drift > 1000:
            print("\n🔴 CRITICAL ISSUE: One motor is losing steps")
            print("\nPossible Causes:")
            print("  1. DIP switches don't match between drivers")
            print("     - Check both are: SW5:ON, SW6:ON, SW7:ON, SW8:OFF")
            print("  2. One motor has mechanical resistance")
            print("     - Check for wheel friction, belt tension")
            print("  3. Driver current setting too low")
            print("     - Verify both drivers set to ~5A (80-90% of 6A)")
            print("  4. Power supply insufficient")
            print("     - Check voltage under load (should be 48V)")
            print("  5. Loose wiring connections")
            print("     - Check PUL+ and DIR+ connections")
            print("\nRecommended Actions:")
            print("  1. Power off and verify DIP switches")
            print("  2. Check driver current potentiometer settings")
            print("  3. Test each motor individually")
            print("  4. Check mechanical setup")
        
        elif final_drift > 100:
            print("\n⚠️  MODERATE ISSUE: Motors drifting more than expected")
            print("\nPossible Causes:")
            print("  1. Slight difference in mechanical load")
            print("  2. Timer precision at high speeds")
            print("  3. Minor electrical noise")
            print("\nRecommended Actions:")
            print("  1. Monitor drift during actual use")
            print("  2. Use SYNC command periodically")
            print("  3. Reduce max speed if drift worsens")
        
        else:
            print("\n✓ GOOD: Sync performance within acceptable range")
            print("\nRecommendations:")
            print("  - Continue monitoring during operation")
            print("  - Use SYNC command if drift exceeds 100 steps")
            print("  - System ready for joystick control")
        
        print()
        print("="*60)
    
    finally:
        ser.close()
        print("\nDiagnostic complete.")

if __name__ == "__main__":
    try:
        run_diagnostic()
    except KeyboardInterrupt:
        print("\n\nDiagnostic interrupted by user")
    except Exception as e:
        print(f"\n\nError during diagnostic: {e}")
