#!/bin/bash

ffplay -f v4l2 -video_size 3840x1920 -input_format mjpeg -codec:v mjpeg -framerate 30 -fflags +nobuffer -i /dev/video0

