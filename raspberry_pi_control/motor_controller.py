#!/usr/bin/env python3
"""
Raspberry Pi 4 Dual Motor Controller
Controls two Teensy 4.1 boards via serial communication
For Wantai 85BYGH450C-060 Stepper Motors with DQ860HA-V3.3 Drivers

Author: Daniel Khito
Date: 2025
"""

import serial
import time
import threading
from typing import Optional, Tuple
import sys

class MotorController:
    """Controls a single motor via Teensy 4.1"""
    
    def __init__(self, port: str, motor_id: int, baud_rate: int = 115200):
        """
        Initialize motor controller
        
        Args:
            port: Serial port (e.g., '/dev/ttyACM0')
            motor_id: Motor identifier (1 or 2)
            baud_rate: Serial communication baud rate
        """
        self.port = port
        self.motor_id = motor_id
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
            print(f"Motor {self.motor_id}: Connected to {self.port}")
            
            # Get initial status
            status = self.get_status()
            if status:
                print(f"Motor {self.motor_id}: {status}")
            
            return True
            
        except serial.SerialException as e:
            print(f"Motor {self.motor_id}: Failed to connect - {e}")
            self.is_connected = False
            return False
    
    def disconnect(self):
        """Close serial connection"""
        if self.serial_conn and self.serial_conn.is_open:
            self.stop()
            time.sleep(0.5)
            self.serial_conn.close()
            self.is_connected = False
            print(f"Motor {self.motor_id}: Disconnected")
    
    def send_command(self, command: str) -> Optional[str]:
        """
        Send command to Teensy and get response
        
        Args:
            command: Command string to send
            
        Returns:
            Response from Teensy or None if error
        """
        if not self.is_connected or not self.serial_conn:
            print(f"Motor {self.motor_id}: Not connected")
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
                        # Break after getting response
                        if len(response_lines) > 0:
                            break
                    else:
                        time.sleep(0.01)
                
                return '\n'.join(response_lines) if response_lines else None
                
            except Exception as e:
                print(f"Motor {self.motor_id}: Command error - {e}")
                return None
    
    def set_speed(self, speed: float) -> bool:
        """
        Set motor speed
        
        Args:
            speed: Speed in steps/second (0-20000)
            
        Returns:
            True if successful
        """
        speed = max(0, min(speed, 20000))  # Constrain to valid range
        response = self.send_command(f"SPEED:{speed}")
        return response is not None
    
    def set_direction(self, forward: bool = True) -> bool:
        """
        Set motor direction
        
        Args:
            forward: True for forward, False for backward
            
        Returns:
            True if successful
        """
        command = "FORWARD" if forward else "BACKWARD"
        response = self.send_command(command)
        return response is not None
    
    def run(self) -> bool:
        """
        Start motor running
        
        Returns:
            True if successful
        """
        response = self.send_command("RUN")
        return response is not None
    
    def stop(self) -> bool:
        """
        Stop motor (gradual deceleration)
        
        Returns:
            True if successful
        """
        response = self.send_command("STOP")
        return response is not None
    
    def emergency_stop(self) -> bool:
        """
        Emergency stop (immediate)
        
        Returns:
            True if successful
        """
        response = self.send_command("ESTOP")
        return response is not None
    
    def get_status(self) -> Optional[str]:
        """
        Get motor status
        
        Returns:
            Status string or None
        """
        return self.send_command("STATUS")
    
    def reset(self) -> bool:
        """
        Reset motor position counter
        
        Returns:
            True if successful
        """
        response = self.send_command("RESET")
        return response is not None
    
    def enable(self) -> bool:
        """Enable motor driver"""
        response = self.send_command("ENABLE")
        return response is not None
    
    def disable(self) -> bool:
        """Disable motor driver"""
        response = self.send_command("DISABLE")
        return response is not None


