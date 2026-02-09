# mod_live_call Deployment Guide

Complete guide untuk deploy mod_live_call di production environment.

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Installation](#installation)
3. [Configuration](#configuration)
4. [WebSocket Server Setup](#websocket-server-setup)
5. [System Tuning](#system-tuning)
6. [Security](#security)
7. [Monitoring](#monitoring)
8. [High Availability](#high-availability)
9. [Troubleshooting](#troubleshooting)

## Prerequisites

### System Requirements

**Minimum:**
- CPU: 4 cores
- RAM: 8 GB
- Storage: 50 GB
- Network: 100 Mbps

**Recommended (for 1000+ concurrent calls):**
- CPU: 16+ cores
- RAM: 32+ GB
- Storage: 500 GB SSD
- Network: 1 Gbps

### Software Requirements

**FreeSWITCH:**
- Version: 1.10.x or later
- Installed with development headers

**System Packages:**
```bash
# Debian/Ubuntu
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libwebsockets-dev \
    pkg-config \
    git

# CentOS/RHEL
sudo yum install -y \
    gcc \
    make \
    libwebsockets-devel \
    pkgconfig \
    git
```

### FreeSWITCH Installation

If FreeSWITCH is not installed:

```bash
# Add FreeSWITCH repository
wget -O - https://files.freeswitch.org/repo/deb/debian-release/fsstretch-archive-keyring.asc | apt-key add -
echo "deb http://files.freeswitch.org/repo/deb/debian-release/ stretch main" > /etc/apt/sources.list.d/freeswitch.list

# Install FreeSWITCH
apt-get update
apt-get install -y freeswitch-meta-all freeswitch-dev
```

## Installation

### Step 1: Download Source

```bash
cd /usr/src
git clone https://github.com/yourusername/mod_live_call.git
cd mod_live_call
```

### Step 2: Build Module

```bash
# Set FreeSWITCH paths (if non-standard)
export FS_INCLUDES=/usr/local/freeswitch/include
export FS_MODULES=/usr/local/freeswitch/mod

# Build
./scripts/build.sh build

# Verify build
ls -lh mod_live_call.so
```

### Step 3: Install Module

```bash
# Install (requires root)
sudo ./scripts/build.sh install

# Verify installation
ls -lh /usr/local/freeswitch/mod/mod_live_call.so
ls -lh /usr/local/freeswitch/conf/autoload_configs/live_call.conf.xml
```

### Step 4: Enable Module

Edit `/usr/local/freeswitch/conf/autoload_configs/modules.conf.xml`:

```xml
<configuration name="modules.conf" description="Modules">
  <modules>
    <!-- Add this line -->
    <load module="mod_live_call"/>
    
    <!-- ... other modules ... -->
  </modules>
</configuration>
```

### Step 5: Load Module

```bash
# Via CLI
fs_cli -x "load mod_live_call"

# Or restart FreeSWITCH
systemctl restart freeswitch
```

### Step 6: Verify

```bash
# Check if module is loaded
fs_cli -x "module_exists mod_live_call"
# Should return: true

# Check status
fs_cli -x "live_call status"
# Should return: +OK
```

## Configuration

### Module Configuration

Edit `/usr/local/freeswitch/conf/autoload_configs/live_call.conf.xml`:

```xml
<configuration name="live_call.conf" description="Live Call Module">
  <settings>
    <!-- WebSocket server URL -->
    <param name="ws-server" value="ws://localhost:8080/audio"/>
    
    <!-- Max concurrent threads (adjust based on expected load) -->
    <param name="max-threads" value="1000"/>
    
    <!-- Audio buffer size in bytes -->
    <param name="buffer-size" value="8192"/>
    
    <!-- Sample rate (8000 for narrowband, 16000 for wideband) -->
    <param name="sample-rate" value="8000"/>
    
    <!-- Debug level (0=none, 1=info, 2=debug) -->
    <param name="debug-level" value="0"/>
  </settings>
</configuration>
```

### Dialplan Configuration

Create `/usr/local/freeswitch/conf/dialplan/default/live_call.xml`:

```xml
<include>
  <!-- Test extension -->
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

## WebSocket Server Setup

### Option 1: Node.js Server

```bash
cd /opt
git clone https://github.com/yourusername/mod_live_call.git
cd mod_live_call/examples

# Install dependencies
npm install

# Create systemd service
sudo tee /etc/systemd/system/ws-audio.service > /dev/null <<EOF
[Unit]
Description=WebSocket Audio Server
After=network.target

[Service]
Type=simple
User=freeswitch
WorkingDirectory=/opt/mod_live_call/examples
ExecStart=/usr/bin/node websocket_server_nodejs.js
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable ws-audio
sudo systemctl start ws-audio

# Check status
sudo systemctl status ws-audio
```

### Option 2: Python Server

```bash
cd /opt/mod_live_call/examples

# Install dependencies
pip3 install -r requirements.txt

# Create systemd service
sudo tee /etc/systemd/system/ws-audio.service > /dev/null <<EOF
[Unit]
Description=WebSocket Audio Server (Python)
After=network.target

[Service]
Type=simple
User=freeswitch
WorkingDirectory=/opt/mod_live_call/examples
ExecStart=/usr/bin/python3 websocket_server_python.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# Enable and start
sudo systemctl daemon-reload
sudo systemctl enable ws-audio
sudo systemctl start ws-audio
```

### Verify WebSocket Server

```bash
# Check if server is listening
netstat -tulpn | grep 8080

# Test connection
wscat -c ws://localhost:8080/audio

# Check status endpoint (Node.js only)
curl http://localhost:8081/status
```

## System Tuning

### File Descriptor Limits

```bash
# Edit limits
sudo tee -a /etc/security/limits.conf > /dev/null <<EOF
freeswitch soft nofile 65535
freeswitch hard nofile 65535
* soft nofile 65535
* hard nofile 65535
EOF

# For systemd services
sudo mkdir -p /etc/systemd/system/freeswitch.service.d
sudo tee /etc/systemd/system/freeswitch.service.d/limits.conf > /dev/null <<EOF
[Service]
LimitNOFILE=65535
EOF

sudo systemctl daemon-reload
sudo systemctl restart freeswitch
```

Verify:
```bash
# Check current limits
cat /proc/$(pgrep freeswitch)/limits | grep "open files"
```

### Network Tuning

```bash
# Edit /etc/sysctl.conf
sudo tee -a /etc/sysctl.conf > /dev/null <<EOF
# Network buffer sizes
net.core.rmem_max = 134217728
net.core.wmem_max = 134217728
net.ipv4.tcp_rmem = 4096 87380 67108864
net.ipv4.tcp_wmem = 4096 65536 67108864

# Connection handling
net.core.netdev_max_backlog = 5000
net.ipv4.tcp_max_syn_backlog = 8192
net.ipv4.tcp_tw_reuse = 1
net.ipv4.ip_local_port_range = 10000 65535

# For high connection count
net.ipv4.tcp_fin_timeout = 30
net.ipv4.tcp_keepalive_time = 300
EOF

# Apply changes
sudo sysctl -p
```

### FreeSWITCH Tuning

Edit `/usr/local/freeswitch/conf/autoload_configs/switch.conf.xml`:

```xml
<configuration name="switch.conf" description="Core Configuration">
  <settings>
    <!-- Increase max sessions -->
    <param name="max-sessions" value="10000"/>
    <param name="sessions-per-second" value="1000"/>
    
    <!-- RTP settings -->
    <param name="rtp-start-port" value="16384"/>
    <param name="rtp-end-port" value="32768"/>
    
    <!-- Performance -->
    <param name="max-db-handles" value="50"/>
    <param name="db-handle-timeout" value="10"/>
  </settings>
</configuration>
```

Reload configuration:
```bash
fs_cli -x "reloadxml"
```

## Security

### Firewall Configuration

```bash
# Allow FreeSWITCH SIP and RTP
sudo ufw allow 5060/tcp
sudo ufw allow 5060/udp
sudo ufw allow 16384:32768/udp

# Allow WebSocket (internal only)
sudo ufw allow from 10.0.0.0/8 to any port 8080

# Enable firewall
sudo ufw enable
```

### SSL/TLS for WebSocket

**Generate certificates:**
```bash
sudo mkdir -p /etc/ssl/websocket
sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
  -keyout /etc/ssl/websocket/server.key \
  -out /etc/ssl/websocket/server.crt
```

**Update WebSocket server to use SSL** (implementation specific)

**Update mod_live_call configuration:**
```xml
<param name="ws-server" value="wss://localhost:8443/audio"/>
```

### Access Control

**IP Whitelisting:**
```bash
# In WebSocket server
const ALLOWED_IPS = ['10.0.0.0/8', '192.168.0.0/16'];
```

**Authentication:**
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

## Monitoring

### Log Files

```bash
# FreeSWITCH logs
tail -f /var/log/freeswitch/freeswitch.log

# Module-specific logs
grep "live_call" /var/log/freeswitch/freeswitch.log

# WebSocket server logs
journalctl -u ws-audio -f
```

### Real-time Monitoring

```bash
# Watch statistics
watch -n 1 'fs_cli -x "live_call stats"'

# Monitor active calls
watch -n 1 'fs_cli -x "show calls"'

# System resources
htop
```

### Metrics Collection

**Prometheus Integration:**
```javascript
// In WebSocket server
const promClient = require('prom-client');

const activeSessions = new promClient.Gauge({
    name: 'ws_audio_active_sessions',
    help: 'Number of active audio sessions'
});

// Update metrics
activeSessions.set(sessions.size);
```

**Export endpoint:**
```javascript
app.get('/metrics', (req, res) => {
    res.set('Content-Type', promClient.register.contentType);
    res.end(promClient.register.metrics());
});
```

### Alerting

**Setup Monit:**
```bash
sudo apt-get install monit

# /etc/monit/conf.d/freeswitch
sudo tee /etc/monit/conf.d/freeswitch > /dev/null <<EOF
check process freeswitch with pidfile /var/run/freeswitch/freeswitch.pid
    start program = "/bin/systemctl start freeswitch"
    stop program = "/bin/systemctl stop freeswitch"
    if cpu > 90% for 5 cycles then alert
    if mem > 80% for 5 cycles then alert
    if failed port 5060 then restart
EOF

sudo monit reload
```

## High Availability

### Load Balancer Setup

**Using HAProxy:**

```bash
# Install HAProxy
sudo apt-get install haproxy

# /etc/haproxy/haproxy.cfg
sudo tee -a /etc/haproxy/haproxy.cfg > /dev/null <<EOF
frontend sip_front
    bind *:5060
    mode tcp
    default_backend sip_back

backend sip_back
    mode tcp
    balance roundrobin
    server fs1 10.0.0.11:5060 check
    server fs2 10.0.0.12:5060 check

frontend ws_front
    bind *:8080
    mode http
    default_backend ws_back

backend ws_back
    mode http
    balance leastconn
    option httpchk GET /health
    server ws1 10.0.0.21:8080 check
    server ws2 10.0.0.22:8080 check
EOF

sudo systemctl restart haproxy
```

### Database Replication

For shared state across multiple FreeSWITCH instances:

```bash
# Setup PostgreSQL replication
# (Implementation specific to your database setup)
```

### Failover Script

```bash
#!/bin/bash
# /usr/local/bin/freeswitch-failover.sh

PRIMARY="10.0.0.11"
SECONDARY="10.0.0.12"

if ! ping -c 1 $PRIMARY &> /dev/null; then
    echo "Primary down, switching to secondary"
    # Update load balancer or DNS
    # Notify monitoring system
fi
```

## Troubleshooting

### Module Won't Load

```bash
# Check dependencies
ldd /usr/local/freeswitch/mod/mod_live_call.so

# Check FreeSWITCH logs
tail -100 /var/log/freeswitch/freeswitch.log | grep -i error

# Try manual load with verbose output
fs_cli -x "load mod_live_call"
```

### WebSocket Connection Fails

```bash
# Test WebSocket server
wscat -c ws://localhost:8080/audio

# Check if server is running
systemctl status ws-audio

# Check logs
journalctl -u ws-audio -n 100

# Verify network connectivity
telnet localhost 8080
```

### No Audio Streaming

```bash
# Check if streaming started
fs_cli -x "live_call status"

# Check channel variables
fs_cli -x "uuid_getvar <uuid> live_call_status"

# Enable debug logging
fs_cli -x "console loglevel debug"
fs_cli -x "reload mod_live_call"

# Check WebSocket server logs
```

### Performance Issues

```bash
# Check CPU usage
top -p $(pgrep freeswitch)

# Check memory usage
free -h
ps aux | grep freeswitch

# Check network stats
netstat -s | grep -i error

# Profile with perf
perf record -g -p $(pgrep freeswitch) -- sleep 10
perf report
```

### Memory Leaks

```bash
# Run with valgrind (on test system)
valgrind --leak-check=full --show-leak-kinds=all \
    /usr/local/freeswitch/bin/freeswitch -nonat -nf

# Monitor memory over time
while true; do
    date
    ps aux | grep freeswitch | grep -v grep
    sleep 60
done > memory_usage.log
```

## Backup and Recovery

### Configuration Backup

```bash
# Backup script
#!/bin/bash
BACKUP_DIR="/backup/freeswitch/$(date +%Y%m%d)"
mkdir -p $BACKUP_DIR

# Backup configurations
tar czf $BACKUP_DIR/conf.tar.gz /usr/local/freeswitch/conf/
tar czf $BACKUP_DIR/modules.tar.gz /usr/local/freeswitch/mod/

# Backup database
pg_dump callcenter > $BACKUP_DIR/database.sql

echo "Backup completed: $BACKUP_DIR"
```

### Disaster Recovery

```bash
# Restore from backup
BACKUP_DIR="/backup/freeswitch/20240209"

# Stop services
systemctl stop freeswitch
systemctl stop ws-audio

# Restore configuration
tar xzf $BACKUP_DIR/conf.tar.gz -C /
tar xzf $BACKUP_DIR/modules.tar.gz -C /

# Restore database
psql callcenter < $BACKUP_DIR/database.sql

# Start services
systemctl start freeswitch
systemctl start ws-audio

# Verify
fs_cli -x "status"
fs_cli -x "live_call status"
```

## Maintenance

### Regular Tasks

**Daily:**
- Check logs for errors
- Monitor disk space
- Review statistics

**Weekly:**
- Rotate logs
- Check for updates
- Review performance metrics

**Monthly:**
- Update system packages
- Review capacity
- Test failover procedures

### Update Procedure

```bash
# 1. Backup current installation
./scripts/backup.sh

# 2. Download new version
cd /usr/src/mod_live_call
git pull

# 3. Build and install
./scripts/build.sh rebuild
sudo ./scripts/build.sh install

# 4. Restart FreeSWITCH
systemctl restart freeswitch

# 5. Verify
fs_cli -x "module_exists mod_live_call"
fs_cli -x "live_call status"

# 6. Test
./scripts/test.sh basic
```

## Support Contacts

- **Technical Support**: support@example.com
- **Emergency**: +1-555-0100
- **Documentation**: https://github.com/yourusername/mod_live_call
- **Community**: https://github.com/yourusername/mod_live_call/discussions

---

**mod_live_call Deployment Guide** - Version 1.0.0
Last Updated: 2024-02-09
