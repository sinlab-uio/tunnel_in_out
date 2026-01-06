#!/usr/bin/env python3

# Usage:
# Slower (100ms between packets)
# ./udp_send.py localhost 2345 0.1 < file.txt
# Faster (1ms between packets)
# ./udp_send.py localhost 2345 0.001 < file.txt

import socket
import sys
import time

host = sys.argv[1]
port = int(sys.argv[2])
delay = float(sys.argv[3]) if len(sys.argv) > 3 else 0.01

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
count = 0

for line in sys.stdin:
    sock.sendto(line.encode(), (host, port))
    count += 1
    if count % 10 == 0:
        print(f"Sent {count} packets...", file=sys.stderr)
    time.sleep(delay)
    
print(f"Total: {count} packets sent", file=sys.stderr)
sock.close()

