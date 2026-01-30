#!/bin/bash

MY_DEFAULT_IP=`hostname -I | head -1 | xargs`

SDP_FILE=sdp-live-${1:-${MY_DEFAULT_IP}}.sdp

echo "================================================================================"
echo "= My default IP is ${MY_DEFAULT_IP}"
echo "= SDP file is ${SDP_FILE}"
echo "================================================================================"

# Play back from RTP
ffplay \
	-max_delay 10000 -sync video \
	-protocol_whitelist "file,rtp,udp" -i ${SDP_FILE}

#	-loglevel debug \

# Google says
# ffplay -fflags nobuffer -flags low_delay -probesize 32 -analyzeduration 1 -strict experimental -framedrop -f rtp rtp://[DESTINATION_IP]:[PORT]
# which is weird



