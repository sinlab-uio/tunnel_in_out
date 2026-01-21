# WebRTC Core Concepts

WebRTC (Web Real-Time Communication):
A set of protocols and APIs for peer-to-peer audio, video, and data transmission with very low latency (typically <500ms).

## Key Terminology

### Connection & Signaling

1. Peer Connection:

 - The main WebRTC connection object between two endpoints
 - Handles media streams, encoding/decoding, and network traversal

2. Signaling:

 - The initial handshake process to establish a WebRTC connection
 - NOT part of WebRTC spec itself - you implement this (usually via WebSocket, HTTP, etc.)
 - Exchanges SDPs and ICE candidates between peers

3. SDP (Session Description Protocol):

 - Text-based description of the media session
 - Contains: codec info, media formats, network info, encryption keys
 - Two types:
   - Offer: Initial proposal from one peer
   - Answer: Response from the other peer

4. ICE (Interactive Connectivity Establishment):

 - Protocol for finding the best network path between peers
 - Tries multiple routes: direct, STUN, TURN

5. ICE Candidate:

 - A potential network address/port combination for connection
 - Peers exchange candidates to find the best connection path
 - Example: 192.168.1.100:54321 (local), 203.0.113.5:12345 (public)

### Network Infrastructure

6. STUN (Session Traversal Utilities for NAT):

 - Server that helps peers discover their public IP address
 - Used to establish direct peer-to-peer connections through NAT
 - Free public STUN servers available (e.g., Google's)

7. TURN (Traversal Using Relays around NAT):

 - Relay server for when direct P2P connection fails
 - Forwards media between peers (higher latency, more bandwidth)
 - Required when firewalls/NAT prevent direct connection
 - Usually requires your own server (bandwidth intensive)

8. NAT (Network Address Translation):

 - What your router does - multiple devices share one public IP
 - Can block direct peer connections, hence need for STUN/TURN

### Media Streams

9. MediaStream:

 - Container for audio/video tracks
 - What you get from getUserMedia() (webcam/mic)
 - Can contain multiple tracks

10. Track:

 - Individual audio or video stream
 - Can be added/removed from peer connections

11. Transceiver:

 - Controls sending/receiving of a track
 - Direction: sendrecv, sendonly, recvonly, inactive

## WebRTC Architecture Patterns

### Live camera to player case

One-to-Many Broadcasting (Most Common):

Camera (RTP) → Media Server → WebRTC → Multiple Browsers
                   ↓
              (Converts RTP to WebRTC)

### Popular media servers:

 - Janus: https://github.com/meetecho/janus-gateway
	Programmed in C

 - Mediasoup: https://github.com/versatica/mediasoup
	Programmed in Rust

 - Kurento: https://github.com/Kurento/kurento
 	Programmed in JavaScript

 - GStreamer: go here: https://gstreamer.freedesktop.org/documentation/webrtc/index.html?gi-language=c


### Simplified Flow

Connection Establishment:

1. Browser connects to signaling server (WebSocket)
2. Browser creates PeerConnection
3. Browser sends "Offer" SDP via signaling
4. Server receives offer, creates "Answer" SDP
5. Server sends answer back via signaling
6. Both sides exchange ICE candidates
7. Connection established, media flows

### Common Confusion Points

"Peer-to-peer" but using a server?

 - Signaling always needs a server
 - Media server converts your RTP stream to WebRTC
 - Browser-to-browser CAN be true P2P (video calls)
 - Broadcasting scenarios need a server

STUN vs TURN:

 - STUN: "What's my public IP?" (lightweight)
 - TURN: "Please relay my data" (bandwidth-heavy)
 - Try STUN first, fallback to TURN

SDP vs ICE:

 - SDP: "Here's what I can send/receive" (codecs, formats)
 - ICE: "Here's how to reach me" (network paths)

For streaming from a camera:

 - Media Server (Janus/Mediasoup) - converts RTP → WebRTC
 - Signaling Server (WebSocket) - coordinates connections
 - STUN Server (can use public ones)
 - TURN Server (optional, for difficult networks)
 - Web Client (HTML/JavaScript) - receives stream

# Janus

`sudo apt install janus`

Edit `/etc/janus/janus.plugin.streaming.jcfg` or `/opt/janus/etc/janus/janus.plugin.streaming.jcfg`

```
general: {
    # Unique ID for this instance
    admin_key = "supersecret"
}

# Define your RTP stream
rtp-sample: {
    type = "rtp"
    id = 1
    description = "Camera H.264 Stream"
    
    # Audio settings (if you have audio)
    audio = true
    audiopt = 111
    audiortpmap = "opus/48000/2"
    audioport = 5004
    
    # Video settings
    video = true
    videopt = 96
    videortpmap = "H264/90000"
    videoport = 5002
    videofmtp = "profile-level-id=42e01f;packetization-mode=1"
    
    # Optional: specific IP if not localhost
    # videoip = "0.0.0.0"
    # audioip = "0.0.0.0"
    
    # Optional: multicast support
    # videomcast = "232.3.4.5"
    # videoiface = "eth0"
}
```

Main config: `/etc/janus/janus.cfg`

```
general: {
    configs_folder = "/etc/janus"
    plugins_folder = "/usr/lib/janus/plugins"
    transports_folder = "/usr/lib/janus/transports"
    
    # Network settings
    nat_1_1_mapping = "YOUR_PUBLIC_IP"  # If behind NAT
    
    # Security
    api_secret = "janusrocks"
    
    # Logging
    log_to_stdout = false
    log_to_file = "/var/log/janus/janus.log"
}

# STUN server (use Google's public STUN or your own)
nat: {
    stun_server = "stun.l.google.com"
    stun_port = 19302
    
    # Optional TURN server
    # turn_server = "your.turn.server"
    # turn_port = 3478
    # turn_type = "udp"
    # turn_user = "username"
    # turn_pwd = "password"
}

# Enable WebSocket transport
transports: {
    transport.websockets.jcfg
    transport.http.jcfg
}

# Enable streaming plugin
plugins: {
    plugin.streaming.jcfg
}
```

Create a service `/etc/systemd/system/janus.service`

```
[Unit]
Description=Janus WebRTC Gateway
After=network.target

[Service]
Type=simple
User=janus
Group=janus
ExecStart=/usr/bin/janus --config=/etc/janus/janus.jcfg
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

Make a Janus user and create the service:

```
# Create janus user
sudo useradd -r -s /bin/false janus

# Create log directory
sudo mkdir -p /var/log/janus
sudo chown janus:janus /var/log/janus

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable janus
sudo systemctl start janus
sudo systemctl status janus
```

Start sending RTP to port 5002 on your Media Server

Check to see if Janus is running:
```
# Should see Janus info
curl http://localhost:8088/janus/info

# Check logs
sudo journalctl -u janus -f
```


