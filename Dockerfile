# 1) Base image + host tools
FROM ubuntu:22.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential ca-certificates wget git pkg-config cmake \
    libncurses5-dev libncursesw5-dev zlib1g-dev \
    gawk gettext libssl-dev xsltproc rsync file \
    unzip python3 python3-distutils python3-pkg-resources && \
    ln -sf /usr/bin/python3 /usr/bin/python

# 2) Create workspace and fetch the OpenWRT SDK
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
# (optional) COPY init.d openwrt-sdk/package/srt-to-rist-gateway/init.d/

# 4) Enter the SDK
WORKDIR /workspace/openwrt-sdk

# 5) Inject only the librist feed, update all feeds, then install RIST & SRT
RUN \
  echo 'src-git librist https://github.com/Haivision/librist.git' >> feeds.conf.default && \
  ./scripts/feeds update -a && \
  ./scripts/feeds install librist srt && \
  ./scripts/feeds install -a

# 6) Build your package
RUN make defconfig && make package/srt-to-rist-gateway/compile V=s

# 7) The resulting .ipk will land under:
#    /workspace/openwrt-sdk/bin/packages/*/*/srt-to-rist-gateway_*.ipk
