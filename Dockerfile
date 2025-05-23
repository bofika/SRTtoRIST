# 1) Base image and install host‐side deps
FROM ubuntu:22.04

ARG GITHUB_TOKEN

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
    mv openwrt-sdk-${OPENWRT_SDK_VERSION} openwrt-sdk

# 3) Configure feeds
WORKDIR /workspace/openwrt-sdk

RUN mkdir -p feeds/librist && \
    git clone https://${GITHUB_TOKEN}@github.com/nanake/librist-openwrt.git feeds/librist && \
    echo 'src-git packages https://github.com/openwrt/packages.git' > feeds.conf.default && \
    echo 'src-git srt https://github.com/openwrt/packages.git' >> feeds.conf.default && \
    ./scripts/feeds update -a && \
    ./scripts/feeds install -a

# 4) Copy in your package into the SDK
RUN mkdir -p package/srt-to-rist-gateway
COPY . package/srt-to-rist-gateway/

# 5) Build your package
RUN make defconfig && \
    make -j$(nproc) package/srt-to-rist-gateway/compile V=s

# 6) Copy the resulting .ipk out to /workspace
RUN find bin/ -name "srt-to-rist-gateway_*.ipk" -exec cp {} /workspace/ \;