#!/usr/bin/env python3
"""
Raspberry Pi 4 Dual Motor Controller (Single Teensy)
Controls two motors via single Teensy 4.1 board
For Wantai 85BYGH450C-060 Stepper Motors with DQ860HA-V3.3 Drivers

Author: Daniel Khito
Date: 2025
"""

import serial
import time
import threading
from typing import Optional
import sys

class DualMotorController:
    """Controls both motors via single Teensy 4.1"""
    
    def __init__(self, port: str, baud_rate: int = 115200):
        """
        Initialize dual motor controller
        
        Args:
            port: Serial port (e.g., '/dev/ttyACM0')
            baud_rate: Serial communication baud rate
        """
        self.port = port
        self.baud_rate = baud_rate
        self.serial_conn: Optional[serial.Serial] = None
        self.is_connected = False
        self.lock = threading.Lock()
        
    def connect(self) -> bool:
        """
        Establish serial connection to Teensy
        
        Returns:
            True if connection successful, False otherwise
        """
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baud_rate,
                timeout=1,
                write_timeout=1
            )
            time.sleep(2)  # Wait for Teensy to initialize
            
            # Clear any startup messages
            self.serial_conn.reset_input_buffer()
            
            self.is_connected = True
            print(f"✓ Connected to Teensy at {self.port}")
            
            # Get initial status
            status = self.get_status()
            if status:
                print(status)
            
            return True
            
        except serial.SerialException as e:
            print(f"✗ Failed to connect - {e}")
            self.is_connected = False
            return False
    
    def disconnect(self):
        """Close serial connection"""
        if self.serial_conn and self.serial_conn.is_open:
            self.stop_all()
            time.sleep(0.5)
            self.serial_conn.close()
            self.is_connected = False
            print("Disconnected from Teensy")
    
    def send_command(self, command: str) -> Optional[str]:
        """
        Send command to Teensy and get response
        
        Args:
            command: Command string to send
            
        Returns:
            Response from Teensy or None if error
        """
        if not self.is_connected or not self.serial_conn:
            print("Not connected to Teensy")
            return None
        
        with self.lock:
            try:
                # Send command
                self.serial_conn.write(f"{command}\n".encode())
                self.serial_conn.flush()
                
                # Read response (timeout after 1 second)
                response_lines = []
                start_time = time.time()
                
                while time.time() - start_time < 1.0:
                    if self.serial_conn.in_waiting:
                        line = self.serial_conn.readline().decode().strip()
                        if line:
                            response_lines.append(line)
                            # For status command, read all lines
                            if "========" in line and len(response_lines) > 1:
                                # Read remaining status lines
                                time.sleep(0.1)
                                while self.serial_conn.in_waiting:
                                    line = self.serial_conn.readline().decode().strip()
                                    if line:
                                        response_lines.append(line)
                                break
                    else:
                        time.sleep(0.01)
                
                return '\n'.join(response_lines) if response_lines else None
                
            except Exception as e:
                print(f"Command error - {e}")
                return None
    
    # Both Motors Commands
    def set_speed_both(self, speed: float) -> bool:
        """Set speed for both motors"""
        speed = max(0, min(speed, 20000))  # Max 20000 steps/sec with 8x microstepping
        response = self.send_command(f"SPEED:{speed}")
        return response is not None
    
    def move_forward(self, speed: float) -> bool:
        """Move both motors forward at specified speed"""
        self.send_command("FORWARD")
        self.set_speed_both(speed)
        self.send_command("RUN")
        return True
    
    def move_backward(self, speed: float) -> bool:
        """Move both motors backward at specified speed"""
        self.send_command("BACKWARD")
        self.set_speed_both(speed)
        self.send_command("RUN")
        return True
    
    def spin_left(self, speed: float) -> bool:
        """Spin left - point turn (M1 back, M2 forward)"""
        response = self.send_command(f"SPIN:LEFT:{speed}")
        return response is not None
    
    def spin_right(self, speed: float) -> bool:
        """Spin right - point turn (M1 forward, M2 back)"""
        response = self.send_command(f"SPIN:RIGHT:{speed}")
        return response is not None
    
    def boost_spin_left(self, speed: float) -> bool:
        """Boosted spin left with initial speed burst"""
        response = self.send_command(f"BOOST:LEFT:{speed}")
        return response is not None
    
    def boost_spin_right(self, speed: float) -> bool:
        """Boosted spin right with initial speed burst"""
        response = self.send_command(f"BOOST:RIGHT:{speed}")
        return response is not None
    
    def boost_forward(self, speed: float) -> bool:
        """Boosted forward movement"""
        response = self.send_command(f"BOOST:FORWARD:{speed}")
        return response is not None
    
    def boost_backward(self, speed: float) -> bool:
        """Boosted backward movement"""
        response = self.send_command(f"BOOST:BACKWARD:{speed}")
        return response is not None
    
    def stop_all(self) -> bool:
        """Stop both motors (gradual)"""
        response = self.send_command("STOP")
        return response is not None
    
    def emergency_stop(self) -> bool:
        """Emergency stop both motors (immediate)"""
        response = self.send_command("ESTOP")
        return response is not None
    
    def get_status(self) -> Optional[str]:
        """Get status of both motors"""
        return self.send_command("STATUS")
    
    def reset_all(self) -> bool:
        """Reset both motor position counters"""
        response = self.send_command("RESET")
        return response is not None
    
    def sync_motors(self) -> bool:
        """Synchronize both motor positions"""
        response = self.send_command("SYNC")
        return response is not None
    
    def configure_boost(self, multiplier: float, duration: int, enabled: bool) -> bool:
        """
        Configure boost parameters
        
        Args:
            multiplier: Speed multiplier (1.0-2.0)
            duration: Boost duration in milliseconds (50-1000)
            enabled: Enable/disable boost
        """
        enabled_val = 1 if enabled else 0
        response = self.send_command(f"CONFIG:BOOST:{multiplier}:{duration}:{enabled_val}")
        return response is not None
    
    # Individual Motor Commands
    def set_motor_speed(self, motor_num: int, speed: float) -> bool:
        """Set speed for individual motor (1 or 2)"""
        speed = max(0, min(speed, 20000))  # Max 20000 steps/sec with 8x microstepping
        response = self.send_command(f"M{motor_num}:SPEED:{speed}")
        return response is not None
    
    def set_motor_direction(self, motor_num: int, forward: bool = True) -> bool:
        """Set direction for individual motor"""
        direction = "FORWARD" if forward else "BACKWARD"
        response = self.send_command(f"M{motor_num}:{direction}")
        return response is not None
    
    def run_motor(self, motor_num: int) -> bool:
        """Start individual motor"""
        response = self.send_command(f"M{motor_num}:RUN")
        return response is not None
    
    def stop_motor(self, motor_num: int) -> bool:
        """Stop individual motor"""
        response = self.send_command(f"M{motor_num}:STOP")
        return response is not None


