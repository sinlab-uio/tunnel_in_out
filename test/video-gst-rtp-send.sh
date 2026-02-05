#!/bin/bash

# the default address for UPV: 158.42.251.4
# MAC in lab: 192.168.50.3

# Parameter 1: destination IP address
# DEST=${1:-192.168.50.3}
DEST=${1:-158.42.251.4}

# Parameter 2: destination port, 5004 is the classical WebRTC potr
PORT=${2:-5004}
# PORT=${2:-6656}

# Parameter 3: cpu or gpu encoding, gpu is default
#              gpu doesn't work yet
ENCODER="${3:-cpu}"

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
	! image/jpeg,width=3840,height=1920,framerate=30/1 ! jpegdec \
	! videoconvert \
	! videoscale \
	! video/x-raw, width=1280,height=720 \
	! x264enc tune=zerolatency bitrate=500 speed-preset=superfast \
	! rtph264pay \
	! udpsink host=${DEST} port=${PORT}

# Reading larger input resolutions makes this really slow
 	# ! video/x-raw,width=1280,height=720 \

#	! image/jpeg,width=1920,height=1080,framerate=30/1 ! jpegdec ! videoconvert \
#	! image/jpeg,width=1920,height=1080,framerate=30/1 ! jpegdec \
#	! video/x-raw,width=640,height=480 \
#	! video/x-raw,width=1920,height=1080 \

else
    # This pipeline works well without the cudascale line
    SCALEX=1280
    # SCALEY=720
    SCALEY=1440

    # Consider the bitrate value. bitrate=5000 means 5Mbit/s. It may be a good idea to use 1000 or 2000.

    gst-launch-1.0 -v v4l2src device=${DEVICE} ! image/jpeg,width=${INPUTX},height=${INPUTY},framerate=30/1 \
        ! jpegdec \
        ! cudaupload \
        ! cudaconvert \
        ! cudascale ! "video/x-raw(memory:CUDAMemory),width=${SCALEX},height=${SCALEY}" \
	! nvh264enc preset=low-latency-hp rc-mode=cbr bitrate=5000 gop-size=60 zerolatency=true \
	! rtph264pay config-interval=1 pt=96 \
        ! udpsink host=${DEST} port=${PORT}
fi

