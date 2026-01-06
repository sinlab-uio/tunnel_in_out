#!/usr/bin/env python3
import socket
import sys
import time

# Usage: ./udp_send.py localhost 2345 < file.txt
host = sys.argv[1]
port = int(sys.argv[2])

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

for line in sys.stdin:
    sock.sendto(line.encode(), (host, port))
    time.sleep(0.01)  # 10ms between packets
    
sock.close()

