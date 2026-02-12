FROM alpine:3.19

# Install ffmpeg — Alpine's package is lean (~50MB total image)
RUN apk add --no-cache ffmpeg

# Copy entrypoint
COPY ffmpeg-start.sh /usr/local/bin/ffmpeg-start.sh
RUN chmod +x /usr/local/bin/ffmpeg-start.sh

# HLS output will be written here (mount a volume)
VOLUME ["/hls-output"]

# Expose the RTP port (UDP) — adjust to your actual port
EXPOSE 5004/udp

ENTRYPOINT ["/usr/local/bin/ffmpeg-start.sh"]

