# 1) Base image and install host‐side deps
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive \
    OPENWRT_SDK_VERSION=23.05.3-mediatek-filogic_gcc-12.3.0_musl.Linux-x86_64

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential ca-certificates wget git pkg-config cmake \
      libncurses5-dev libncursesw5-dev zlib1g-dev \
      gawk gettext libssl-dev xsltproc rsync file \
      unzip python3 python3-distutils python3-pkg-resources musl-tools && \
    rm -rf /var/lib/apt/lists/*

# 2) Download & unpack the OpenWRT SDK
WORKDIR /workspace
RUN wget https://downloads.openwrt.org/releases/23.05.3/targets/mediatek/filogic/openwrt-sdk-${OPENWRT_SDK_VERSION}.tar.xz && \
    tar xf openwrt-sdk-${OPENWRT_SDK_VERSION}.tar.xz && \
    mv openwrt-sdk-${OPENWRT_SDK_VERSION} openwrt-sdk

# 3) Copy in your package into the SDK
COPY . /workspace/openwrt-sdk/package/srt-to-rist-gateway/

WORKDIR /workspace/openwrt-sdk

# 4) Add external librist & SRT feeds, update & install them
RUN echo 'src-git packages https://git.openwrt.org/feed/packages.git' >> feeds.conf.default && \
    echo 'src-git librist https://github.com/nanake/librist.git' >> feeds.conf.default && \
    echo 'src-git srt https://github.com/Haivision/srt.git' >> feeds.conf.default && \
    ./scripts/feeds update -a && \
    ./scripts/feeds install -a

# 5) Enable required packages in config
RUN echo 'CONFIG_PACKAGE_librist=y' >> .config && \
    echo 'CONFIG_PACKAGE_srt=y' >> .config && \
    echo 'CONFIG_PACKAGE_libavformat=y' >> .config && \
    echo 'CONFIG_PACKAGE_libavcodec=y' >> .config && \
    echo 'CONFIG_PACKAGE_libavutil=y' >> .config

# 6) Clean stale state
RUN rm -rf build_dir staging_dir/target-* staging_dir/toolchain-*

# 7) Build your package
RUN make defconfig && \
    make package/srt-to-rist-gateway/compile V=s

# 8) Copy the resulting .ipk out to /workspace
RUN cp bin/packages/*/*/srt-to-rist-gateway_*.ipk /workspace/