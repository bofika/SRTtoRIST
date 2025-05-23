# 1) Base image and install host‐side deps
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive \
    OPENWRT_SDK_VERSION=23.05.3-mediatek-filogic_gcc-12.3.0_musl.Linux-x86_64

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential ca-certificates wget git pkg-config cmake \
      libncurses5-dev libncursesw5-dev zlib1g-dev flex bison \
      gawk gettext libssl-dev xsltproc rsync file \
      unzip python3 python3-distutils python3-pkg-resources musl-tools libelf-dev && \
    rm -rf /var/lib/apt/lists/*


# 2) Download & unpack the OpenWRT SDK
WORKDIR /workspace
RUN wget https://downloads.openwrt.org/releases/23.05.3/targets/mediatek/filogic/openwrt-sdk-${OPENWRT_SDK_VERSION}.tar.xz && \
    tar xf openwrt-sdk-${OPENWRT_SDK_VERSION}.tar.xz && \ 
    mv openwrt-sdk-${OPENWRT_SDK_VERSION} openwrt-sdk && \
    mkdir -p openwrt-sdk/package/srt-to-rist-gateway

# 3) Copy in your package into the SDK
COPY CMakeLists.txt Makefile config.json /workspace/openwrt-sdk/package/srt-to-rist-gateway/
COPY src/ /workspace/openwrt-sdk/package/srt-to-rist-gateway/src/
COPY init.d/ /workspace/openwrt-sdk/package/srt-to-rist-gateway/init.d/

WORKDIR /workspace/openwrt-sdk

# 4) Build your package
RUN make defconfig && \
    make -j$(nproc) package/srt-to-rist-gateway/compile V=s

# 5) Copy the resulting .ipk out to /workspace
RUN cp bin/packages/*/*/srt-to-rist-gateway_*.ipk /workspace/ || \
    cp bin/targets/*/*/packages/srt-to-rist-gateway_*.ipk /workspace/