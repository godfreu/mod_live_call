#!/usr/bin/env python3

"""
WebSocket Server Example untuk mod_live_call

Install dependencies:
    pip install websockets

Run:
    python websocket_server_python.py
"""

import asyncio
import websockets
import logging
from datetime import datetime
from pathlib import Path
import json

# Configuration
WS_HOST = "localhost"
WS_PORT = 8080
WS_PATH = "/audio"
AUDIO_DIR = Path("./recordings")

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] %(levelname)s: %(message)s'
)
logger = logging.getLogger(__name__)

# Create recordings directory
AUDIO_DIR.mkdir(exist_ok=True)

# Track active sessions
sessions = {}

class AudioSession:
    def __init__(self, uuid):
        self.uuid = uuid
        self.start_time = datetime.now()
        self.bytes_received = 0
        self.bytes_sent = 0
        
        # Create audio file
        timestamp = self.start_time.strftime("%Y%m%d_%H%M%S")
        self.filename = AUDIO_DIR / f"{uuid}_{timestamp}.raw"
        self.file = open(self.filename, 'wb')
        
        logger.info(f"Session started: {uuid}")
    
    def write_audio(self, audio_data):
        """Write audio data to file"""
        self.file.write(audio_data)
        self.bytes_received += len(audio_data)
    
    def close(self):
        """Close session and cleanup"""
        if self.file:
            self.file.close()
        
        duration = (datetime.now() - self.start_time).total_seconds()
        logger.info(f"Session ended: {self.uuid}")
        logger.info(f"  Duration: {duration:.1f}s")
        logger.info(f"  Received: {self.bytes_received/1024:.1f} KB")
        logger.info(f"  Sent: {self.bytes_sent/1024:.1f} KB")
        logger.info(f"  File: {self.filename}")

async def audio_handler(websocket, path):
    """Handle WebSocket connection"""
    client_ip = websocket.remote_address[0]
    logger.info(f"Client connected from {client_ip}")
    
    session = None
    last_log_bytes = 0
    
    try:
        async for message in websocket:
            # Parse protocol: [UUID_LEN][UUID][AUDIO_DATA]
            if isinstance(message, bytes):
                uuid_len = message[0]
                uuid = message[1:1+uuid_len].decode('utf-8')
                audio_data = message[1+uuid_len:]
                
                # Initialize session on first message
                if session is None:
                    session = AudioSession(uuid)
                    sessions[uuid] = session
                
                # Save audio data
                if len(audio_data) > 0:
                    session.write_audio(audio_data)
                    
                    # Echo back for testing (bi-directional audio)
                    # Comment out in production or replace with processed audio
                    await websocket.send(audio_data)
                    session.bytes_sent += len(audio_data)
                    
                    # Log every 40KB (~5s at 8kHz PCMU)
                    if session.bytes_received - last_log_bytes > 40000:
                        logger.info(
                            f"[{uuid}] RX: {session.bytes_received/1024:.1f} KB, "
                            f"TX: {session.bytes_sent/1024:.1f} KB"
                        )
                        last_log_bytes = session.bytes_received
    
    except websockets.exceptions.ConnectionClosed:
        logger.info(f"Client disconnected from {client_ip}")
    except Exception as e:
        logger.error(f"Error: {e}")
    finally:
        if session:
            session.close()
            if session.uuid in sessions:
                del sessions[session.uuid]

async def status_handler(websocket, path):
    """Handle status requests"""
    status = {
        "activeSessions": len(sessions),
        "sessions": [
            {
                "uuid": uuid,
                "duration": (datetime.now() - s.start_time).total_seconds(),
                "bytesReceived": s.bytes_received,
                "bytesSent": s.bytes_sent
            }
            for uuid, s in sessions.items()
        ]
    }
    await websocket.send(json.dumps(status, indent=2))

async def router(websocket, path):
    """Route connections based on path"""
    if path == WS_PATH:
        await audio_handler(websocket, path)
    elif path == "/status":
        await status_handler(websocket, path)
    else:
        logger.warning(f"Unknown path: {path}")

async def main():
    """Start WebSocket server"""
    logger.info(f"Starting WebSocket server on ws://{WS_HOST}:{WS_PORT}{WS_PATH}")
    
    async with websockets.serve(
        router,
        WS_HOST,
        WS_PORT,
        subprotocols=["audio-stream"],
        max_size=None,  # No message size limit
        compression=None  # Disable compression for low latency
    ):
        logger.info("WebSocket server ready")
        logger.info(f"Status endpoint: ws://{WS_HOST}:{WS_PORT}/status")
        await asyncio.Future()  # Run forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Shutting down...")
