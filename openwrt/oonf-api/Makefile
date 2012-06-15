include $(TOPDIR)/rules.mk

PKG_NAME:=oonf-api
PKG_VERSION:=2012-06-14
PKG_RELEASE=$(PKG_SOURCE_VERSION)

CMAKE_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_SOURCE_DIR=$(CURDIR)/../../
CMAKE_OPTIONS=-DOONF_LOGGING_LEVEL:String=warn -DCMAKE_BUILD_TYPE:String=Release -DOONF_NO_TESTING:Bool=true -DINSTALL_LIB_DIR:Path=lib/oonf -DINSTALL_INCLUDE_DIR:Path=include/oonf -DINSTALL_CMAKE_DIR:Path=lib/CMake

define Package/oonf-api/template
	SECTION:=net
	CATEGORY:=Network
	MAINTAINER:=Henning Rogge <hrogge@gmail.com>
	SUBMENU:=OLSR.org network framework
	URL:=http://www.olsr.org/
endef

define Package/oonf-common-api
	$(call Package/oonf-api/template) 
	TITLE:=Common-API (generic helpers functions) 
endef

define Package/oonf-config-api
	$(call Package/oonf-api/template) 
	TITLE:=Config-API (generic configuration library)
endef

define Package/oonf-rfc5444-api
	$(call Package/oonf-api/template) 
	TITLE:=RFC5444-API (generic parser/generator for RFC5444)
endef

define Package/oonf-core-api
	$(call Package/oonf-api/template) 
	TITLE:=Core-API (basic infrastructure/scheduler code)
endef

define Package/oonf-plugin-cfgio-file
	$(call Package/oonf-api/template) 
	TITLE:=File input/output configuration plugin
endef

define Package/oonf-plugin-cfgparser-compact
	$(call Package/oonf-api/template) 
	TITLE:=Compact configurationf format plugin
endef

define Package/oonf-plugin-httptelnet
	$(call Package/oonf-api/template) 
	TITLE:=HTTP to Telnet bridge plugin
endef

define Package/oonf-plugin-remotecontrol
	$(call Package/oonf-api/template) 
	TITLE:=Remote debugging and configuration plugin
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include


define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/oonf/common
	$(INSTALL_DIR) $(1)/usr/include/oonf/config
	$(INSTALL_DIR) $(1)/usr/include/oonf/rfc5444
	$(INSTALL_DIR) $(1)/usr/include/oonf/core
        
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/include/oonf/common/* \
		$(1)/usr/include/oonf/common
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/include/oonf/config/* \
		$(1)/usr/include/oonf/config
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/include/oonf/rfc5444/* \
		$(1)/usr/include/oonf/rfc5444
	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/include/oonf/core/* \
		$(1)/usr/include/oonf/core

	$(INSTALL_DIR) $(1)/usr/lib/oonf/
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/lib/oonf/lib*.{so*,a} \
		$(1)/usr/lib/oonf/

#        $(INSTALL_DIR) $(1)/usr/lib/pkgconfig
#        $(INSTALL_DATA) \
#                $(PKG_INSTALL_DIR)/usr/lib/pkgconfig/libusb.pc \
#                $(1)/usr/lib/pkgconfig/
endef

define Package/oonf-common-api/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_common*.so* \
		$(1)/usr/lib/oonf
endef

define Package/oonf-config-api/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_config*.so* \
		$(1)/usr/lib/oonf
endef

define Package/oonf-rfc5444-api/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_rfc5444*.so* \
		$(1)/usr/lib/oonf
endef

define Package/oonf-core-api/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_core*.so* \
		$(1)/usr/lib/oonf
endef

define Package/oonf-plugin-cfgio-file/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_cfgio_file*.so* \
		$(1)/usr/lib/oonf
endef

define Package/oonf-plugin-cfgparser-compact/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_cfgparser_compact*.so* \
		$(1)/usr/lib/oonf
endef

define Package/oonf-plugin-httptelnet/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_httptelnet*.so* \
		$(1)/usr/lib/oonf
endef

define Package/oonf-plugin-remotecontrol/install
	$(INSTALL_DIR) $(1)/usr/lib/oonf
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/lib/oonf/liboonf_remotecontrol*.so* \
		$(1)/usr/lib/oonf
endef

$(eval $(call BuildPackage,oonf-common-api))
$(eval $(call BuildPackage,oonf-config-api))
$(eval $(call BuildPackage,oonf-rfc5444-api))
$(eval $(call BuildPackage,oonf-core-api))
$(eval $(call BuildPackage,oonf-plugin-cfgio-file))
$(eval $(call BuildPackage,oonf-plugin-cfgparser-compact))
$(eval $(call BuildPackage,oonf-plugin-httptelnet))
$(eval $(call BuildPackage,oonf-plugin-remotecontrol))
