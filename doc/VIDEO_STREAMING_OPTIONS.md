# Streaming attempts

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
       -f rtp -sdp_file video.sdp \
       "rtp://${DEST}:${PORT}"
```

WARNING: Currently, this requires 3 steps:
 - Run the commands once. It creates a video.sdp file. This is a human-readable text file that the client needs to know where data is coming from. At ffmpeg version 4.2.7, this is still buggy. The first line in video.sdp is "SDP:", which is not allowed by the RFC 8866. Stop the command delete that first line.
 - Copy the video.sdp to the client. Perhaps using git add/push and pull on the other side.
 - Run the command again. Ignore the new video.sdp file, it's broken again. But the server is streaming RTP/UDP/IP packets.

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
	-hls_time 1 -hls_list_size 3 -hls_flags delete_segments+low_delay -f hls /path/to/your/webserver/stream.m3u8
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

This is our config file /etc/nginx/sites-available/hls:
```
server {
    listen 8080; # Port to serve HLS over HTTP

    location /hls {
        # Serve HLS fragments
        root /var/www/hls/;

        # Add CORS headers for web players if needed
        add_header Access-Control-Allow-Origin *;
    }
}
```
This means that a client connecting to this server on the port 8080 and with a URL of the kind `http://webrtc.mlab.no:8080/hls/something.m3u8' will be served with a file name something.m3u8 from the directory `/var/www/hls`.

