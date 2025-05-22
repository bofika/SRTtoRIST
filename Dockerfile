# 1) Base image + host tools
FROM ubuntu:22.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential ca-certificates wget git pkg-config cmake \
    libncurses5-dev libncursesw5-dev zlib1g-dev \
    gawk gettext libssl-dev xsltproc rsync file \
    unzip python3 python3-distutils

# 2) Create a workspace and fetch the SDK
WORKDIR /workspace
RUN wget https://downloads.openwrt.org/releases/23.05.3/targets/mediatek/filogic/openwrt-sdk-23.05.3-mediatek-filogic_gcc-12.3.0_musl.Linux-x86_64.tar.xz && \
    tar -xf openwrt-sdk-23.05.3-mediatek-filogic_gcc-12.3.0_musl.Linux-x86_64.tar.xz && \
    mv openwrt-sdk-23.05.3-mediatek-filogic_gcc-12.3.0_musl.Linux-x86_64 openwrt-sdk

# 3) Copy your package into the SDK
RUN mkdir -p openwrt-sdk/package/srt-to-rist-gateway
COPY src/             openwrt-sdk/package/srt-to-rist-gateway/src/
COPY Makefile         openwrt-sdk/package/srt-to-rist-gateway/
COPY CMakeLists.txt   openwrt-sdk/package/srt-to-rist-gateway/
COPY config.json      openwrt-sdk/package/srt-to-rist-gateway/

# 4) Enter the SDK and update feeds
WORKDIR /workspace/openwrt-sdk
RUN ./scripts/feeds update -a && ./scripts/feeds install -a

# 5) Build your package
RUN make defconfig && make package/srt-to-rist-gateway/compile V=s

# 6) The .ipk will end up under:
#    /workspace/openwrt-sdk/bin/packages/*/*/srt-to-rist-gateway_*.ipk
