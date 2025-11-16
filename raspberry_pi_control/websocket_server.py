#!/usr/bin/env python3
"""
WebSocket Server for Joystick Control
Bridges HTML interface to Teensy motor controller
Runs on Raspberry Pi 4

Author: Daniel Khito
Date: 2025
"""

import asyncio
import websockets
import json
import logging
from motor_controller import DualMotorController
from typing import Set
import signal

# Configuration
WEBSOCKET_HOST = '0.0.0.0'  # Listen on all interfaces
WEBSOCKET_PORT = 8765
TEENSY_PORT = '/dev/ttyACM0'

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Global state
controller = None
connected_clients: Set[websockets.WebSocketServerProtocol] = set()
current_state = {
    'speed': 0,
    'direction': 'STOPPED',
    'syncDrift': 0
}


class JoystickServer:
    def __init__(self, teensy_port: str):
        """Initialize joystick server"""
        self.controller = DualMotorController(teensy_port)
        self.running = False
        
    async def start(self):
        """Start the server"""
        # Connect to Teensy
        if not self.controller.connect():
            logger.error("Failed to connect to Teensy!")
            return False
        
        logger.info(f"✓ Connected to Teensy at {TEENSY_PORT}")
        self.running = True
        return True
    
    async def handle_client(self, websocket):
        """Handle a new WebSocket client connection"""
        client_id = f"{websocket.remote_address[0]}:{websocket.remote_address[1]}"
        logger.info(f"New client connected: {client_id}")
        
        # Add to connected clients
        connected_clients.add(websocket)
        
        try:
            # Send initial connection confirmation
            await websocket.send(json.dumps({
                'type': 'response',
                'message': 'Connected to Raspberry Pi motor controller'
            }))
            
            # Handle incoming messages
            async for message in websocket:
                try:
                    await self.process_message(websocket, message)
                except Exception as e:
                    logger.error(f"Error processing message: {e}")
                    await websocket.send(json.dumps({
                        'type': 'error',
                        'message': str(e)
                    }))
        
        except websockets.exceptions.ConnectionClosed:
            logger.info(f"Client disconnected: {client_id}")
        except Exception as e:
            logger.error(f"Unexpected error in client handler: {e}")
        
        finally:
            connected_clients.discard(websocket)
            # Stop motors when client disconnects (non-blocking)
            try:
                await asyncio.to_thread(self.controller.stop_all)
                logger.info(f"Motors stopped - client {client_id} disconnected")
            except Exception as e:
                logger.error(f"Error stopping motors on disconnect: {e}")
    
    async def process_message(self, websocket, message: str):
        """Process incoming WebSocket message"""
        try:
            data = json.loads(message)
            msg_type = data.get('type')
            
            if msg_type == 'command':
                # Direct serial command
                command = data.get('command')
                logger.info(f"Direct command: {command}")
                response = await asyncio.to_thread(self.controller.send_command, command)
                
                await websocket.send(json.dumps({
                    'type': 'response',
                    'message': response or 'Command sent'
                }))
            
            elif msg_type == 'motor_control':
                # Joystick motor control
                await self.handle_motor_control(websocket, data.get('command'))
            
            else:
                logger.warning(f"Unknown message type: {msg_type}")
        
        except json.JSONDecodeError:
            logger.error(f"Invalid JSON: {message}")
        except Exception as e:
            logger.error(f"Error processing message: {e}")
    
    async def handle_motor_control(self, websocket, command: dict):
        """Handle motor control commands from joystick"""
        cmd_type = command.get('type')
        
        try:
            if cmd_type == 'forward':
                speed = command.get('speed', 2000)
                await asyncio.to_thread(self.controller.move_forward, speed)
                current_state['speed'] = speed
                current_state['direction'] = 'FORWARD'
                logger.debug(f"Forward at {speed} steps/sec")
            
            elif cmd_type == 'backward':
                speed = command.get('speed', 2000)
                await asyncio.to_thread(self.controller.move_backward, speed)
                current_state['speed'] = speed
                current_state['direction'] = 'BACKWARD'
                logger.debug(f"Backward at {speed} steps/sec")
            
            elif cmd_type == 'spin':
                direction = command.get('direction')
                speed = command.get('speed', 2000)
                
                if direction == 'left':
                    await asyncio.to_thread(self.controller.spin_left, speed)
                    current_state['direction'] = 'SPIN LEFT'
                elif direction == 'right':
                    await asyncio.to_thread(self.controller.spin_right, speed)
                    current_state['direction'] = 'SPIN RIGHT'
                
                current_state['speed'] = speed
                logger.debug(f"Spin {direction} at {speed} steps/sec")
            
            elif cmd_type == 'differential':
                # Differential drive for smooth turning
                direction = command.get('direction')
                left_speed = command.get('leftSpeed', 2000)
                right_speed = command.get('rightSpeed', 2000)
                
                # Set individual motor speeds (run in thread to avoid blocking)
                await asyncio.to_thread(self.controller.set_motor_speed, 1, int(left_speed))
                await asyncio.to_thread(self.controller.set_motor_speed, 2, int(right_speed))
                
                if direction == 'forward':
                    await asyncio.to_thread(self.controller.send_command, "M1:FORWARD")
                    await asyncio.to_thread(self.controller.send_command, "M2:FORWARD")
                elif direction == 'backward':
                    await asyncio.to_thread(self.controller.send_command, "M1:BACKWARD")
                    await asyncio.to_thread(self.controller.send_command, "M2:BACKWARD")
                
                await asyncio.to_thread(self.controller.send_command, "RUN")
                
                current_state['speed'] = int((left_speed + right_speed) / 2)
                current_state['direction'] = f"DIFF {direction.upper()}"
                logger.debug(f"Differential {direction}: L={left_speed}, R={right_speed}")
            
            elif cmd_type == 'stop':
                await asyncio.to_thread(self.controller.stop_all)
                current_state['speed'] = 0
                current_state['direction'] = 'STOPPED'
                logger.debug("Motors stopped")
            
            # Send status update to all clients
            await self.broadcast_status()
        
        except Exception as e:
            logger.error(f"Motor control error: {e}")
            await websocket.send(json.dumps({
                'type': 'error',
                'message': f"Motor control error: {str(e)}"
            }))
    
    async def broadcast_status(self):
        """Broadcast current status to all connected clients"""
        if not connected_clients:
            return
        
        status_msg = json.dumps({
            'type': 'status',
            'speed': current_state['speed'],
            'direction': current_state['direction'],
            'syncDrift': current_state['syncDrift']
        })
        
        # Send to all connected clients
        disconnected = set()
        for client in connected_clients:
            try:
                await client.send(status_msg)
            except websockets.exceptions.ConnectionClosed:
                disconnected.add(client)
        
        # Remove disconnected clients
        connected_clients.difference_update(disconnected)
    
    async def status_update_loop(self):
        """Periodically request status from Teensy and broadcast to clients"""
        while self.running:
            try:
                # Request status from Teensy (non-blocking)
                status = await asyncio.to_thread(self.controller.get_status)
                
                if status and 'Sync Drift:' in status:
                    # Parse sync drift from status
                    lines = status.split('\n')
                    for line in lines:
                        if 'Sync Drift:' in line:
                            drift_str = line.split(':')[1].strip().split()[0]
                            current_state['syncDrift'] = int(drift_str)
                            break
                
                # Broadcast status to all clients
                await self.broadcast_status()
            
            except Exception as e:
                logger.error(f"Status update error: {e}")
            
            # Update every 2 seconds
            await asyncio.sleep(2)
    
    async def run_server(self):
        """Run the WebSocket server"""
        if not await self.start():
            return
        
        # Start status update loop
        status_task = asyncio.create_task(self.status_update_loop())
        
        logger.info(f"WebSocket server starting on {WEBSOCKET_HOST}:{WEBSOCKET_PORT}")
        
        try:
            async with websockets.serve(
                self.handle_client, 
                WEBSOCKET_HOST, 
                WEBSOCKET_PORT,
                ping_interval=20,  # Send ping every 20 seconds
                ping_timeout=10    # Wait 10 seconds for pong
            ):
                logger.info("✓ WebSocket server running")
                logger.info("Open the HTML interface in your browser:")
                logger.info("  http://192.168.1.43/joystick_control.html")
                logger.info("\nPress Ctrl+C to stop")
                
                # Keep running
                await asyncio.Future()  # Run forever
        
        except KeyboardInterrupt:
            logger.info("\nShutting down...")
        
        finally:
            self.running = False
            status_task.cancel()
            self.controller.emergency_stop()
            self.controller.close()
            logger.info("Server stopped")


def main():
    """Main entry point"""
    logger.info("=" * 60)
    logger.info("Dual Motor WebSocket Control Server")
    logger.info("=" * 60)
    
    # Create server
    server = JoystickServer(TEENSY_PORT)
    
    # Run server
    try:
        asyncio.run(server.run_server())
    except KeyboardInterrupt:
        logger.info("\nServer stopped by user")


if __name__ == "__main__":
    main()
