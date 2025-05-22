FROM ubuntu:22.04

# 1) Install host tools
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential ca-certificates wget git pkg-config cmake \
    libncurses5-dev libncursesw5-dev zlib1g-dev \
    gawk gettext libssl-dev xsltproc rsync file

# 2) Create and switch to /workspace
WORKDIR /workspace

# 3) Download & unpack SDK here (so openwrt-sdk ends up under /workspace)
RUN wget https://downloads.openwrt.org/releases/23.05.3/targets/mediatek/filogic/\
openwrt-sdk-23.05.3-mediatek-filogic_gcc-12.3.0_musl.Linux-x86_64.tar.xz && \
    tar -xf openwrt-sdk-23.05.3-mediatek-filogic_gcc-12.3.0_musl.Linux-x86_64.tar.xz && \
    mv openwrt-sdk-23.05.3-mediatek-filogic_gcc-12.3.0_musl.Linux-x86_64 openwrt-sdk

# 4) Copy in your package (make sure these COPYs come _after_ WORKDIR!)
RUN mkdir -p openwrt-sdk/package/srt-to-rist-gateway
COPY src/ openwrt-sdk/package/srt-to-rist-gateway/src/
COPY Makefile CMakeLists.txt config.json openwrt-sdk/package/srt-to-rist-gateway/

# 5) Enter the SDK
WORKDIR /workspace/openwrt-sdk

# 6) Now feeds exist
RUN ./scripts/feeds update -a && ./scripts/feeds install -a && \
    rm -rf build_dir/target-* staging_dir/target-*

# 7) Build your package
RUN make defconfig && make package/srt-to-rist-gateway/compile V=s

# 8) Leave the IPK in /workspace
