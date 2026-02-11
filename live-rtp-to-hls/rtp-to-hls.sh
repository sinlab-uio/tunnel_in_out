#!/bin/bash

SDP=${1:-video.sdp}

set -x
ffmpeg \
	-fflags nobuffer+genpts \
        -max_delay 50000 \
        -reorder_queue_size 500 \
	-analyzeduration 100M -probesize 100M \
	-protocol_whitelist "file,rtp,udp" \
	-i ${SDP} \
	-c:v libx264 -an \
	-tune zerolatency -preset veryfast \
	-x264-params keyint=15:min-keyint=15:scenecut=0:bframes=0 \
	-hls_time 2 -hls_list_size 10 -hls_flags delete_segments -f hls /var/www/hls/live/stream.m3u8
set +x

# When the source stream is thin enough, this is sufficient:
#    -fflags nobuffer \
# but since we are loosing lots of packets, we use instead:
#    -fflags nobuffer+genpts \
#    -max_delay 50000 \
#    -reorder_queue_size 500 \
# I'm not sure if that really helps because I cannot watch the video.

