#!/bin/bash

SELF=192.168.50.3

# Play back from RTP
ffplay -protocol_whitelist "file,rtp,udp" -i sdp-live-${SELF}.sdp

# Google says
# ffplay -fflags nobuffer -flags low_delay -probesize 32 -analyzeduration 1 -strict experimental -framedrop -f rtp rtp://[DESTINATION_IP]:[PORT]
# which is weird



