#!/bin/bash

# an address for UPV: 172.31.144.1
# the default address for UPV: 158.42.251.4
# MAC in lab: 192.168.50.3

# Parameter 1: destination IP address
DEST=${1:-192.168.50.3}

# Parameter 2: destination port, 5004 is the classical WebRTC potr
PORT=${2:-5004}
# PORT=${2:-6656}

# Parameter 3: cpu or gpu encoding, gpu is default
ENCODER="${3:-gpu}"

# Parameter 4: video device to read from, machine could have several, default is /dev/video0
#              depending on the device properties, set the resolution of the input stream
DEVICENO=${4:-0}
DEVICE="/dev/video${DEVICENO}"
INPUTRES=3840x1920
v4l2-ctl -D -d ${DEVICE} | grep BisonCam > /dev/null
if [ $? -eq 0 ] ; then
    echo "Camera is onboard BisonCam, resolution is only 1920x1080"
    INPUTRES=1920x1080
fi


echo "Sending video from the camera to destination ${DEST}:${PORT}"

if [[ "${ENCODER}" == "cpu" ]]; then

gst-launch-1.0 -v v4l2src device=${DEVICE} \
	! image/jpeg,width=1920,height=1080,framerate=30/1 ! jpegdec \
	! videoconvert \
	! x264enc tune=zerolatency bitrate=500 speed-preset=superfast \
	! rtph264pay \
	! udpsink host=${DEST} port=${PORT}

# Reading larger input resolutions makes this really slow
#	! image/jpeg,width=640,height=480,framerate=30/1 ! jpegdec \
#	! image/jpeg,width=1920,height=1080,framerate=30/1 ! jpegdec \
#	! video/x-raw,width=640,height=480 \
#	! video/x-raw,width=1920,height=1080 \
# 	! video/x-raw,width=1280,height=720 \

else

echo gst-launch-1.0 -v v4l2src device=${DEVICE} \
	! nvh264enc preset=low-latency-hp zerolatency=true \
	! h264parse \
	! rtph264pay \
	! udpsink host=${DEST} port=${PORT}

  	# ! "video/x-raw,width=1920,height=1080,framerate=30/1" \
	# ! cudaupload \
	# ! nvh264enc preset=low-latency-hp rc-mode=cbr bitrate=5000 gop-size=60 zerolatency=true \
	# ! h264parse \
	# ! rtph264pay config-interval=1 pt=96 \
	# ! udpsink host=${DEST} port=${PORT} sync=false async=false

# Special command for Tegras:
# gst-launch-1.0 -v v4l2src device=/dev/video0 \
# 	! "image/jpeg,width=1920,height=1080,framerate=30/1"
# 	! nvv4l2decoder mjpeg=1 \
# 	! nvvidconv \
# 	! "video/x-raw(memory:NVMM),format=NV12" \
# 	! nvv4l2h264enc bitrate=4000000 \
# 	! h264parse \
# 	! rtph264pay config-interval=1 pt=96 \
# 	! udpsink host=${DEST} port=${PORT}

fi

