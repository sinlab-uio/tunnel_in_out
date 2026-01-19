#!/bin/bash

DEST=192.169.50.3

echo "Sending video from the camera to destination ${DEST}"

# ffplay -f v4l2 -video_size 3840x1920 -input_format mjpeg -codec:v mjpeg -framerate 30 -fflags +nobuffer -i /dev/video0

# -re read input at native rate
# -an disable audio
# -c:v copy copy the audio format without change
# -sdp_file <file> create the SDP file
# ffmpeg -re -i input.mp4 -an -c:v copy -f rtp -sdp_file video.sdp "rtp://192.168.1.109:5004"

ffmpeg -use_wallclock_as_timestamps 1 \
       -f v4l2 -video_size 3840x1920 -input_format mjpeg -codec:v mjpeg -framerate 30 \
       -fflags +nobuffer \
       -i /dev/video0 \
       -re \
       -an \
       -c:v libx264 -preset ultrafast -tune zerolatency -fflags nobuffer -bf 0 -pix_fmt yuv420p \
       -x264opts keyint=30:min-keyint=1:scenecut=-1 \
       -f rtp -sdp_file video.sdp "rtp://${DEST}:5004"

# Play back from RTP
# ffplay -protocol_whitelist "file,rtp,udp" -i video.sdp

