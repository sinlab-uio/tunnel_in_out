# Making an HLS version of BigBuckBunny

## Make preparations
- `sudo apt install ffmpeg`

## Download BigBuckBunny
wget http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4

## Create HLS segments
sudo mkdir -p /var/www/hls
sudo ffmpeg -i BigBuckBunny.mp4 \
  -c:v libx264 -c:a aac \
  -hls_time 4 \
  -hls_list_size 0 \
  -hls_segment_filename '/var/www/hls/segment%03d.ts' \
  /var/www/hls/playlist.m3u8

## Set ownership
sudo chown -R rtc:rtc /var/www/hls

## Web page to serve HLS content

```
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>HLS Stream Test</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 1200px;
            margin: 50px auto;
            padding: 20px;
        }
        .player-container {
            margin: 20px 0;
            background: #000;
        }
        video {
            width: 100%;
            max-width: 800px;
        }
        .info {
            background: #f0f0f0;
            padding: 15px;
            border-radius: 5px;
            margin: 20px 0;
        }
        h2 {
            margin-top: 40px;
        }
    </style>
</head>
<body>
    <h1>HLS Stream Test</h1>

    <div class="info">
        <strong>Stream URL:</strong> <span id="streamUrl"></span><br>
        <strong>Latency Mode:</strong> <span id="latencyMode">Standard</span>
    </div>

    <!-- Option 1: hls.js (Recommended for low-latency) -->
    <h2>Player 1: hls.js (Recommended for Low-Latency)</h2>
    <div class="player-container">
        <video id="video-hls" controls></video>
    </div>
    <div class="info">
        <strong>Current Time:</strong> <span id="currentTime">-</span><br>
        <strong>Buffer Length:</strong> <span id="bufferLength">-</span><br>
        <strong>Latency:</strong> <span id="latency">-</span>
    </div>

    <!-- Option 2: Video.js -->
    <h2>Player 2: Video.js</h2>
    <link href="https://vjs.zencdn.net/8.10.0/video-js.css" rel="stylesheet" />
    <div class="player-container">
        <video id="video-js" class="video-js vjs-default-skin" controls></video>
    </div>

    <!-- Load hls.js -->
    <script src="https://cdn.jsdelivr.net/npm/hls.js@latest"></script>

    <!-- Load Video.js -->
    <script src="https://vjs.zencdn.net/8.10.0/video.min.js"></script>

    <script>
        // Your HLS stream URL
        const streamUrl = '/hls/playlist.m3u8';
        document.getElementById('streamUrl').textContent = streamUrl;

        // ========== HLS.JS PLAYER (Better for low-latency) ==========
        const videoHls = document.getElementById('video-hls');

        if (Hls.isSupported()) {
            const hls = new Hls({
                // Low-latency optimizations
                maxBufferLength: 4,           // Reduce buffer to 4 seconds
                maxMaxBufferLength: 6,        // Max buffer ceiling
                liveSyncDurationCount: 2,     // Stay closer to live edge
                liveMaxLatencyDurationCount: 4,
                highBufferWatchdogPeriod: 1,  // Aggressive buffer management
                enableWorker: true,
                lowLatencyMode: true          // Enable if using LL-HLS
            });

            hls.loadSource(streamUrl);
            hls.attachMedia(videoHls);

            hls.on(Hls.Events.MANIFEST_PARSED, function() {
                console.log('HLS.js: Manifest loaded');
                videoHls.play();
            });

            hls.on(Hls.Events.ERROR, function(event, data) {
                console.error('HLS.js error:', data);
            });

            // Display stats
            setInterval(() => {
                if (hls.media) {
                    const buffered = hls.media.buffered;
                    const currentTime = hls.media.currentTime;
                    const bufferLength = buffered.length > 0 ?
                        (buffered.end(buffered.length - 1) - currentTime).toFixed(2) : 0;

                    document.getElementById('currentTime').textContent =
                        currentTime.toFixed(2) + 's';
                    document.getElementById('bufferLength').textContent =
                        bufferLength + 's';

                    // Calculate approximate latency (distance from live edge)
                    if (hls.levels.length > 0) {
                        const latency = hls.latency || '-';
                        document.getElementById('latency').textContent =
                            (typeof latency === 'number' ? latency.toFixed(2) + 's' : latency);
                    }
                }
            }, 1000);

        } else if (videoHls.canPlayType('application/vnd.apple.mpegurl')) {
            // Native HLS support (Safari)
            videoHls.src = streamUrl;
            videoHls.addEventListener('loadedmetadata', function() {
                videoHls.play();
            });
        }

        // ========== VIDEO.JS PLAYER ==========
        const playerVjs = videojs('video-js', {
            html5: {
                vhs: {
                    // Low-latency settings for Video.js
                    overrideNative: true,
                    smoothQualityChange: true,
                    maxPlaylistRetries: 10,
                    bandwidth: 4096000
                }
            },
            sources: [{
                src: streamUrl,
                type: 'application/x-mpegURL'
            }],
            autoplay: true,
            controls: true,
            fluid: true
        });

        // Log Video.js events
        playerVjs.on('error', function(e) {
            console.error('Video.js error:', playerVjs.error());
        });
    </script>
</body>
</html>
```

Call this `index.html` and change your Nginx server settings to include
the line 'index index.html;' in the location section.


