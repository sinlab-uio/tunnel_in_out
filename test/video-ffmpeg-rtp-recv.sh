#!/bin/bash

# Play back from RTP
ffplay -protocol_whitelist "rtp,udp" -i video.sdp

# Google says
# ffplay -fflags nobuffer -flags low_delay -probesize 32 -analyzeduration 1 -strict experimental -framedrop -f rtp rtp://[DESTINATION_IP]:[PORT]
# which is weird



