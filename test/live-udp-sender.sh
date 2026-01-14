#!/bin/bash

export PATH=$HOME/Install/tunnel/bin

echo "================================================================================"
echo "= Starting a UDP sender"
echo "= Python_SendTest.py webrtc.mlab.no 5349 < DUMP.txt"
echo "================================================================================"

# iperf client sends UDP packets to port 2345 (outer tunnel UDP)
/opt/homebrew/bin/python3 ./Python_SendTest.py webrtc.mlab.no 5349 < DUMP.txt

