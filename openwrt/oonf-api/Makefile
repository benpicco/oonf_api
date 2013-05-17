include $(TOPDIR)/rules.mk

PKG_NAME:=oonf-api
PKG_VERSION:=2013.05-17
#PKG_REV:=ec2bca8f28b4d14df049fc81e177160ca0e3c3a3
#PKG_RELEASE:=$(PKG_REV)

#PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
#PKG_SOURCE_URL:=http://olsr.org/git/oonf_api.git
#PKG_SOURCE_PROTO:=git
#PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
#PKG_SOURCE_VERSION:=$(PKG_REV)

CMAKE_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_SOURCE_DIR=$(CURDIR)/../../
CMAKE_OPTIONS=-DOONF_DO_NOT_REGISTER_PACKAGE:Bool=true \
              -DOONF_LOGGING_LEVEL:String=warn \
              -DCMAKE_BUILD_TYPE:String=MinSizeRel \
              -DOONF_NO_TESTING:Bool=true \
              -DINSTALL_LIB_DIR:Path=lib/oonf \
              -DINSTALL_INCLUDE_DIR:Path=include/oonf \
              -DINSTALL_CMAKE_DIR:Path=lib/oonf

define Package/oonf-api-git/template
	SECTION:=net
	CATEGORY:=Network
	MAINTAINER:=Henning Rogge <hrogge@gmail.com>
	SUBMENU:=OLSR.org network framework
	URL:=http://www.olsr.org/
endef

define Package/oonf-api-git-common
	$(call Package/oonf-api-git/template) 
	TITLE:=Common-API (generic helpers functions) 
endef

define Package/oonf-api-git-config
	$(call Package/oonf-api-git/template) 
	TITLE:=Config-API (generic configuration library)
	DEPENDS:=+oonf-api-git-common
endef

define Package/oonf-api-git-rfc5444
	$(call Package/oonf-api-git/template) 
	TITLE:=RFC5444-API (generic parser/generator for RFC5444)
	DEPENDS:=+oonf-api-git-common
endef

define Package/oonf-api-git-core
	$(call Package/oonf-api-git/template) 
	TITLE:=Core-API (basic infrastructure/scheduler code)
	DEPENDS:=+oonf-api-git-common +oonf-api-git-config +librt
endef

define Package/oonf-api-git-subsystems
	$(call Package/oonf-api-git/template) 
	TITLE:=OONF subsystems (high level infrastructure)
	DEPENDS:=+oonf-api-git-common +oonf-api-git-config +oonf-api-git-rfc5444 +oonf-api-git-core
endef

define Package/oonf-plugin-git-cfgio-file
	$(call Package/oonf-api-git/template) 
	TITLE:=File input/output configuration plugin
	DEPENDS:=+oonf-api-git-core +oonf-api-git-subsystems +ldconfig
endef

define Package/oonf-plugin-git-cfgparser-compact
	$(call Package/oonf-api-git/template) 
	TITLE:=Compact configurationf format plugin
	DEPENDS:=+oonf-api-git-core +oonf-api-git-subsystems +ldconfig
endef

define Package/oonf-plugin-git-httptelnet
	$(call Package/oonf-api-git/template) 
	TITLE:=HTTP to Telnet bridge plugin
	DEPENDS:=+oonf-api-git-core +oonf-api-git-subsystems +ldconfig
endef

define Package/oonf-plugin-git-layer2-viewer
	$(call Package/oonf-api-git/template) 
	TITLE:=Telnet viewer for layer2 data
	DEPENDS:=+oonf-api-git-core +oonf-api-git-subsystems +ldconfig
endef

define Package/oonf-plugin-git-nl80211-listener
	$(call Package/oonf-api-git/template) 
	TITLE:=Probing plugin for nl80211
	DEPENDS:=+oonf-api-git-core +oonf-api-git-subsystems +ldconfig +libnl-tiny
endef

define Package/oonf-plugin-git-plugin-controller
	$(call Package/oonf-api-git/template) 
	TITLE:=Remote loading and unloading of plugins
	DEPENDS:=+oonf-api-git-core +oonf-api-git-subsystems +ldconfig
