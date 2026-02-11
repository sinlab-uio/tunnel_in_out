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

# Some nice tips:
# set "-fflags nobuffer+genpts+flush_packets" to reduce buffering. I'm not sure if that is really
#     a good idea.
# set "-hls_flags delete_segments+append_list+omit_endlist", instead of the default. The default
#     creates a new stream.m3u8 file for every segment, end the segment with an ENDLIST tag, and moves
#     the new file to the previous one. I'm not sure if this is a good idea, I believe that this
#     will fail with delete. But worth trying.
#
# Using fragmented MP4:
#     -hls_segment_type fmp4 \
#     -hls_fmp4_init_filename init.mp4 \
# This creates the init.mp4 file (and adds it to the m3u8 file), and .m4s files that contain
# the actual video data. These can be concatenated for playable video files. Can be extended
# to several layers, but that's not our goal.
# Definitely worth trying for our case.
#
# Server mode:
# To allow the server to experience a timeout but still go back to waiting for data from a streaming
# client, use the following even before --fflags:
#     -reconnect 1 \
#     -reconnect_streamed 1 \
#     -reconnect_delay_max 10 \
#
# However, for extensive resilience, use also a while true; do ... done loop in bash

# When the source stream is thin enough, this is sufficient:
#    -fflags nobuffer \
# but since we are loosing lots of packets, we use instead:
#    -fflags nobuffer+genpts \
#    -max_delay 50000 \
#    -reorder_queue_size 500 \
# I'm not sure if that really helps because I cannot watch the video.

