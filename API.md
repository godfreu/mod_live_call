# mod_live_call API Documentation

## Table of Contents
1. [Application Interface](#application-interface)
2. [API Interface](#api-interface)
3. [Channel Variables](#channel-variables)
4. [Events](#events)
5. [WebSocket Protocol](#websocket-protocol)
6. [C API](#c-api)

## Application Interface

### `live_call`

Start or stop live audio streaming for a call.

**Syntax:**
```
live_call <start|stop>
```

**Parameters:**
- `start` - Begin audio streaming for the current session
- `stop` - End audio streaming for the current session

**Returns:**
- Success or failure status

**Examples:**

```xml
<!-- Dialplan usage -->
<action application="live_call" data="start"/>
<action application="bridge" data="user/1000@domain"/>
<action application="live_call" data="stop"/>
```

```bash
# CLI usage
fs_cli -x "uuid_live_call <uuid> start"
fs_cli -x "uuid_live_call <uuid> stop"
```

**Notes:**
- Must be called on an answered channel
- Automatically starts worker thread and WebSocket connection
- Safe to call multiple times (idempotent)
- Will auto-stop on channel hangup

## API Interface

### `live_call status`

Get current module status and session counts.

**Syntax:**
```
live_call status
```

**Returns:**
```
+OK
Active Sessions: 42
Total Sessions: 156
```

**Example:**
```bash
fs_cli -x "live_call status"
```

### `live_call stats`

Get detailed statistics including connection pool information.

**Syntax:**
```
live_call stats
```

**Returns:**
```
+OK
=== Live Call Statistics ===
Active Sessions: 42
Total Sessions: 156

=== Connection Pool ===
Total Connections: 1000
Active Connections: 42
Available Connections: 958
Total Acquires: 156
Total Releases: 114
```

**Example:**
```bash
fs_cli -x "live_call stats"
```

**Metrics Explained:**
- **Active Sessions**: Currently streaming calls
- **Total Sessions**: Cumulative sessions since module load
- **Total Connections**: Size of connection pool
- **Active Connections**: WebSocket connections in use
- **Available Connections**: Idle connections in pool
- **Total Acquires**: Times connections were acquired
- **Total Releases**: Times connections were released

## Channel Variables

### Set by Module

#### `live_call_status`
Current streaming status for the channel.

**Values:**
- `active` - Streaming is active
- `inactive` - Streaming is not active

**Example:**
```xml
<action application="log" data="INFO Status: ${live_call_status}"/>
```

#### `live_call_uuid`
Session UUID for tracking.

**Example:**
```xml
<action application="set" data="my_uuid=${live_call_uuid}"/>
```

### Configuration Variables

Can be set before calling `live_call`:

#### `live_call_ws_server`
Override default WebSocket server URL.

**Example:**
```xml
<action application="set" data="live_call_ws_server=ws://custom-server:8080/audio"/>
<action application="live_call" data="start"/>
```

#### `live_call_buffer_size`
Override buffer size for this session.

**Example:**
```xml
<action application="set" data="live_call_buffer_size=16384"/>
<action application="live_call" data="start"/>
```

## Events

### Custom Events

Module fires custom events for monitoring.

#### `CUSTOM::live_call::started`

Fired when streaming starts.

**Event Headers:**
```
Event-Name: CUSTOM
Event-Subclass: live_call::started
Unique-ID: <uuid>
Channel-Name: <channel-name>
Caller-Caller-ID-Number: <caller-id>
WebSocket-Server: <ws-url>
```

**Example:**
```xml
<action application="event" data="CUSTOM live_call::started"/>
```

#### `CUSTOM::live_call::stopped`

Fired when streaming stops.

**Event Headers:**
```
Event-Name: CUSTOM
Event-Subclass: live_call::stopped
Unique-ID: <uuid>
Channel-Name: <channel-name>
Duration-Seconds: <duration>
Packets-Sent: <count>
Packets-Received: <count>
Bytes-Sent: <bytes>
Bytes-Received: <bytes>
```

## WebSocket Protocol

### Connection

**URL Format:**
```
ws://host:port/path
```

**Example:**
```
ws://localhost:8080/audio
```

**Protocol:**
```
audio-stream
```

### Message Format

#### Outbound (FreeSWITCH → Server)

Binary message format:
```
┌──────────────┬──────────────┬──────────────────┐
│ UUID Length  │     UUID     │   Audio Data     │
│   (1 byte)   │  (N bytes)   │  (remaining)     │
└──────────────┴──────────────┴──────────────────┘
```

**Example:**
```
0x24  # 36 bytes
"550e8400-e29b-41d4-a716-446655440000"
[PCM audio bytes...]
```

**C Structure:**
```c
struct outbound_message {
    uint8_t uuid_len;
    char uuid[36];
    uint8_t audio_data[];
};
```

#### Inbound (Server → FreeSWITCH)

Raw audio data (no header required):
```
┌──────────────────┐
│   Audio Data     │
│   (PCM bytes)    │
└──────────────────┘
```

**Note:** Server can send audio without UUID. FreeSWITCH will inject it to the appropriate call based on the WebSocket connection.

### Audio Format

**Default:**
- Codec: PCMU (G.711 μ-law)
- Sample Rate: 8000 Hz
- Channels: 1 (Mono)
- Bit Depth: 8 bits
- Endianness: Little-endian

**Other Codecs:**
Audio is captured in whatever codec the call is using. No transcoding is performed by the module.

### Connection States

1. **CONNECTING** - Initial connection attempt
2. **CONNECTED** - Connection established
3. **STREAMING** - Actively sending/receiving audio
4. **RECONNECTING** - Attempting to reconnect after failure
5. **CLOSED** - Connection closed

### Error Handling

**Close Codes:**
- `1000` - Normal closure
- `1001` - Going away
- `1002` - Protocol error
- `1006` - Abnormal closure
- `1008` - Policy violation
- `1011` - Server error

## C API

### Core Functions

#### `start_live_call()`

Start streaming for a session.

**Prototype:**
```c
switch_status_t start_live_call(switch_core_session_t *session);
```

**Parameters:**
- `session` - FreeSWITCH session pointer

**Returns:**
- `SWITCH_STATUS_SUCCESS` - Successfully started
- `SWITCH_STATUS_FALSE` - Failed to start

**Example:**
```c
if (start_live_call(session) == SWITCH_STATUS_SUCCESS) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
                     "Streaming started\n");
}
```

#### `stop_live_call()`

Stop streaming for a session.

**Prototype:**
```c
switch_status_t stop_live_call(switch_core_session_t *session);
```

**Parameters:**
- `session` - FreeSWITCH session pointer

**Returns:**
- `SWITCH_STATUS_SUCCESS` - Successfully stopped
- `SWITCH_STATUS_FALSE` - Failed or not streaming

#### `get_live_call_status()`

Get module status.

**Prototype:**
```c
switch_status_t get_live_call_status(switch_stream_handle_t *stream);
```

**Parameters:**
- `stream` - Output stream for results

**Returns:**
- `SWITCH_STATUS_SUCCESS`

#### `get_live_call_stats()`

Get module statistics.

**Prototype:**
```c
switch_status_t get_live_call_stats(switch_stream_handle_t *stream);
```

**Parameters:**
- `stream` - Output stream for results

**Returns:**
- `SWITCH_STATUS_SUCCESS`

### WebSocket API

#### `ws_create_connection()`

Create a WebSocket connection.

**Prototype:**
```c
ws_connection_t *ws_create_connection(const char *url, 
                                     switch_memory_pool_t *pool);
```

**Parameters:**
- `url` - WebSocket URL
- `pool` - Memory pool

**Returns:**
- Pointer to connection or NULL on error

#### `ws_connect()`

Connect to WebSocket server.

**Prototype:**
```c
switch_status_t ws_connect(ws_connection_t *conn);
```

#### `ws_reconnect()`

Reconnect to WebSocket server.

**Prototype:**
```c
switch_status_t ws_reconnect(ws_connection_t *conn);
```

#### `ws_disconnect()`

Disconnect from WebSocket server.

**Prototype:**
```c
void ws_disconnect(ws_connection_t *conn);
```

#### `ws_send_audio()`

Send audio data over WebSocket.

**Prototype:**
```c
switch_status_t ws_send_audio(ws_connection_t *conn, 
                             const char *uuid,
                             const uint8_t *data, 
                             size_t len);
```

#### `ws_receive_audio()`

Receive audio data from WebSocket.

**Prototype:**
```c
switch_status_t ws_receive_audio(ws_connection_t *conn, 
                                uint8_t **data, 
                                size_t *len);
```

### Buffer API

#### `audio_buffer_create()`

Create audio buffer.

**Prototype:**
```c
audio_buffer_t *audio_buffer_create(size_t size, 
                                   switch_memory_pool_t *pool);
```

#### `audio_buffer_write()`

Write to audio buffer.

**Prototype:**
```c
switch_status_t audio_buffer_write(audio_buffer_t *ab, 
                                  const void *data, 
                                  size_t len);
```

#### `audio_buffer_read()`

Read from audio buffer.

**Prototype:**
```c
switch_status_t audio_buffer_read(audio_buffer_t *ab, 
                                 void *data, 
                                 size_t len);
```

### Jitter Buffer API

#### `jitter_buffer_create()`

Create jitter buffer.

**Prototype:**
```c
jitter_buffer_t *jitter_buffer_create(size_t min_size, 
                                     size_t max_size,
                                     switch_memory_pool_t *pool);
```

#### `jitter_buffer_put()`

Put data into jitter buffer.

**Prototype:**
```c
switch_status_t jitter_buffer_put(jitter_buffer_t *jb, 
                                 const void *data, 
                                 size_t len);
```

#### `jitter_buffer_get()`

Get data from jitter buffer.

**Prototype:**
```c
switch_status_t jitter_buffer_get(jitter_buffer_t *jb, 
                                 void *data, 
                                 size_t len);
```

#### `jitter_buffer_ready()`

Check if buffer is ready for reading.

**Prototype:**
```c
int jitter_buffer_ready(jitter_buffer_t *jb);
```

## Integration Examples

### JavaScript/Node.js

```javascript
const WebSocket = require('ws');

const ws = new WebSocket('ws://localhost:8080/audio', {
    perMessageDeflate: false
});

ws.on('message', (data) => {
    // Parse message
    const uuidLen = data[0];
    const uuid = data.slice(1, 1 + uuidLen).toString('utf-8');
    const audioData = data.slice(1 + uuidLen);
    
    console.log(`Received ${audioData.length} bytes from ${uuid}`);
    
    // Process and send back
    const processed = processAudio(audioData);
    ws.send(processed);
});
```

### Python

```python
import asyncio
import websockets

async def handler(websocket, path):
    async for message in websocket:
        # Parse message
        uuid_len = message[0]
        uuid = message[1:1+uuid_len].decode('utf-8')
        audio_data = message[1+uuid_len:]
        
        print(f"Received {len(audio_data)} bytes from {uuid}")
        
        # Process and send back
        processed = process_audio(audio_data)
        await websocket.send(processed)
```

### C

```c
#include "mod_live_call.h"

// Custom application
SWITCH_STANDARD_APP(my_app_function)
{
    // Start streaming
    if (start_live_call(session) == SWITCH_STATUS_SUCCESS) {
        // Do something
        switch_sleep(10 * 1000000); // 10 seconds
        
        // Stop streaming
        stop_live_call(session);
    }
}
```

## Error Codes

| Code | Description | Action |
|------|-------------|--------|
| `SWITCH_STATUS_SUCCESS` | Operation successful | Continue |
| `SWITCH_STATUS_FALSE` | Operation failed | Check logs |
| `SWITCH_STATUS_MEMERR` | Memory allocation failed | Check system resources |
| `SWITCH_STATUS_GENERR` | General error | Check configuration |

## Troubleshooting

### Enable Debug Logging

```xml
<!-- live_call.conf.xml -->
<param name="debug-level" value="2"/>
```

Or via CLI:
```bash
fs_cli -x "console loglevel debug"
```

### Check Module Status

```bash
# Verify module is loaded
fs_cli -x "module_exists mod_live_call"

# Get status
fs_cli -x "live_call status"

# Get detailed stats
fs_cli -x "live_call stats"
```

### WebSocket Connection Test

```bash
# Using wscat
wscat -c ws://localhost:8080/audio

# Using curl
curl -i -N -H "Connection: Upgrade" -H "Upgrade: websocket" \
     -H "Sec-WebSocket-Version: 13" -H "Sec-WebSocket-Key: test" \
     http://localhost:8080/audio
```

## Performance Tips

1. **Buffer Sizing**: Adjust buffer size based on network latency
2. **Connection Pool**: Set max-threads according to expected load
3. **Jitter Buffer**: Tune min/max sizes for your network conditions
4. **Worker Threads**: Monitor CPU usage and adjust thread priorities
5. **Memory**: Monitor for leaks with valgrind

## Support

For API questions or issues:
- GitHub Issues: Technical problems
- GitHub Discussions: Usage questions
- Email: support@example.com

---

**mod_live_call API Documentation** - Version 1.0.0
