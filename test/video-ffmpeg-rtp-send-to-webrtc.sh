#!/bin/bash

# an address for UPV: 172.31.144.1
# the default address for UPV: 158.42.251.4
# MAC in lab: 192.168.50.3

DEST=${1:-158.39.75.57}
PORT=5004 # - WebRTC classic
# PORT=6656 # - UPV open port

echo "Sending video from the camera to destination ${DEST}"

# ffplay -f v4l2 -video_size 3840x1920 -input_format mjpeg -codec:v mjpeg -framerate 30 -fflags +nobuffer -i /dev/video0

# -re read input at native rate
# -an disable audio
# -c:v copy copy the audio format without change
# -sdp_file <file> create the SDP file
# ffmpeg -re -i input.mp4 -an -c:v copy -f rtp -sdp_file video.sdp "rtp://192.168.1.109:${PORT}"

if [[ "$2" == "cpu" ]]; then
echo "================================================================================"
echo "Encode with Intel"
echo "================================================================================"
ffmpeg -use_wallclock_as_timestamps 1 \
       -re \
       -f v4l2 -video_size 3840x1920 -input_format mjpeg -codec:v mjpeg -framerate 30 \
       -fflags +nobuffer \
       -i /dev/video0 \
       -an \
       -c:v libx264 -preset ultrafast -tune zerolatency -fflags nobuffer -bf 0 -pix_fmt yuv420p \
       -x264opts keyint=30:min-keyint=1:scenecut=-1 \
       -f rtp -sdp_file video.sdp "rtp://${DEST}:${PORT}"
else
echo "================================================================================"
echo "CUDA encoding"
echo "================================================================================"
echo ffmpeg -use_wallclock_as_timestamps 1 \
       -re \
       -f v4l2 -video_size 3840x1920 -input_format mjpeg -codec:v mjpeg -framerate 30 \
       -fflags +nobuffer \
       -i /dev/video0 \
       -an \
       -c:v h264_nvenc \
       -preset p1 \
       -f rtp \
       "rtp://${DEST}:${PORT}"

       ffmpeg -use_wallclock_as_timestamps 1 \
	      -re -f v4l2 -video_size 3840x1920 -input_format mjpeg -framerate 30 -i /dev/video0 -c:v h264_nvenc -preset p1 -f rtp rtp://158.39.75.57:5004

       # INPUT
       # -fflags +nobuffer \
       # OUTPUT
       # -b:v 2M -maxrate 2M -bufsize 2M \
       # -analyzeduration 1 -probesize 32 \
       # -g 30 -coder 0 -bf 0 -flags +low_delay -fflags +nobuffer \
       # -sdp_file video.sdp \
fi





# Play back from RTP
# ffplay -protocol_whitelist "file,rtp,udp" -i video.sdp