class DualMotorController:
    """Controls both motors simultaneously"""
    
    def __init__(self, port1: str, port2: str):
        """
        Initialize dual motor controller
        
        Args:
            port1: Serial port for left motor (e.g., '/dev/ttyACM0')
            port2: Serial port for right motor (e.g., '/dev/ttyACM1')
        """
        self.motor_left = MotorController(port1, motor_id=1)
        self.motor_right = MotorController(port2, motor_id=2)
    
    def connect_all(self) -> bool:
        """Connect to both motors"""
        left_ok = self.motor_left.connect()
        right_ok = self.motor_right.connect()
        return left_ok and right_ok
    
    def disconnect_all(self):
        """Disconnect both motors"""
        self.motor_left.disconnect()
        self.motor_right.disconnect()
    
    def set_speed(self, left_speed: float, right_speed: float) -> bool:
        """Set speed for both motors"""
        left_ok = self.motor_left.set_speed(left_speed)
        right_ok = self.motor_right.set_speed(right_speed)
        return left_ok and right_ok
    
    def move_forward(self, speed: float) -> bool:
        """Move both motors forward at specified speed"""
        self.motor_left.set_direction(forward=True)
        self.motor_right.set_direction(forward=True)
        self.set_speed(speed, speed)
        self.motor_left.run()
        self.motor_right.run()
        return True
    
    def move_backward(self, speed: float) -> bool:
        """Move both motors backward at specified speed"""
        self.motor_left.set_direction(forward=False)
        self.motor_right.set_direction(forward=False)
        self.set_speed(speed, speed)
        self.motor_left.run()
        self.motor_right.run()
        return True
    
    def turn_left(self, speed: float) -> bool:
        """Turn left (left motor backward, right motor forward)"""
        self.motor_left.set_direction(forward=False)
        self.motor_right.set_direction(forward=True)
        self.set_speed(speed, speed)
        self.motor_left.run()
        self.motor_right.run()
        return True
    
    def turn_right(self, speed: float) -> bool:
        """Turn right (left motor forward, right motor backward)"""
        self.motor_left.set_direction(forward=True)
        self.motor_right.set_direction(forward=False)
        self.set_speed(speed, speed)
        self.motor_left.run()
        self.motor_right.run()
        return True
    
    def stop_all(self) -> bool:
        """Stop both motors"""
        left_ok = self.motor_left.stop()
        right_ok = self.motor_right.stop()
        return left_ok and right_ok
    
    def emergency_stop_all(self) -> bool:
        """Emergency stop both motors"""
        left_ok = self.motor_left.emergency_stop()
        right_ok = self.motor_right.emergency_stop()
        return left_ok and right_ok
    
    def get_status_all(self) -> Tuple[Optional[str], Optional[str]]:
        """Get status of both motors"""
        left_status = self.motor_left.get_status()
        right_status = self.motor_right.get_status()
        return left_status, right_status


def main():
    """Main function for testing"""
    
    # Configuration - UPDATE THESE PORTS
    LEFT_MOTOR_PORT = '/dev/ttyACM0'   # Change to your left Teensy port
    RIGHT_MOTOR_PORT = '/dev/ttyACM1'  # Change to your right Teensy port
    
    print("=" * 60)
    print("Dual Motor Control System")
    print("=" * 60)
    
    # Initialize controller
    controller = DualMotorController(LEFT_MOTOR_PORT, RIGHT_MOTOR_PORT)
    
    try:
        # Connect to both motors
        if not controller.connect_all():
            print("Failed to connect to motors. Check ports and connections.")
            return
        
        print("\nMotors connected successfully!")
        print("\nCommands:")
        print("  w - Move forward")
        print("  s - Move backward")
        print("  a - Turn left")
        print("  d - Turn right")
        print("  x - Stop")
        print("  e - Emergency stop")
        print("  + - Increase speed")
        print("  - - Decrease speed")
        print("  ? - Get status")
        print("  q - Quit")
        
        current_speed = 1000  # Starting speed
        speed_increment = 500
        
        print(f"\nCurrent speed: {current_speed} steps/sec")
        print("Ready for commands...")
        
        while True:
            cmd = input("\nCommand: ").strip().lower()
            
            if cmd == 'w':
                print(f"Moving forward at {current_speed} steps/sec")
                controller.move_forward(current_speed)
                
            elif cmd == 's':
                print(f"Moving backward at {current_speed} steps/sec")
                controller.move_backward(current_speed)
                
            elif cmd == 'a':
                print(f"Turning left at {current_speed} steps/sec")
                controller.turn_left(current_speed)
                
            elif cmd == 'd':
                print(f"Turning right at {current_speed} steps/sec")
                controller.turn_right(current_speed)
                
            elif cmd == 'x':
                print("Stopping motors...")
                controller.stop_all()
                
            elif cmd == 'e':
                print("EMERGENCY STOP!")
                controller.emergency_stop_all()
                
            elif cmd == '+':
                current_speed = min(current_speed + speed_increment, 20000)
                print(f"Speed increased to {current_speed} steps/sec")
                
            elif cmd == '-':
                current_speed = max(current_speed - speed_increment, 100)
                print(f"Speed decreased to {current_speed} steps/sec")
                
            elif cmd == '?':
                print("\n--- Motor Status ---")
                left_status, right_status = controller.get_status_all()
                print("\nLeft Motor:")
                print(left_status if left_status else "No response")
                print("\nRight Motor:")
                print(right_status if right_status else "No response")
                
            elif cmd == 'q':
                print("Shutting down...")
                controller.stop_all()
                time.sleep(1)
                break
                
            else:
                print(f"Unknown command: {cmd}")
    
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        controller.emergency_stop_all()
    
    except Exception as e:
        print(f"\nError: {e}")
        controller.emergency_stop_all()
    
    finally:
        controller.disconnect_all()
        print("Disconnected. Goodbye!")


if __name__ == "__main__":
    main()
