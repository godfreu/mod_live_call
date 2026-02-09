# Changelog

All notable changes to mod_live_call will be documented in this file.

## [1.0.0] - 2024-02-09

### Added
- Initial release of mod_live_call
- Bi-directional audio streaming via WebSocket
- Unlimited concurrent channels support
- Connection pooling for WebSocket reuse
- Audio buffer management
- Jitter buffer for audio stability
- Auto-reconnect mechanism with exponential backoff
- Comprehensive statistics tracking
- Configurable buffer sizes and thread limits
- FreeSWITCH application interface (`live_call start/stop`)
- FreeSWITCH API interface (`live_call status/stats`)
- Example WebSocket servers (Node.js and Python)
- Example dialplan configurations
- Build and test scripts
- Comprehensive documentation

### Features
- **Non-blocking Architecture**: All I/O operations are non-blocking
- **Resource Pooling**: Efficient memory and connection management
- **Scalability**: Support for thousands of concurrent channels
- **Reliability**: Auto-reconnect and graceful error handling
- **Performance**: Optimized for low latency and high throughput
- **Monitoring**: Real-time statistics and metrics

### Technical Details
- Based on libwebsockets for WebSocket communication
- Uses FreeSWITCH media bug API for audio capture/injection
- Thread-per-session model with configurable thread pool
- Lock-free paths for critical operations
- Memory pooling for allocation efficiency
- Jitter buffer for network jitter compensation

### Components
- `mod_live_call.c` - Main module implementation
- `ws_client.c` - WebSocket client with connection pooling
- `audio_buffer.c` - Audio buffer management
- `jitter_buffer.c` - Jitter buffer implementation
- `connection_pool.c` - Connection pool statistics

### Examples
- WebSocket server in Node.js
- WebSocket server in Python
- Dialplan examples for various use cases
- Build and test scripts

### Documentation
- Comprehensive README with architecture details
- API reference
- Configuration guide
- Troubleshooting guide
- Performance benchmarks
- Best practices for production deployment

## Future Plans

### [1.1.0] - Planned
- [ ] Multi-server failover support
- [ ] SSL/TLS support for secure WebSocket (wss://)
- [ ] Compression support for bandwidth optimization
- [ ] Custom protocol support (JSON, Protobuf)
- [ ] Enhanced metrics and Prometheus integration
- [ ] Load balancing improvements
- [ ] Advanced codec support

### [2.0.0] - Under Consideration
- [ ] gRPC support as alternative to WebSocket
- [ ] Built-in speech-to-text integration
- [ ] Real-time audio analytics
- [ ] Multi-tenant support
- [ ] Cloud-native deployment (Kubernetes)
- [ ] Distributed tracing support