def main():
    """Main function for interactive control"""
    
    # Configuration - UPDATE THIS PORT
    TEENSY_PORT = '/dev/ttyACM0'  # Change to your Teensy port
    
    print("=" * 60)
    print("Dual Motor Control System (Single Teensy)")
    print("=" * 60)
    
    # Initialize controller
    controller = DualMotorController(TEENSY_PORT)
    
    try:
        # Connect to Teensy
        if not controller.connect():
            print("Failed to connect to Teensy. Check port and connection.")
            return
        
        print("\n✓ System ready!")
        print("\nCommands:")
        print("  f - Move forward")
        print("  s - Move backward")
        print("  a - Spin left (point turn)")
        print("  d - Spin right (point turn)")
        print("  v - BOOST forward")
        print("  x - BOOST backward")
        print("  z - BOOST spin left")
        print("  c - BOOST spin right")
        print("  w - Stop")
        print("  e - Emergency stop")
        print("  + - Increase speed")
        print("  - - Decrease speed")
        print("  ? - Get status")
        print("  y - Sync motors")
        print("  1 - Control Motor 1 only")
        print("  2 - Control Motor 2 only")
        print("  r - Reset positions")
        print("  q - Quit")
        
        current_speed = 2000  # Starting speed (equivalent to old 250 steps/sec in 1x)
        speed_increment = 1000  # Larger increment for 8x microstepping
        
        print(f"\nCurrent speed: {current_speed} steps/sec")
        print("Ready for commands...")
        
        while True:
            cmd = input("\nCommand: ").strip()  # No .lower() to preserve case
            
            if cmd == 'f':
                print(f"Moving forward at {current_speed} steps/sec")
                controller.move_forward(current_speed)
                
            elif cmd == 's':
                print(f"Moving backward at {current_speed} steps/sec")
                controller.move_backward(current_speed)
                
            elif cmd == 'a':
                print(f"Spinning left at {current_speed} steps/sec")
                controller.spin_left(current_speed)
                
            elif cmd == 'd':
                print(f"Spinning right at {current_speed} steps/sec")
                controller.spin_right(current_speed)
                
            elif cmd == 'v':
                print(f"BOOST forward at {current_speed} steps/sec")
                controller.boost_forward(current_speed)
                
            elif cmd == 'x':
                print(f"BOOST backward at {current_speed} steps/sec")
                controller.boost_backward(current_speed)
                
            elif cmd == 'z':
                print(f"BOOST spin left at {current_speed} steps/sec")
                controller.boost_spin_left(current_speed)
                
            elif cmd == 'c':
                print(f"BOOST spin right at {current_speed} steps/sec")
                controller.boost_spin_right(current_speed)
                
            elif cmd == 'w':
                print("Stopping motors...")
                controller.stop_all()
                
            elif cmd == 'e':
                print("EMERGENCY STOP!")
                controller.emergency_stop()
                
            elif cmd == '+':
                current_speed = min(current_speed + speed_increment, 20000)
                controller.set_speed_both(current_speed)
                print(f"Speed increased to {current_speed} steps/sec")
                
            elif cmd == '-':
                current_speed = max(current_speed - speed_increment, 100)
                controller.set_speed_both(current_speed)
                print(f"Speed decreased to {current_speed} steps/sec")
                
            elif cmd == '?':
                status = controller.get_status()
                print(status if status else "No response")
                
            elif cmd == 'r':
                print("Resetting positions...")
                controller.reset_all()
                
            elif cmd == 'y':
                print("Synchronizing motors...")
                controller.sync_motors()
                
            elif cmd == '1':
                # Motor 1 control mode
                print("\n--- Motor 1 Control Mode ---")
                m1_cmd = input("Motor 1 (f=forward, b=back, x=stop): ").lower()
                if m1_cmd == 'f':
                    controller.set_motor_direction(1, True)
                    controller.set_motor_speed(1, current_speed)
                    controller.run_motor(1)
                    print(f"Motor 1 forward at {current_speed}")
                elif m1_cmd == 'b':
                    controller.set_motor_direction(1, False)
                    controller.set_motor_speed(1, current_speed)
                    controller.run_motor(1)
                    print(f"Motor 1 backward at {current_speed}")
                elif m1_cmd == 'x':
                    controller.stop_motor(1)
                    print("Motor 1 stopped")
                    
            elif cmd == '2':
                # Motor 2 control mode
                print("\n--- Motor 2 Control Mode ---")
                m2_cmd = input("Motor 2 (f=forward, b=back, x=stop): ").lower()
                if m2_cmd == 'f':
                    controller.set_motor_direction(2, True)
                    controller.set_motor_speed(2, current_speed)
                    controller.run_motor(2)
                    print(f"Motor 2 forward at {current_speed}")
                elif m2_cmd == 'b':
                    controller.set_motor_direction(2, False)
                    controller.set_motor_speed(2, current_speed)
                    controller.run_motor(2)
                    print(f"Motor 2 backward at {current_speed}")
                elif m2_cmd == 'x':
                    controller.stop_motor(2)
                    print("Motor 2 stopped")
                
            elif cmd == 'q':
                print("Shutting down...")
                controller.stop_all()
                time.sleep(1)
                break
                
            else:
                print(f"Unknown command: {cmd}")
    
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        controller.emergency_stop()
    
    except Exception as e:
        print(f"\nError: {e}")
        controller.emergency_stop()
    
    finally:
        controller.disconnect()
        print("Disconnected. Goodbye!")


if __name__ == "__main__":
    main()
