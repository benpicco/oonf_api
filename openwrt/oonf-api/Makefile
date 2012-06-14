include $(TOPDIR)/rules.mk

PKG_NAME:=oonf-api
PKG_VERSION:=2012-06-14
PKG_RELEASE=$(PKG_SOURCE_VERSION)

CMAKE_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_SOURCE_DIR=$(CURDIR)/../../
CMAKE_OPTIONS=-DOONF_LOGGING_LEVEL:String=warn -DCMAKE_BUILD_TYPE:String=Release -DOONF_NO_TESTING:Bool=true -DINSTALL_LIB_DIR:Path=lib/oonf -DINSTALL_INCLUDE_DIR:Path=include/oonf -DINSTALL_CMAKE_DIR:Path=lib/CMake

define Package/oonf-common-api
	SECTION:=net
	CATEGORY:=Network
	TITLE:=OLSR.org network framework (common API)
endef

define Package/oonf-config-api
	SECTION:=net
	CATEGORY:=Network
	DEPENDS:=+oonf-common-api
	TITLE:=OLSR.org network framework (config API)
endef

define Package/oonf-rfc5444-api
	SECTION:=net
	CATEGORY:=Network
	DEPENDS:=+oonf-common-api
	TITLE:=OLSR.org network framework (rfc5444 API)
endef

define Package/oonf-core-api
	SECTION:=net
	CATEGORY:=Network
	DEPENDS:=+oonf-common-api +oonf-config-api +oonf-rfc5444-api
	TITLE:=OLSR.org network framework (core API)
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

$(eval $(call BuildPackage,oonf-common-api))
$(eval $(call BuildPackage,oonf-config-api))
$(eval $(call BuildPackage,oonf-rfc5444-api))
$(eval $(call BuildPackage,oonf-core-api))
