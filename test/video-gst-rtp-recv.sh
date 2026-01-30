#!/bin/bash

PORT=${1:-5004}"

echo "================================================================================"
echo "= Receiving RTP on port ${PORT}"
echo "================================================================================"

# Play back from RTP

gst-launch-1.0 -v udpsrc port=${PORT} caps = "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" \
	! rtph264depay \
	! decodebin \
	! videoconvert \
	! autovideosink
