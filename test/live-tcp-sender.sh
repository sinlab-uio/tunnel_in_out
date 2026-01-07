#!/bin/bash

echo "================================================================================"
echo "= Starting a TCP sender"
echo "= nc webrtc.mlab.no 5349 < DUMP.txt"
echo "================================================================================"

nc webrtc.mlab.no 5349 < DUMP.txt

