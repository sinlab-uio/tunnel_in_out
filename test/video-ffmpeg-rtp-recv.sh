#!/bin/bash

# Play back from RTP
ffplay -protocol_whitelist "file,rtp,udp" -i video.sdp

