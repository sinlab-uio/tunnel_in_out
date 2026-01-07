#!/bin/bash

export PATH=$PATH:/home/griff/Install/tunnel/bin

# tunnel server listens for tunnel connects on port 1234
#                       for external TCP connections on port 2345
#                       for external UDP packets on port 2345
/Users/griff/Install/tunnel/bin/TunnelServer -t 2345 -u 2345 1234

