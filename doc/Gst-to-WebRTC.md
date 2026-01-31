# Using GStreamer with WebRTC

## Hints and tips from AI

(webrtc-sendrecv)[https://webrtchacks.com/webrtc-plumbing-with-gstreamer/] is an example application
in C and Python that uses the
(webrtcbin element)[https://webrtchacks.com/webrtc-plumbing-with-gstreamer/] for bidirectional WebRTC
streaming. It handles SDP exchange and ICE candidate negotation. It doesn't use H.264 by default.

The webrtcbin element requires a signaling server, which must be reachable by TCP,
to exchange SDP and ICE candidates between the GStreamer pipeline elements.

A typical pipeline includes videotestsrc or camera input, followed by encoding (h264enc, vp8enc, opusenc),
RTP payloading (rtph264pay, rtpvp8pay, rtpopuspay), and webrtcbin.

Common Use Cases
 - Streaming camera feeds to a web browser.
 - Creating low-latency, bidirectional media applications.
 - Bridging RTSP camera streams to WebRTC.

## WebRTC server fed from GStreamer

Setting up a WebRTC server fed by GStreamer with RTP streams involves using a media server or a specific library designed to bridge GStreamer and WebRTC. The Mediasoup WebRTC server is frequently combined with webrtc-sendrecv to do this,.

Requirements
 - GStreamer: A multimedia framework.
 - Node.js: For running the mediasoup server-side application.
 - libnice-dev (or equivalent): A STUN/TURN library dependency for GStreamer's WebRTC implementation.
 - libsrtp2-dev: Secure Real-time Transport Protocol library.

## Setup

### 1. Configure the Server

Set up a signalling server. According to AI, this combines webrtc-sendrecv, Node.js, mediasoup, and a library like ws for WebSockets.

While these seems like sensible elements, the combination does not actually make sense. To be investigated.

### 2. The sending side

Build a client with webrtcbin. The following was proposed by AI, and the pipeline contains many elements that I do not understand yet.

```
gst-launch-1.0 \
	webrtcbin \
	bundle-policy=max-bundle stun-server=stun://stun.l.google.com:19302 \
	name=sendrecv \
		videotestsrc is-live=true pattern=smpte ! video/x-raw,width=640,height=480,framerate=30/1 \
		! vp8enc deadline=1 \
		! rtpvp8pay \
		! sendrecv.sink_0 sendrecv.src_0 ! application/x-rtp,media=video,encoding-name=VP8 \
		! rtpvp8depay \
		! decodebin \
		! autovideosink
```

### 3. Signalling connection

Steps
 - GStreamer must generate an SDL offer.
 - Send the offer to the signalling server using some kind of TCP connection.
 - Receive the signalling server's SDP answer.
 - Set the remote description in GStreamer.
 - Exchange ICE candidates with the signalling server (IP addresses/ports) until you succeed.

For more documentation about this, search:
 - GStreamer WebRTC Examples
 - mediasoup Documentation
 - GStreamer Documentation

## Related Open Source projects

From (medevel)[https://medevel.com/13-os-webrtc-server/]: Kurento, Ant Media Server, Open-EasyRTC, openVidu, Galene, SaltyRTC, Janus, ion media server, MediaSoup, OvenMediaEngine, Temasys, JSCommunicator, PeerJS Server and Library

