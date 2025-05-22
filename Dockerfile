# Dockerfile: Build SRTtoRIST under OpenWRT SDK for GL-MT3000
FROM ubuntu:22.04

# 1) Install host tools
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
      build-essential ca-certificates wget git pkg-config cmake \
      libncurses5-dev libncursesw5-dev zlib1g-dev \
      gawk gettext libssl-dev xsltproc rsync file

# 2) Download & explode the OpenWRT SDK
RUN wget https://downloads.openwrt.org/releases/23.05.3/targets/mediatek/filogic/openwrt-sdk-23.05.3-mediatek-filogic_gcc-12.3.0_musl.Linux-x86_64.tar.xz && \
    tar -xf openwrt-sdk-23.05.3-mediatek-filogic_gcc-12.3.0_musl.Linux-x86_64.tar.xz && \
    mv openwrt-sdk-23.05.3-mediatek-filogic_gcc-12.3.0_musl.Linux-x86_64 openwrt-sdk


WORKDIR /workspace

# 3) Copy your package into the SDK
#    (assumes Docker build context is your repo root)
RUN mkdir -p /workspace/openwrt-sdk/package/srt-to-rist-gateway
COPY src   /workspace/openwrt-sdk/package/srt-to-rist-gateway/src
COPY Makefile CMakeLists.txt config.json /workspace/openwrt-sdk/package/srt-to-rist-gateway/
# if you have an init.d dir:
COPY init.d /workspace/openwrt-sdk/package/srt-to-rist-gateway/init.d

WORKDIR /workspace/openwrt-sdk

# 4) Prepare feeds & clean any stale state
RUN ./scripts/feeds update -a && ./scripts/feeds install -a && \
    rm -rf build_dir/target-* staging_dir/target-*

# 5) Build your package only (verbose)
RUN make defconfig && \
    make package/srt-to-rist-gateway/compile V=s

# 6) Final .ipk lands here:
#    bin/packages/*/*/srt-to-rist-gateway_*.ipk
