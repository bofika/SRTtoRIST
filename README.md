# Stream Relay

A lightweight, modular C++ application for OpenWRT (ARM) that receives live video via SRT or RTSP and relays it over RIST, while providing network feedback to the encoder.

## Features

- **Multiple Input Modes**:
  - SRT in caller, listener, or multi-interface mode
  - RTSP input using FFmpeg

- **RIST Output**:
  - Reliable transmission using libRIST
  - Each input has a dedicated RIST peer

- **Network Feedback**:
  - Extracts bitrate, packet loss, and RTT metrics
  - Dynamically adjusts bitrate recommendations
  - Sends feedback to encoder via UDP

- **Multi-Interface Support**:
  - Automatic WAN interface detection
  - Each interface can have a dedicated RIST output

## Requirements

- OpenWRT system (ARM architecture)
- Required libraries:
  - libsrt
  - librist
  - FFmpeg (libavformat, libavcodec)
  - nlohmann/json (included as header-only)

## Installation

### From OpenWRT Package Manager

```
opkg update
opkg install stream_relay
```

### From Source

1. Clone the repository:
   ```
   git clone https://github.com/user/stream_relay.git
   cd stream_relay
   ```

2. Build with CMake:
   ```
   mkdir build
   cd build
   cmake ..
   make
   ```

3. Install:
   ```
   make install
   ```

## Configuration

The application uses a JSON configuration file located at `/etc/stream_relay/config.json`. Below are example configurations for different modes:

### SRT Caller Mode

```json
{
  "mode": "srt",
  "srt_mode": "caller",
  "input_url": "srt://192.168.1.100:1234",
  "rist_dst": "192.168.1.200",
  "rist_port": 8000,
  "min_bitrate": 1000,
  "max_bitrate": 5000
}
```

### SRT Listener Mode

```json
{
  "mode": "srt",
  "srt_mode": "listener",
  "listen_port": 1234,
  "rist_dst": "192.168.1.200",
  "rist_port": 8000,
  "min_bitrate": 1000,
  "max_bitrate": 5000
}
```

### SRT Multi-Route Mode

```json
{
  "mode": "srt",
  "srt_mode": "multi",
  "listen_port": 1234,
  "min_bitrate": 1000,
  "max_bitrate": 5000,
  "filter_to_wan": true,
  "multi_route": [
    {
      "interface_ip": "auto",
      "rist_dst": "10.0.0.100",
      "rist_port": 8000
    },
    {
      "interface_ip": "auto",
      "rist_dst": "10.0.0.101",
      "rist_port": 8000
    }
  ]
}
```

### RTSP Mode

```json
{
  "mode": "rtsp",
  "input_url": "rtsp://192.168.1.120:554/stream",
  "rist_dst": "192.168.1.200",
  "rist_port": 8000,
  "min_bitrate": 1000,
  "max_bitrate": 5000
}
```

## Running the Application

```
stream_relay /path/to/config.json
```

The application will:
1. Parse the configuration
2. Set up the appropriate input mode (SRT or RTSP)
3. Create RIST output(s)
4. Start relaying data
5. Send feedback to the encoder

## Network Feedback Format

The application sends feedback to the encoder via UDP in this format:

```
bitrate_hint: 3900 kbps
packet_loss: 0.52%
rtt_ms: 34
```

Alternatively, it can send an HTTP GET request with the syntax:

```
GET /ctrl/stream_setting?index=stream1&width=1920&height=1080&bitrate=10000000
```

## Service Management

Stream Relay is installed as a service that can be managed with:

```
/etc/init.d/stream_relay start
/etc/init.d/stream_relay stop
/etc/init.d/stream_relay restart
```

To enable automatic startup:

```
/etc/init.d/stream_relay enable
```

## Troubleshooting

If you encounter issues:

1. Check the system logs:
   ```
   logread | grep stream_relay
   ```

2. Verify network connectivity to SRT/RTSP sources and RIST destinations

3. Ensure required libraries are installed:
   ```
   opkg list-installed | grep -E 'libsrt|librist|libavformat'
   ```

## License

This project is licensed under the MIT License - see the LICENSE file for details.