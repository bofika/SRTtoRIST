include $(TOPDIR)/rules.mk

PKG_NAME:=srt-to-rist-gateway
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/$(PKG_NAME)
  SECTION:=net
  CATEGORY:=Network
  TITLE:=SRT to RIST gateway
  DEPENDS:=+libstdcpp +librist +srt +ffmpeg +libopenssl +libpthread +spdlog
  URL:=https://github.com/yourusername/srt-to-rist-gateway
  MAINTAINER:=Your Name <your.email@example.com>
endef

define Package/$(PKG_NAME)/description
  A gateway that receives SRT or RTSP video and sends it via RIST.
  Supports both listener and caller modes for SRT input.
  Built with FFmpeg for video processing capabilities.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(CP) ./CMakeLists.txt $(PKG_BUILD_DIR)/
	$(CP) ./config.json $(PKG_BUILD_DIR)/
	$(if $(wildcard ./init.d),$(CP) ./init.d $(PKG_BUILD_DIR)/)
endef

CMAKE_OPTIONS += \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_VERBOSE_MAKEFILE=ON \
	-DCMAKE_PREFIX_PATH="$(STAGING_DIR)/usr"

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/srt_to_rist_gateway $(1)/usr/bin/

	$(INSTALL_DIR) $(1)/etc/srt_to_rist_gateway
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/config.json $(1)/etc/srt_to_rist_gateway/

	$(INSTALL_DIR) $(1)/etc/init.d
	$(if $(wildcard $(PKG_BUILD_DIR)/init.d/srt-to-rist-gateway),\
		$(INSTALL_BIN) $(PKG_BUILD_DIR)/init.d/srt-to-rist-gateway $(1)/etc/init.d/srt-to-rist-gateway)
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
