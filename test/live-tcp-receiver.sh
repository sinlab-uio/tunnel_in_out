#!/bin/bash

echo "================================================================================"
echo "= Starting a TCP receiver"
echo "= nc -l 1235"
echo "================================================================================"

# iperf server waits for UDP packets on port 1235
nc -l 1235

