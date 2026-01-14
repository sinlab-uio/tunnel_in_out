#!/bin/bash

echo "================================================================================"
echo "= Starting the TunnelServer on the outside of the firewall"
echo "= TunnelServer $* -t 5349 -u 5349 1234"
echo "================================================================================"

# Some useful ports:
# 3478 STUN and TURN services, use to discover public addresses and relay media traffic
# 5349 used to relay media for WebRTC in conjunction with STUN and TURN

# tunnel server listens for tunnel connects on port 1234
#                       for external TCP connections on port 2345
#                       for external UDP packets on port 2345
$HOME/Install/tunnel/bin/TunnelServer $* -t 5349 -u 5349 3478

