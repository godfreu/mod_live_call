#!/usr/bin/env node

/**
 * WebSocket Server Example untuk mod_live_call
 * 
 * Install dependencies:
 *   npm install ws
 * 
 * Run:
 *   node websocket_server_nodejs.js
 */

const WebSocket = require('ws');
const fs = require('fs');
const path = require('path');

// Configuration
const WS_PORT = 8080;
const WS_PATH = '/audio';
const AUDIO_DIR = './recordings';

// Create recordings directory
if (!fs.existsSync(AUDIO_DIR)) {
    fs.mkdirSync(AUDIO_DIR, { recursive: true });
}

// Create WebSocket server
const wss = new WebSocket.Server({ 
    port: WS_PORT, 
    path: WS_PATH 
});

console.log(`WebSocket server started on ws://localhost:${WS_PORT}${WS_PATH}`);

// Track active sessions
const sessions = new Map();

wss.on('connection', (ws, req) => {
    const clientIp = req.socket.remoteAddress;
    console.log(`[${new Date().toISOString()}] Client connected from ${clientIp}`);
    
    let sessionId = null;
    let audioFile = null;
    let bytesReceived = 0;
    let bytesSent = 0;
    
    ws.on('message', (data) => {
        try {
            // Parse protocol: [UUID_LEN][UUID][AUDIO_DATA]
            const buffer = Buffer.from(data);
            const uuidLen = buffer[0];
            const uuid = buffer.slice(1, 1 + uuidLen).toString('utf-8');
            const audioData = buffer.slice(1 + uuidLen);
            
            // Initialize session on first message
            if (!sessionId) {
                sessionId = uuid;
                const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
                const filename = path.join(AUDIO_DIR, `${uuid}_${timestamp}.raw`);
                audioFile = fs.createWriteStream(filename);
                
                sessions.set(uuid, {
                    startTime: Date.now(),
                    audioFile: filename,
                    bytesReceived: 0,
                    bytesSent: 0
                });
                
                console.log(`[${new Date().toISOString()}] Session started: ${uuid}`);
            }
            
            // Save audio data
            if (audioFile && audioData.length > 0) {
                audioFile.write(audioData);
                bytesReceived += audioData.length;
                
                const session = sessions.get(uuid);
                if (session) {
                    session.bytesReceived = bytesReceived;
                }
            }
            
            // Echo back for testing (bi-directional audio)
            // Comment out in production atau replace dengan processed audio
            if (audioData.length > 0) {
                // Simple echo
                ws.send(audioData);
                bytesSent += audioData.length;
                
                const session = sessions.get(uuid);
                if (session) {
                    session.bytesSent = bytesSent;
                }
            }
            
            // Log every 5 seconds
            if (bytesReceived % (8000 * 5) < 1000) { // ~5s at 8kHz PCMU
                console.log(`[${uuid}] RX: ${(bytesReceived/1024).toFixed(1)} KB, TX: ${(bytesSent/1024).toFixed(1)} KB`);
            }
            
        } catch (error) {
            console.error('Error processing message:', error);
        }
    });
    
    ws.on('close', () => {
        if (audioFile) {
            audioFile.end();
        }
        
        if (sessionId) {
            const session = sessions.get(sessionId);
            if (session) {
                const duration = ((Date.now() - session.startTime) / 1000).toFixed(1);
                console.log(`[${new Date().toISOString()}] Session ended: ${sessionId}`);
                console.log(`  Duration: ${duration}s`);
                console.log(`  Received: ${(session.bytesReceived/1024).toFixed(1)} KB`);
                console.log(`  Sent: ${(session.bytesSent/1024).toFixed(1)} KB`);
                console.log(`  File: ${session.audioFile}`);
                sessions.delete(sessionId);
            }
        }
        
        console.log(`[${new Date().toISOString()}] Client disconnected from ${clientIp}`);
    });
    
    ws.on('error', (error) => {
        console.error(`[${new Date().toISOString()}] WebSocket error:`, error.message);
    });
});

// Status endpoint (HTTP)
const http = require('http');
const statusServer = http.createServer((req, res) => {
    if (req.url === '/status') {
        const status = {
            uptime: process.uptime(),
            activeSessions: sessions.size,
            sessions: Array.from(sessions.entries()).map(([uuid, session]) => ({
                uuid,
                duration: ((Date.now() - session.startTime) / 1000).toFixed(1),
                bytesReceived: session.bytesReceived,
                bytesSent: session.bytesSent
            }))
        };
        
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify(status, null, 2));
    } else {
        res.writeHead(404);
        res.end('Not Found');
    }
});

statusServer.listen(8081, () => {
    console.log('Status endpoint: http://localhost:8081/status');
});

// Graceful shutdown
process.on('SIGINT', () => {
    console.log('\nShutting down...');
    wss.close(() => {
        statusServer.close(() => {
            console.log('Server stopped');
            process.exit(0);
        });
    });
});
