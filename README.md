# SRT to RIST Gateway

This project provides a small gateway that receives video over SRT or RTSP and forwards it via RIST. It is intended to be built as an OpenWRT package.

## Required packages

When building against the OpenWRT SDK make sure the following development packages are present:

- `libsrt1.5-openssl`
- `libsrt-openssl-dev`
- `librist-dev`
- `libavformat-dev`
- `libavcodec-dev`
- `libavutil-dev`
- `libopenssl`
- `libpthread`

These provide the SRT, RIST and FFmpeg libraries used by the gateway.

## Building with the OpenWRT SDK

1. **Obtain the SDK** for your target device from [openwrt.org](https://openwrt.org/). Extract it and enter the SDK directory.
2. **Clone and install library packages**:

   ```sh
   mkdir -p package/libs
   git clone https://github.com/Haivision/srt.git      openwrt-srt
   git clone https://code.videolan.org/rist/librist.git openwrt-librist
   cp -r openwrt-srt       package/libs/srt
   cp -r openwrt-librist   package/libs/librist
   ```

3. **Update and install feeds** to obtain FFmpeg and other dependencies:

   ```sh
   ./scripts/feeds update -a
   ./scripts/feeds install -a -f
   ./scripts/feeds install ffmpeg libavcodec libavutil libavformat
   ```

4. **Add this project** to the `package` directory of the SDK:

   ```sh
   mkdir -p package/srt-to-rist-gateway
   cp -r /path/to/srt-to-rist-gateway/* package/srt-to-rist-gateway/
   ```

5. **Select the required packages** using `make menuconfig` (or edit `.config` directly). Enable the packages listed above together with `srt-to-rist-gateway`.
6. **Build the package**:

   ```sh
   make package/srt-to-rist-gateway/compile -j$(nproc)
   ```

The resulting `.ipk` file will be found in `bin/packages/*/`. Install it on your OpenWRT device with `opkg install <package>.ipk`.

