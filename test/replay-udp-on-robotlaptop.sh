#!/bin/bash
# tcprewrite -v --srcipmap=0.0.0.0/0:192.168.50.3/32 --dstipmap=0.0.0.0/0:192.168.50.152/32 -i udponly.pcap -o udponly-local.pcap
tcpreplay -v udponly-local.pcap -i en0

