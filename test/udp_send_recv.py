#!/usr/bin/env python3
import socket
import sys
import time
import select

host = sys.argv[1]
port = int(sys.argv[2])

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', 0))  # Bind to any available port

print(f"Sending to {host}:{port}, listening on port {sock.getsockname()[1]}")

# Send test message
test_msg = "Hello from Python!"
sock.sendto(test_msg.encode(), (host, port))
print(f"Sent: {test_msg}")

# Wait for response (with timeout)
sock.settimeout(5.0)
try:
    data, addr = sock.recvfrom(65536)
    print(f"Received response from {addr[0]}:{addr[1]}: {data.decode()}")
except socket.timeout:
    print("No response received (timeout)")
    
sock.close()

