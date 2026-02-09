# mod_live_call Quick Start Guide

Get mod_live_call up and running in 10 minutes!

## Prerequisites

- FreeSWITCH installed and running
- Build tools (gcc, make)
- libwebsockets installed
- Node.js or Python (for WebSocket server)

## Step 1: Build and Install (2 minutes)

```bash
# Clone repository
git clone https://github.com/yourusername/mod_live_call.git
cd mod_live_call

# Build module
./scripts/build.sh build

# Install (requires root/sudo)
sudo ./scripts/build.sh install
```

## Step 2: Configure Module (2 minutes)

Edit `/usr/local/freeswitch/conf/autoload_configs/modules.conf.xml`:

```xml
<!-- Add this line in the modules section -->
<load module="mod_live_call"/>
```

The configuration file is automatically installed at:
`/usr/local/freeswitch/conf/autoload_configs/live_call.conf.xml`

Default configuration is good for testing. For production, adjust:
- `ws-server` - Your WebSocket server URL
- `max-threads` - Based on expected load

## Step 3: Start WebSocket Server (2 minutes)

### Option A: Node.js (Recommended)

```bash
cd examples
npm install
npm start
```

Server will start on `ws://localhost:8080/audio`

### Option B: Python

```bash
cd examples
pip install -r requirements.txt
python websocket_server_python.py
```

## Step 4: Load Module (1 minute)

```bash
# Via CLI
fs_cli -x "load mod_live_call"

# Or restart FreeSWITCH
sudo systemctl restart freeswitch
```

Verify:
```bash
fs_cli -x "module_exists mod_live_call"
# Should return: true
```

## Step 5: Add Test Dialplan (2 minutes)

Create `/usr/local/freeswitch/conf/dialplan/default/9000_live_call_test.xml`:

```xml
<include>
  <extension name="live_call_test">
    <condition field="destination_number" expression="^9000$">
      <action application="answer"/>
      <action application="live_call" data="start"/>
      <action application="playback" data="ivr/ivr-welcome.wav"/>
      <action application="sleep" data="30000"/>
      <action application="live_call" data="stop"/>
      <action application="hangup"/>
    </condition>
  </extension>
</include>
```

Reload dialplan:
```bash
fs_cli -x "reloadxml"
```

## Step 6: Test! (1 minute)

### Make a test call:

```bash
# From CLI
fs_cli -x "originate user/1000@domain 9000"

# Or dial 9000 from a registered SIP phone
```

### Check if it's working:

```bash
# Check status
fs_cli -x "live_call status"

# Check statistics
fs_cli -x "live_call stats"

# Watch WebSocket server logs
# You should see audio data being received
```

### Check recordings:

```bash
# WebSocket server saves audio to recordings/
ls -lh examples/recordings/

# Convert to WAV for playback
sox -r 8000 -e mu-law -c 1 examples/recordings/*.raw test.wav

# Play
play test.wav
```

## What Just Happened?

1. ✅ Module loaded into FreeSWITCH
2. ✅ WebSocket server started
3. ✅ Made a test call to extension 9000
4. ✅ Audio streamed from FreeSWITCH to WebSocket server
5. ✅ Audio echoed back to FreeSWITCH (bi-directional)
6. ✅ Recorded audio saved to file

## Next Steps

### Try More Examples

```bash
# Test concurrent calls
./scripts/test.sh concurrent 10

# Load test (requires SIPp)
./scripts/test.sh stress 100 10
```

### Configure for Production

See [DEPLOYMENT.md](DEPLOYMENT.md) for:
- System tuning
- Security hardening
- High availability setup
- Monitoring configuration

### Integrate with Your Application

**WebSocket Server Examples:**
- See `examples/websocket_server_nodejs.js` for Node.js
- See `examples/websocket_server_python.py` for Python
- See `examples/README.md` for integration examples

**Dialplan Examples:**
- See `examples/dialplan_example.xml` for various use cases

### Read Documentation

- [README.md](README.md) - Overview and features
- [API.md](API.md) - Complete API reference
- [DEPLOYMENT.md](DEPLOYMENT.md) - Production deployment
- [SUMMARY.md](SUMMARY.md) - Technical summary

## Common Issues

### Module won't load

```bash
# Check dependencies
ldd /usr/local/freeswitch/mod/mod_live_call.so

# Check logs
tail -f /var/log/freeswitch/freeswitch.log
```

### WebSocket won't connect

```bash
# Check if server is running
netstat -tulpn | grep 8080

# Test connection
wscat -c ws://localhost:8080/audio

# Check firewall
sudo ufw status
```

### No audio

```bash
# Enable debug logging
fs_cli -x "console loglevel debug"

# Check if streaming started
fs_cli -x "live_call status"

# Check WebSocket server logs
```

## Get Help

- **Documentation**: See docs in repository
- **Issues**: https://github.com/yourusername/mod_live_call/issues
- **Discussions**: https://github.com/yourusername/mod_live_call/discussions

## Quick Reference

### CLI Commands

```bash
# Module management
fs_cli -x "load mod_live_call"
fs_cli -x "reload mod_live_call"
fs_cli -x "unload mod_live_call"

# Status and stats
fs_cli -x "live_call status"
fs_cli -x "live_call stats"

# Per-call control
fs_cli -x "uuid_live_call <uuid> start"
fs_cli -x "uuid_live_call <uuid> stop"

# Testing
./scripts/test.sh basic
./scripts/test.sh concurrent 10
```

### Configuration Files

- Module config: `/usr/local/freeswitch/conf/autoload_configs/live_call.conf.xml`
- Dialplan: `/usr/local/freeswitch/conf/dialplan/default/`
- Logs: `/var/log/freeswitch/freeswitch.log`

### Default Ports

- FreeSWITCH SIP: 5060 (TCP/UDP)
- FreeSWITCH RTP: 16384-32768 (UDP)
- WebSocket: 8080 (configurable)
- WebSocket Status (Node.js): 8081

### Directory Structure

```
mod_live_call/
├── src/              # Source code
├── include/          # Headers
├── examples/         # WebSocket servers & dialplan
├── scripts/          # Build & test scripts
├── configs/          # Configuration files
└── *.md             # Documentation
```

---

**Congratulations!** 🎉

You now have mod_live_call running and streaming audio bi-directionally!

For more advanced usage, check out the other documentation files.

Happy streaming! 🎙️
