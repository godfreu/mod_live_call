# mod_live_call Examples

This directory contains example implementations and configurations for mod_live_call.

## WebSocket Servers

### Node.js Server

Simple WebSocket server that receives audio from FreeSWITCH and echoes it back.

**Requirements:**
```bash
npm install
```

**Run:**
```bash
npm start
# or with auto-reload during development
npm run dev
```

**Features:**
- Receives audio streams from FreeSWITCH
- Saves recordings to `recordings/` directory
- Echoes audio back for bi-directional testing
- HTTP status endpoint on port 8081
- Session tracking and statistics

**Test:**
```bash
# Check status
curl http://localhost:8081/status

# Start a call from FreeSWITCH
fs_cli -x "originate user/1000 9000"
```

### Python Server

Async WebSocket server implementation using websockets library.

**Requirements:**
```bash
pip install -r requirements.txt
```

**Run:**
```bash
python websocket_server_python.py
```

**Features:**
- Async/await for high concurrency
- Audio recording to files
- Echo functionality for testing
- Detailed logging
- Session management

## Dialplan Examples

`dialplan_example.xml` contains ready-to-use dialplan examples:

**Installation:**
```bash
# Copy to FreeSWITCH dialplan
sudo cp dialplan_example.xml /usr/local/freeswitch/conf/dialplan/default/live_call.xml

# Reload dialplan
fs_cli -x "reloadxml"
```

**Extensions:**
- `9000` - Simple streaming test
- `9001` - Bridge with streaming
- `9002` - Conference with streaming
- `9003` - IVR with streaming
- `9004` - Call forwarding with streaming
- `9005` - Call queue with streaming
- `9006` - Voicemail with streaming
- `9007` - Echo test with streaming
- `9008` - API control example

**Usage:**
```bash
# Dial extension 9000
fs_cli -x "originate user/1000 9000"

# Or from SIP phone, dial 9000
```

## Integration Examples

### AI/ML Integration

Example of integrating with AI services:

```javascript
// In websocket_server_nodejs.js, modify the message handler:

ws.on('message', async (data) => {
    const audioData = parseAudio(data);
    
    // Send to Speech-to-Text
    const transcript = await sttService.transcribe(audioData);
    console.log('Transcript:', transcript);
    
    // Process with AI
    const response = await aiService.process(transcript);
    console.log('AI Response:', response);
    
    // Text-to-Speech
    const responseAudio = await ttsService.synthesize(response);
    
    // Send back to FreeSWITCH
    ws.send(responseAudio);
});
```

### Custom Audio Processing

Example with audio processing:

```javascript
const { Transform } = require('stream');

// Create audio processor
const audioProcessor = new Transform({
    transform(chunk, encoding, callback) {
        // Apply audio effects, noise reduction, etc.
        const processed = applyNoiseReduction(chunk);
        callback(null, processed);
    }
});

ws.on('message', (data) => {
    const audioData = parseAudio(data);
    
    // Process through pipeline
    audioProcessor.write(audioData);
});

audioProcessor.on('data', (processedAudio) => {
    // Send processed audio back
    ws.send(processedAudio);
});
```

### Database Integration

Example with session logging:

```javascript
const { Pool } = require('pg');
const pool = new Pool({
    host: 'localhost',
    database: 'callcenter',
    user: 'postgres',
    password: 'password'
});

ws.on('message', async (data) => {
    const { uuid, audioData } = parseMessage(data);
    
    // Log to database
    await pool.query(
        'INSERT INTO call_streams (uuid, timestamp, bytes) VALUES ($1, $2, $3)',
        [uuid, new Date(), audioData.length]
    );
    
    // Process audio
    processAudio(audioData);
});
```

## Testing

### Manual Testing

```bash
# Start WebSocket server
npm start  # or python websocket_server_python.py

# In another terminal, start FreeSWITCH call
fs_cli -x "originate user/1000 9000"

# Monitor WebSocket server logs
# Check recordings directory
ls -lh recordings/
```

### Automated Testing

```bash
# Use test script from parent directory
cd ..
./scripts/test.sh single
./scripts/test.sh concurrent 10
```

### Load Testing

```bash
# Using SIPp (requires SIPp installation)
cd ..
./scripts/test.sh stress 100 10
```

## Audio Format

Audio is transmitted as raw PCM data:
- Sample Rate: 8000 Hz (or as configured)
- Format: PCMU (G.711 μ-law) by default
- Endianness: Little-endian
- Channels: Mono (1 channel)

**Convert recorded audio to WAV:**
```bash
# Using sox
sox -r 8000 -e mu-law -c 1 recording.raw recording.wav

# Using ffmpeg
ffmpeg -f mulaw -ar 8000 -ac 1 -i recording.raw recording.wav
```

## Protocol

WebSocket messages follow this format:

```
[1 byte: UUID length]
[N bytes: UUID string]
[Remaining bytes: Audio data]
```

**Example:**
```
0x24  # UUID length = 36 bytes
"550e8400-e29b-41d4-a716-446655440000"  # UUID
[audio bytes...]  # PCM audio data
```

## Production Deployment

### Security

1. **Use SSL/TLS:**
```javascript
const https = require('https');
const fs = require('fs');

const server = https.createServer({
    cert: fs.readFileSync('server.crt'),
    key: fs.readFileSync('server.key')
});

const wss = new WebSocket.Server({ server });
```

2. **Add Authentication:**
```javascript
wss.on('connection', (ws, req) => {
    const token = req.headers['authorization'];
    if (!isValidToken(token)) {
        ws.close(1008, 'Unauthorized');
        return;
    }
    // Continue...
});
```

3. **Rate Limiting:**
```javascript
const rateLimit = require('express-rate-limit');

const limiter = rateLimit({
    windowMs: 15 * 60 * 1000, // 15 minutes
    max: 100 // limit each IP to 100 requests per windowMs
});
```

### Scaling

**Horizontal Scaling:**
- Run multiple server instances
- Use load balancer (Nginx, HAProxy)
- Implement session affinity

**Vertical Scaling:**
- Increase server resources
- Optimize Node.js heap size
- Use clustering

**Example with PM2:**
```bash
pm2 start websocket_server_nodejs.js -i max
pm2 monit
```

## Troubleshooting

**WebSocket won't connect:**
- Check if server is running: `netstat -an | grep 8080`
- Verify firewall rules: `sudo ufw allow 8080`
- Check FreeSWITCH config: `live_call.conf.xml`

**No audio received:**
- Verify dialplan is loaded: `fs_cli -x "reloadxml"`
- Check if streaming started: `fs_cli -x "live_call status"`
- Enable debug logging in server

**High latency:**
- Reduce buffer sizes
- Check network latency: `ping websocket-server`
- Optimize server processing

## Support

For issues with examples, please open an issue on GitHub.
