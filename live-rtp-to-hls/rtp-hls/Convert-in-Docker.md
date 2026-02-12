# ffmpeg start in Docker

This creates a time Docker image.
The image does not do any NATting, but uses the host's UDP and TPC ports.
Only UDP port 5004 is exposed.

To create a Docker image from this:
```
cd rtp-hls
docker compose up --build -d
```

More Docker commands:
```
docker compose logs -f          # tail logs
docker compose restart          # restart after config changes
docker compose down             # stop
```



