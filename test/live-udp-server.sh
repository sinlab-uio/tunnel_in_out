#!/bin/bash

echo "================================================================================"
echo "= Starting a UDP server"
echo "= nc -u -l 1235"
echo "================================================================================"

# iperf server waits for UDP packets on port 1235
nc -u -l 1235

