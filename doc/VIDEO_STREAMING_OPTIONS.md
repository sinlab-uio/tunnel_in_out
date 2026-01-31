# Streaming attempts

## On the MSI laptop

The MSI laptop has a built-in camera. It is a USB device that is hot-plugged and hot-unplugged by pressing FN-F6. It shows a white light when it is on.
The built-in camera and the VR.cam get their device numbers in the order of appearance.
The one that is found first by the system becomes /dev/video0, the second one becomes /dev/video2. I don't know if you can change the order without rebooting. Easier to keep the built-in camera turned off when you don't need it.

## Try GStreamer for reception

```
sudo apt install gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav

```

```
gst-launch-1.0 -v udpsrc port=5000 caps = "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" ! \
  rtph264depay ! \
  nvh264dec ! \
  glimagesink sync=false
```

```
gst-launch-1.0 playbin uri="file://$(pwd)/stream.sdp"
```

Test that gstreamer video output works in principle:
```
gst-launch-1.0 videotestsrc ! videoconvert ! autovideosink
```

Test that gstreamer can play from the video devices:
```
gst-launch-1.0 v4l2src device=/dev/video2 ! videoconvert ! autovideosink
```

A video sink that listens for RTP, does not work
```
gst-launch-1.0 -v udpsrc port=5004 caps = "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" ! rtph264depay ! decodebin ! videoconvert ! autovideosink
```

GStreamer pipeline for sending RTP
```
gst-launch-1.0 -v videotestsrc ! x264enc tune=zerolatency bitrate=500 speed-preset=superfast ! rtph264pay ! udpsink host=127.0.0.1 port=5004
```

## RTP end-to-end

TODO: Requires NAT traversal on the receiver side.

## RTP-to-HLS conversion

The idea is send RTP to a server that is easily accessible from a web browser.

### RTP Sender

The server would be sending:
```
DEST=${1:-192.168.50.3}
PORT=5004
ffmpeg -use_wallclock_as_timestamps 1 \
       -re \
       -f v4l2 -video_size 3840x1920 -input_format mjpeg -codec:v mjpeg -framerate 30 \
       -fflags +nobuffer \
       -i /dev/video0 \
       -an \
       -c:v h264_nvenc \
       -preset ll \
       -b:v 2M -maxrate 2M -bufsize 2M \
       -g 30 -coder 0 -bf 0 -sc_threshold 0 -flags +low_delay -fflags +nobuffer \
       -analyzeduration 1 -probesize 32 \
       -flags +global_header \
       -f rtp -sdp_file video.sdp \
       "rtp://${DEST}:${PORT}"
```

#### WARNING
Currently, this requires 3 steps:
 - Run the commands once. It creates a video.sdp file. This is a human-readable text file that the client needs to know where data is coming from. At ffmpeg version 4.2.7, this is still buggy. The first line in video.sdp is "SDP:", which is not allowed by the RFC 8866. Stop the command delete that first line.
 - Copy the video.sdp to the client. Perhaps using git add/push and pull on the other side.
 - Run the command again. Ignore the new video.sdp file, it's broken again. But the server is streaming RTP/UDP/IP packets.

#### WARNING
 - When we live-stream RTP and the SDP is created, the following line is incomplete in the SDP file
 ```a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z2QAM6xyBEB4AiflwEQAAAMABAAAAwDAPGDGEYA=,aOhDssiw; profile-level-id=640033```
   The sprop information that encodes details about the video format is missing for the live stream, because RTP is capable of changing this while the streaming is going on. For that reason, the information must be embedded in the live stream itself. The H.264 headers can contain the relevant information, and I am fairly certain that RTP packets are capable of carrying such information, but it must be parsed out of the H.264 headers in real-time. ffmpeg does no do that by default, but stackoverflow posts imply that there are compile-time flags that enable it.
 - It seems that we can send the SPS/PPS (Sequence Parameter Set/Picture Parameter Set) NALUs in-band. There is a comment that this requires an I (IBR frame?). That may be true and should not be a problam because parameter changes will require a new IBR frame anyway. To send this, use `-flags +global_header`.
 - If the encoder/transcoder is x264 (not the case for us), use `-x264-params repeat-header=1` to force SPS/PPS information before every keyframe.


