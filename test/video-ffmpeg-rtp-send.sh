#!/bin/bash

# an address for UPV: 172.31.144.1
# the default address for UPV: 158.42.251.4
# MAC in lab: 192.168.50.3

# Parameter 1: destination IP address
DEST=${1:-192.168.50.3}

# Parameter 2: destination port, 5004 is the classical WebRTC potr
# PORT=${2:-5004}
PORT=${2:-6656}

# Parameter 3: cpu or gpu encoding, gpu is default
ENCODER="${3:-gpu}"

# Parameter 4: video device to read from, machine could have several, default is /dev/video0
#              depending on the device properties, set the resolution of the input stream
DEVICENO=${4:-0}
DEVICE="/dev/video${DEVICENO}"
INPUTRES=3840x1920
v4l2-ctl -D -d ${VIDEODEV} | grep BisonCam > /dev/null
if [ $? -eq 0 ] ; then
    echo "Camera is onboard BisonCam, resolution is only 1920x1080"
    INPUTRES=1920x1080
fi


echo "Sending video from the camera to destination ${DEST}:${PORT}"

# ffplay -f v4l2 -video_size 3840x1920 -input_format mjpeg -codec:v mjpeg -framerate 30 -fflags +nobuffer -i /dev/video0

# -re read input at native rate
# -an disable audio
# -c:v copy copy the audio format without change
# -sdp_file <file> create the SDP file
# ffmpeg -re -i input.mp4 -an -c:v copy -f rtp -sdp_file video.sdp "rtp://192.168.1.109:${PORT}"

#
# Find all parameters for NVEnc encoders
#
# ffmpeg -hide_banner -h encoder=h264_nvenc

if [[ "${ENCODER}" == "cpu" ]]; then
echo "================================================================================"
echo "Encode with Intel"
echo "================================================================================"
ffmpeg -use_wallclock_as_timestamps 1 \
       -re \
       -f v4l2 -video_size ${INPUTRES} -input_format mjpeg -codec:v mjpeg -framerate 30 \
       -fflags +nobuffer \
       -i ${DEVICE}\
       -an \
       -c:v libx264 -preset ultrafast -tune zerolatency -fflags nobuffer -bf 0 -pix_fmt yuv420p \
       -profile:v baseline -level:v 3.1 \
       -x264opts keyint=30:min-keyint=1:scenecut=-1 \
       -f rtp -sdp_file video.sdp "rtp://${DEST}:${PORT}"
else
echo "================================================================================"
echo "CUDA encoding"
echo "================================================================================"
ffmpeg -use_wallclock_as_timestamps 1 \
       -re \
       -f v4l2 -video_size ${INPUTRES} -input_format mjpeg -codec:v mjpeg -framerate 30 \
       -fflags +nobuffer \
       -i ${DEVICE}\
       -an \
       -c:v h264_nvenc \
       -preset p1 -tune ull -profile:v baseline -rc vbr \
       -flags +global_header \
       -f rtp -sdp_file sdp-live-${DEST}.sdp \
       "rtp://${DEST}:${PORT}"
fi

#	-bf 0 \
#       -b:v 2M -maxrate 2M -bufsize 2M \
#       -g 30 -coder 0 -bf 0 -sc_threshold 0 -flags +low_delay -fflags +nobuffer \
#       -analyzeduration 1 -probesize 32 \




# Play back from RTP
# ffplay -protocol_whitelist "file,rtp,udp" -i video.sdp

