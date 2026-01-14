#!/bin/bash

echo "================================================================================"
echo "= Starting the TunnelClient on the inside of the firewall"
echo "= TunnelClient $* -t localhost:1235 -u localhost:1235 webrtc.mlab.no:3478"
echo "================================================================================"

# tunnel client makes a tunnel to webrtc.mlab.no:3478
#               forwards TCP connections from the tunnel to localhost 1235
#               forwards UDP packets from the tunnel to localhost:1235
$HOME/Install/bin/TunnelClient $* -t localhost:1235 -u localhost:1235 158.42.251.4:5201