#### More flags
Some more interesting flags found on the web:
 - `-flags global_header` enables global H.264 header, from (here)[https://superuser.com/questions/1011976/how-to-make-ffmpeg-stream-h264-with-pps-and-sps-in-the-rtp-stream]
 - `-bsf h264_mp4toannexb` supposedly enables Annex B header information
 - `-fflags +genpts` may be needed on the encoder side of the command line to force generation of the SPS/PPS header info.

### RTP-to-HLS translation

The proxy that translates RTP to HLS runs the following:

```
ffmpeg \
	-protocol_whitelist file,udp,rtp -i video.sdp \
	-c:v copy -an \
	-f hls -hls_time 4 -hls_playlist_type event outputstream.m3u8
```

The first line tells the proxy that is should receive the video according to the SDP specification. The second lines says that it should copy the video from the input into the output without re-encoding anything; that saves CPU/GPU power and time, but it wastes some bandwidth. It also says that we have no audio stream.
The last line says that we create HLS-compliant output. Essentially, we write small video files to the local directory and update the metadata file outputstream.m3u8 continuously. All of these files must be served by a web server.

This is a variation with 2 audio streams:

```
ffmpeg \
	-protocol_whitelist file,udp,rtp -i video.sdp \
	-c:v copy \
	-c:a aac -b:a 128k -ac 2 \
	-f hls -hls_time 4 -hls_playlist_type event outputstream.m3u8
```

The third line says that we copy the audio streams, that we have 2 audio channels, and that the audio buffer is 128k big. You can see in the sender section that we don't send any audio so far.

This is a variation that repackages the video for HLS low latency:

```
ffmpeg \
	-fflags nobuffer \
	-i rtp://@<multicast_ip>:<port> \
	-c:v libx264 -an \
	-tune zerolatency -preset veryfast \
	-x264-params keyint=15:min-keyint=15:scenecut=0:bframes=0 \
	-hls_time 2 -hls_list_size 3 -hls_flags delete_segments+low_delay -f hls /var/www/hls/stream.m3u8
```

Proposed conversion instructions:
```
ffmpeg -i rtp://... \
  -c:v copy \
  -c:a copy \
  -f hls \
  -hls_time 2 \
  -hls_list_size 3 \
  -hls_flags delete_segments+append_list \
  -hls_segment_filename '/var/www/hls/segment%03d.ts' \
  /var/www/hls/playlist.m3u8
```

### FFmpeg conversion service on an Ubuntu machine

Writing the ffmpeg rule:

```
sudo -u rtc ffmpeg -i rtp://... \
  -c:v copy \
  -c:a copy \
  -f hls \
  -hls_time 2 \
  -hls_list_size 3 \
  -hls_flags delete_segments+append_list \
  -hls_segment_filename '/var/www/hls/segment%03d.ts' \
  /var/www/hls/playlist.m3u8
```

Turning it into a service:

In a file called `/etc/systemd/system/hls-converter.service`, say
```
ini[Unit]
Description=FFmpeg HLS Converter
After=network.target

[Service]
Type=simple
User=www-data
Group=www-data
Restart=always
RestartSec=5
ExecStart=/usr/bin/ffmpeg -i rtp://your_stream_url \
  -c:v copy \
  -c:a copy \
  -f hls \
  -hls_time 2 \
  -hls_list_size 3 \
  -hls_flags delete_segments+append_list \
  -hls_segment_filename '/var/www/hls/segment%%03d.ts' \
  /var/www/hls/playlist.m3u8

[Install]
WantedBy=multi-user.target
```

Then make it a service that is maintained by systemd:

```
sudo systemctl daemon-reload
sudo systemctl enable hls-converter
sudo systemctl start hls-converter
```

### Nginx server setup

On the proxy, install Nginx using
```
sudo apt install nginx
```

Edit /etc/nginx/nginx.conf
This contains the basic web serving information.
Individual servers are created in the directory /etc/nginx/sites-available/.
A server is activated by creating a softlink to it in /etc/nginx/sites-enabled/.
There is one default server names `default` that listens on port 80.

We need a server that listens on port 8080.

In the /etc/nginx/nginx.con file, we make the following changes:
```
	# Increase buffer sizes for streaming
        client_body_buffer_size 128k;
        client_max_body_size 10m;

        sendfile on;
        tcp_nopush on;
        tcp_nodelay on;
        types_hash_max_size 2048;

	keepalive_timeout 15;
        send_timeout 10;
```


This is our config file /etc/nginx/sites-available/hls:
```
server {
    listen 8080; # Port to serve HLS over HTTP
    server_name webrtc.mlab.no

    # HLS stream location
    location /hls {
        # Serve HLS fragments
        root /var/www/hls/;
        alias /var/www/hls;

        # CORS headers (if serving to web browsers)
        add_header Access-Control-Allow-Origin *;
        add_header Access-Control-Allow-Methods 'GET, OPTIONS';
        add_header Access-Control-Allow-Headers 'Range';

        # Critical for low-latency: disable all caching
        add_header Cache-Control 'no-cache, no-store, must-revalidate';
        add_header Pragma 'no-cache';
        add_header Expires '0';

        # Serve m3u8 playlists with correct MIME type
        types {
	    text/html html htm;
            application/vnd.apple.mpegurl m3u8;
            video/mp2t ts;
        }

	# Ensure default MIME type mapping is included
        default_type application/octet-stream;

        # Disable buffering for live content
        sendfile off;
        tcp_nopush off;
        tcp_nodelay on;

        # Enable direct I/O for large files (optional, test performance)
        directio 512k;

        # Prevent caching at proxy level if behind another proxy
        proxy_no_cache 1;
        proxy_cache_bypass 1;
    }
}
```
This means that a client connecting to this server on the port 8080 and with a URL of the kind `http://webrtc.mlab.no:8080/hls/something.m3u8' will be served with a file name something.m3u8 from the directory `/var/www/hls`.

Create the file and change ownership and access to it. In our case the owner is the user `rtc`. Do this:
```
sudo mkdir -p /var/www/hls
sudo chown -R rtc:rtc /var/www/hls
sudo chmod 755 /var/www/hls
```

Activate the site by:
 - soft-linking from /etc/nginx/sites-enabled.
 - validate the configuration using `sudo nginx -t`
 - reload nginx using `sudo systemctl reload nginx`


