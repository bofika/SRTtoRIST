include $(TOPDIR)/rules.mk

# Name and version
PKG_NAME:=stream_relay
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

# Package source
PKG_SOURCE_PROTO:=git
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/user/stream_relay.git
PKG_SOURCE_VERSION:=HEAD

# Maintainer and license info
PKG_MAINTAINER:=Your Name <your.email@example.com>
PKG_LICENSE:=MIT

# Dependencies
include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

# Package definition
define Package/stream_relay
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Stream Relay for SRT/RTSP to RIST
  URL:=https://github.com/user/stream_relay
  DEPENDS:=+librist +libsrt +libavformat +libavcodec +libavutil +libstdcpp
endef

# Package description
define Package/stream_relay/description
  A lightweight application for OpenWRT that receives video via SRT or RTSP,
  sends the stream over RIST using libRIST, and provides network feedback
  to the encoder via UDP.
endef

# Package preparation
define Build/Prepare
	$(call Build/Prepare/Default)
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/third_party/nlohmann
	$(DOWNLOAD) https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp \
		$(PKG_BUILD_DIR)/third_party/nlohmann/json.hpp
endef

# Package installation
define Package/stream_relay/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/stream_relay $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/etc/stream_relay
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/config.json $(1)/etc/stream_relay/
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/init.d/stream_relay $(1)/etc/init.d/
endef

$(eval $(call BuildPackage,stream_relay))