endef

define Package/oonf-plugin-git-remotecontrol
	$(call Package/oonf-api-git/template) 
	TITLE:=Remote debugging and configuration plugin
	DEPENDS:=+oonf-api-git-core +oonf-api-git-subsystems +ldconfig
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include -I${STAGING_DIR}/usr/include/libnl-tiny

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/oonf/common \
		       $(1)/usr/include/oonf/config \
	               $(1)/usr/include/oonf/rfc5444 \
	               $(1)/usr/include/oonf/core \
	               $(1)/usr/include/oonf/core/os_linux \
	               $(1)/usr/include/oonf/subsystems \
	               $(1)/usr/include/oonf/subsystems/os_linux
        
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/include/oonf/common/*.h \
		$(1)/usr/include/oonf/common
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/include/oonf/config/*.h \
		$(1)/usr/include/oonf/config
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/include/oonf/rfc5444/*.h \
		$(1)/usr/include/oonf/rfc5444
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/include/oonf/core/*.h \
		$(1)/usr/include/oonf/core
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/include/oonf/core/os_linux/*.h \
		$(1)/usr/include/oonf/core/os_linux
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/include/oonf/subsystems/*.h \
		$(1)/usr/include/oonf/subsystems
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/include/oonf/subsystems/os_linux/*.h \
		$(1)/usr/include/oonf/subsystems/os_linux

	$(INSTALL_DIR) $(1)/usr/lib/oonf/
	$(INSTALL_BIN) \
		$(PKG_INSTALL_DIR)/usr/lib/oonf/lib*.{so*,a} \
		$(1)/usr/lib/oonf/
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/lib/oonf/OONF*.cmake                                                  \
		$(1)/usr/lib/oonf/
endef

define Package/oonf-api-git-common/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_common*.so* $(1)/usr/lib/oonf
endef

define Package/oonf-api-git-config/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_config*.so* $(1)/usr/lib/oonf
endef

define Package/oonf-api-git-rfc5444/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_rfc5444*.so* $(1)/usr/lib/oonf
endef

define Package/oonf-api-git-core/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_core*.so* $(1)/usr/lib/oonf
endef

define Package/oonf-api-git-subsystems/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_subsystems*.so* $(1)/usr/lib/oonf
endef

define Package/oonf-plugin-git-cfgio-file/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_cfgio_file*.so* $(1)/usr/lib/oonf
endef

define Package/oonf-plugin-git-cfgparser-compact/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_cfgparser_compact*.so* $(1)/usr/lib/oonf
endef

define Package/oonf-plugin-git-httptelnet/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_httptelnet*.so* $(1)/usr/lib/oonf
endef

define Package/oonf-plugin-git-layer2-viewer/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_layer2_viewer*.so* $(1)/usr/lib/oonf
endef

define Package/oonf-plugin-git-nl80211-listener/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_nl80211_listener*.so* $(1)/usr/lib/oonf
endef

define Package/oonf-plugin-git-plugin-controller/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_plugin_controller*.so* $(1)/usr/lib/oonf
endef

define Package/oonf-plugin-git-remotecontrol/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_remotecontrol*.so* $(1)/usr/lib/oonf
endef

$(eval $(call BuildPackage,oonf-api-git-common))
$(eval $(call BuildPackage,oonf-api-git-config))
$(eval $(call BuildPackage,oonf-api-git-rfc5444))
$(eval $(call BuildPackage,oonf-api-git-core))
$(eval $(call BuildPackage,oonf-api-git-subsystems))
$(eval $(call BuildPackage,oonf-plugin-git-cfgio-file))
$(eval $(call BuildPackage,oonf-plugin-git-cfgparser-compact))
$(eval $(call BuildPackage,oonf-plugin-git-httptelnet))
$(eval $(call BuildPackage,oonf-plugin-git-layer2-viewer))
$(eval $(call BuildPackage,oonf-plugin-git-nl80211-listener))
$(eval $(call BuildPackage,oonf-plugin-git-plugin-controller))
$(eval $(call BuildPackage,oonf-plugin-git-remotecontrol))
