#!/usr/bin/env python3
import socket

# Usage: ./udp_receive.py 9999
import sys
port = int(sys.argv[1]) if len(sys.argv) > 1 else 9999

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', port))

print(f"Listening on UDP port {port}...")
while True:
    data, addr = sock.recvfrom(65536)
    print(f"From {addr[0]}:{addr[1]}: {data.decode()}", end='')

