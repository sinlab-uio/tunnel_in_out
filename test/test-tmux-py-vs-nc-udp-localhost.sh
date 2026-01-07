#!/bin/bash

export PATH=$PATH:/home/griff/Install/tunnel/bin:/Users/griff/Install/tunnel/bin
SESSION=TunnelInOut-UDP-Test-01

tmux new-session -d -s $SESSION

tmux rename-window "Main"

tmux split-window -v -t $SESSION:0.0
tmux split-window -h -t $SESSION:0.1
tmux split-window -h -t $SESSION:0.0

# TOP LEFT: 0.0
# iperf client sends UDP packets to port 2345 (outer tunnel UDP)
tmux send-keys -t 0.0 'sleep 4; python3 ./Python_SendTest.py localhost 2345 < DUMP.txt' C-M
# tmux send-keys -t 0.0 'sleep 2; iperf3-darwin -V -u -c localhost -p 2345  -b 0 -t 5' C-M

# BOTTOM LEFT: 0.2
# tunnel server listens for tunnel connects on port 1234
#                       for external TCP connections on port 2345
#                       for external UDP packets on port 2345
tmux send-keys -t 0.2 '/Users/griff/Install/tunnel/bin/TunnelServer -t 2345 -u 2345 1234' C-M

# BOTTOM RIGHT: 0.3
# tunnel client makes a tunnel to localhost:1234
#               forwards TCP connections from the tunnel to localhost 1235
#               forwards UDP packets from the tunnel to localhost:1235
tmux send-keys -t 0.3 'sleep 1; /Users/griff/Install/tunnel/bin/TunnelClient -t localhost:1235 -u localhost:1235 localhost:1234' C-M

# TOP LEFT: 0.1
# iperf server waits for UDP packets on port 1235
tmux send-keys -t 0.1 'nc -u -l 1235' C-M

tmux attach-session -t $SESSION

