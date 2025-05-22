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
  DEPENDS:=+librist +libsrt +libavformat +libavcodec +libavutil +libstdcpp
endef

define Package/$(PKG_NAME)/description
  A gateway that receives SRT or RTSP video and sends it via RIST.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(CP) ./CMakeLists.txt $(PKG_BUILD_DIR)/
	$(CP) ./config.json $(PKG_BUILD_DIR)/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/srt_to_rist_gateway $(1)/usr/bin/

	$(INSTALL_DIR) $(1)/etc/srt_to_rist
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/config.json $(1)/etc/srt_to_rist/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
