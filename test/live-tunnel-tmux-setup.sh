#!/bin/bash

SESSION=LiveTunnelTmuxSession-01

tmux new-session -d -s $SESSION

tmux rename-window "Main"

tmux split-window -h -t $SESSION:0.0

# TOP LEFT: 0.0
# TunnelServer running on webrtc.mlab.no
tmux send-keys -t 0.0 'ssh webrtc.mlab.no ~/Install/tunnel/bin/TunnelServer $* -t 9090 -u 12121 5201' C-M

# TOP RIGHT: 0.1
# TunnelCLient running locally
tmux send-keys -t 0.1 'sleep 1; ~/Install/tunnel/bin/TunnelClient $* -t localhost:9090 -u localhost:12121 webrtc.mlab.no:5201' C-M

tmux attach-session -t $SESSION

