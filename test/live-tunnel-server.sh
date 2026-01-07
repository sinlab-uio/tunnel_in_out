#!/bin/bash

export PATH=$PATH:/home/griff/Install/tunnel/bin

echo "================================================================================"
echo "= Starting the TunnelServer on the outside of the firewall"
echo "= TunnelServer -t 2345 -u 2345 1234"
echo "================================================================================"

# tunnel server listens for tunnel connects on port 1234
#                       for external TCP connections on port 2345
#                       for external UDP packets on port 2345
TunnelServer -t 2345 -u 2345 1234

