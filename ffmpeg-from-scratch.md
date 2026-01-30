# ffmpeg from scratch

ffmpeg must currently be compiled from scratch to stream H.264 over MPEG-2-TS/RTP/UDP/IP. The reason is that the stream description in SDP is not generated in advance (for the obvious reason that the stream is live), and the video format information must therefore be contained in the TS stream itself. This feature is not included in ffmpeg by default.

## Prepare Ubuntu

Some of these things that need to be pre-installed are Ubuntu-only but most are generic for Linux distros that use apt.

```
sudo apt update
sudo apt upgrade
sudo apt-get -y install autoconf automake libtool pkg-config texinfo wget build-essential cmake git-core \
    libass-dev libfreetype6-dev libgnutls28-dev libmp3lame-dev libsdl2-dev libva-dev libvdpau-dev \
    libvorbis-dev libxcb1-dev libxcb-shm0-dev libxcb-xfixes0-dev meson ninja-build yasm zlib1g-dev \
    libunistring-dev libaom-dev libdav1d-dev nasm libx264-dev libx265-dev libnuma-dev libvpx-dev \
    libfdk-aac-dev libopus-dev libdav1d-dev
```
## Fetch ffmpeg

```
git clone https://git.ffmpeg.org/ffmpeg.git
```

## Configure ffmpeg

NOTE: this part of the description is incomplete. Describe how NVENC is added. Describe how H.264 meta information is included into RTP streams.


Change into the directory where you extracted ffmpeg from GIT. After that:

```
export FFMPEG_SRC=$HOME/GIT/ffmpeg
export FFMPEG_BUILD=$HOME/ffmpeg_build
export FFMPEG_INSTALL=$HOME/Install/ffmpeg

mkdir $FFMPEG_BUILD
cd $FFMPEG_SRC
PATH="$FFMPEG_INSTALL/bin:$PATH" PKG_CONFIG_PATH="$FFMPEG_BUILD/lib/pkgconfig" ./configure \
  --prefix="$FFMPEG_BUILD" \
  --pkg-config-flags="--static" \
  --extra-cflags="-I$FFMPEG_BUILD/include" \
  --extra-ldflags="-L$FFMPEG_BUILD/lib" \
  --extra-libs="-lpthread -lm" \
  --ld="g++" \
  --bindir="$HOME/bin" \
  --enable-gpl \
  --enable-gnutls \
  --enable-libaom \
  --enable-libass \
  --enable-libfdk-aac \
  --enable-libfreetype \
  --enable-libmp3lame \
  --enable-libopus \
  --enable-libsvtav1 \
  --enable-libdav1d \
  --enable-libvorbis \
  --enable-libvpx \
  --enable-libx264 \
  --enable-libx265 \
  --enable-nonfree
PATH="$FFMPEG_INSTALL/bin:$PATH" make && make install
hash -r
```
