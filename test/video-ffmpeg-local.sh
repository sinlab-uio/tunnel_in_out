#!/bin/bash

DEVICE=${1:-0}

VIDEODEV="/dev/video${DEVICE}"

RESOLUTION=3840x1920

v4l2-ctl -D -d ${VIDEODEV} | grep BisonCam > /dev/null

if [ $? -eq 0 ] ; then
    echo "Camera is onboard BisonCam, resolution is only 1920x1080"
    RESOLUTION=1920x1080
fi

echo ffplay -f v4l2 -video_size ${RESOLUTION} -input_format mjpeg -codec:v mjpeg -framerate 30 -fflags +nobuffer -i ${VIDEODEV}
ffplay -f v4l2 -video_size ${RESOLUTION} -input_format mjpeg -codec:v mjpeg -framerate 30 -fflags +nobuffer -i ${VIDEODEV}

# Stereo USB camera from vr.com:
# VR.Cam 02
# VR.cam max resolution 3840x1920 @30fps

# MSI laptop's built-in camera:
# BisonCam, NB Pro
# MSI onboard camera max resolution 1920x1080 @30fps

