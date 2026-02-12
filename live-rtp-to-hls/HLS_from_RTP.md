# Converting RTP into HLS with ffmpeg

ffmpeg can convert arriving RTP streams into MPEG segments continuously, keep the last few of them
around, and update a playlist file (MPD file in MPEG DASH terminology). Apple's HLS is less flexible
than MPEG DASH and cannot create playlists containing templates, but its playlist file must be
updated once for every segment.

LL-HLS (low latency HLS) may have fixed this, but it requires some Apple-proprietary things that we
don't know or have.

To perform the conversion, we expect that the RTP sender includes meta information (stream properties)
in the video stream and repeats it at regular intervals. A GStreamer source must add this as a parameter
to the `rtph264pay` module using `rtph264pay config-interval=1`.

## Session description

The conversion function requires an SDP description. While an ffmpeg sender generates this SDP file when
it is started, GStreamer doesn't do this, you must write it manually. This is an example:
```
v=0
o=- 0 0 IN IP4 127.0.0.1
s=GStreamer H.264 stream from SINLab
c=IN IP4 158.39.75.57
t=0 0
a=tool:GStreamer
a=type:unicast
m=video 5004 RTP/AVP 96
b=AS:5000
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1
```
 - `v=`stands for version.
 - `o=` contains information about the sender. This can be the sending user, session IDs
and session version. To transmit over IPv4, `IN IP4` is required. The final address is a valid address of the
sender. is a valid address of the sender.
 - `s=` contains a stream description.
 - `c=` is important because it contains the protocol and IP address of the receiver; meaning in this case the
machine where the converter runs.
 - `t=` provides the start and end timestamps for a stream. These are both 0 for live streams.
 - `a=tool:` describes the sender application. The receiver could use knowledge of that tool for special behaviours.
 - `a=type:` is informational only, and usually contains `broadcast` or `unicast`.
 - `m=` starts one media block in the SDP description. Several of these blocks may be present.
The line specifies in this case that the medium is video arriving on UDP part 5004, implemented as RTP over UDP.
The `96` is an example of a non-stanardized media type number (96-127 are allowed). Since it is not standardized,
`a=rtpmap:` and `a=fmtp:` lines are expected for more information.
 - `b=AS:" means bandwidth, application-specific maximum, and it is a promise from the sender not to exceed this
bitrate. The player can allocated buffers accordingly. That are also `b=TIAS:` and `b=CT:`.
 - `a=rtpmap:` makes a connection from the media type number to more information about the encoding. This is first
a string, followed by a sampling rate. The string is something that the player must know, there is no obvious
standard and the list grows with every RTP profile that is standardized.
 - `a=ftmp:" is highly codec-specific. Although old as a line, it hasn't been required for the mainstream video
codecs before H.264. Essentially, information about dynamic decisions is taken from the video headers and store
here, allowing the receiver to set up everything before the media stream arrives. For the reception of RTP and HLS,
it is a relatively bad idea because these settings change during the stream, but that information must be forced
in the stream itself on the sender side, and some parameters are still unavoidable here.


## Conversion with ffmpeg

This is our current conversion line:
```
ffmpeg \
	-fflags nobuffer+genpts \
        -max_delay 50000 \
        -reorder_queue_size 500 \
	-analyzeduration 100M -probesize 100M \
	-protocol_whitelist "file,rtp,udp" \
	-i video.sdp \
	-c:v libx264 -an \
	-tune zerolatency -preset veryfast \
	-x264-params keyint=15:min-keyint=15:scenecut=0:bframes=0 \
	-hls_time 2 -hls_list_size 10 -hls_flags delete_segments -f hls /var/www/hls/live/stream.m3u8
```

`-fflags nobuffer+genpts` instructs to avoid buffering as much as possible and regenerate MPEG transport
timing information (PTS, presentation timestamp). PTS helps the play compute the relative playout time
of samples, not absolute times.
`-max_delay 50000 -reorder_queue_size 500` establish some worst-case buffering that must cover both
the arriving bitrate and the expected jitter in terms of worst-case milliseconds.
When the source stream was thin enough, this was sufficient:
`-fflags nobuffer`
but since we are lost lots of packets, we use instead:
`-fflags nobuffer+genpts -max_delay 50000 -reorder_queue_size 500`.

`-analyzeduration 100M -probesize 100M` indicates how much data the converter may consuming until it gives
up attempting to parse the incoming RTP stream.
`-protocol_whitelist "file,rtp,udp" -i video.sdp`is the last parameter from the incoming side. It tells
the converter to read additional information (such as the arriving port and media type) from an SDP file.

Next comes the conversion.
`-an` means no audio.
`-c:v libx264` instructs to use the x264 library for repackaging and conversion.
`-tune zerolatency -preset veryfast` is an ffmpeg-wide instruction to encode for low-latency streaming.
`-x264-params keyint=15:min-keyint=15:scenecut=0:bframes=0` instructs x264 to ensure an I frame (not IBR)
every 15 frames, continue motion vector encoding across scene changes, and to avoid generating B-frames.
The keyint parameters are probably a bad idea; this should be revisited.
`-f hls` instructs to generated HLS output.
`-hls_time 2 -hls_list_size 10 -hls_flags delete_segments` instructs to generate 2-second longs segments,
where each segment is its own video, we are going to keep the last 10 of these segments around, and delete
the oldest one of the 10 when a new one is ready.
`/var/www/hls/live/stream.m3u8` gives the filename of the latest playlist.

### Some more tips

set `-fflags nobuffer+genpts+flush_packets` to reduce buffering. I'm not sure if that is really
    a good idea.

set `-hls_flags delete_segments+append_list+omit_endlist`, instead of the default. The default
    creates a new stream.m3u8 file for every segment, end the segment with an ENDLIST tag, and moves
    the new file to the previous one. I'm not sure if this is a good idea, I believe that this
    will fail with delete. But worth trying.

To use modern fragmented MP4 (.m4s) instead of classical MPEG2-Transport (.ts), add these parameters:
`-hls_segment_type fmp4 -hls_fmp4_init_filename init.mp4 `.
This creates the init.mp4 file (and adds it to the m3u8 file), and .m4s files that contain
the actual video data. These can be concatenated for playable video files. Can be extended
to several layers, but that's not our goal.
Definitely worth trying for our case.

### Server mode

To allow the server to experience a timeout but still go back to waiting for data from a streaming
client, use the following even before --fflags:
`-reconnect 1 -reconnect_streamed 1 -reconnect_delay_max 10`
However, for extensive resilience, use also a `while true; do ... done` loop in bash.

