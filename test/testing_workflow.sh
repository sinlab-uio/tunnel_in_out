# Make scripts executable
chmod +x udp_send.py udp_receive.py

# Terminal 1: Receiver at destination
./udp_receive.py 9999

# Terminal 2: Your TunnelServer
./TunnelServer 8888 --udp 2345 --tcp 7777

# Terminal 3: Your TunnelClient
./TunnelClient localhost:8888 --fwd-udp localhost:9999 --fwd-tcp localhost:80

# Terminal 4: Send file
./udp_send.py localhost 2345 < ../src/CMakeLists.txt
```

