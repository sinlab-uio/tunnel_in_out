#!/bin/bash
set -e

# Create output directory if it doesn't exist
mkdir -p /hls-output

# Your FFmpeg RTP→HLS conversion command
# Replace this with your actual working ffmpeg line
exec ffmpeg \
  -i rtp://0.0.0.0:5004 \
  -c:v copy \
  -c:a copy \
  -f hls \
  -hls_time 2 \
  -hls_list_size 5 \
  -hls_flags delete_segments \
  /hls-output/stream.m3u8

