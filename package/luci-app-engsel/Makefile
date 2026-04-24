include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-engsel
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_MAINTAINER:=0x
PKG_LICENSE:=MIT

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-engsel
  SECTION:=luci
  CATEGORY:=LuCI
  SUBCATEGORY:=3. Applications
  TITLE:=LuCI - Engsel (MyXL Client)
  DEPENDS:=+engsel +luci-base +curl +openssl-util
  PKGARCH:=all
endef

define Package/luci-app-engsel/description
  Web interface untuk engsel MyXL CLI client.
  Tampilan modern dark theme dengan semua fitur CLI:
  login, lihat paket, beli paket, riwayat transaksi, dll.
endef

define Build/Compile
endef

define Package/luci-app-engsel/install
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) ./root/usr/libexec/rpcd/engsel $(1)/usr/libexec/rpcd/engsel

	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./root/usr/share/rpcd/acl.d/luci-app-engsel.json \
		$(1)/usr/share/rpcd/acl.d/luci-app-engsel.json

	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DATA) ./root/usr/share/luci/menu.d/luci-app-engsel.json \
		$(1)/usr/share/luci/menu.d/luci-app-engsel.json

	$(INSTALL_DIR) $(1)/www/luci-static/resources/view
	$(INSTALL_DATA) ./htdocs/luci-static/resources/view/engsel.js \
		$(1)/www/luci-static/resources/view/engsel.js

	$(INSTALL_DIR) $(1)/www/luci-static/resources/engsel
	$(INSTALL_DATA) ./htdocs/luci-static/resources/engsel/engsel.css \
		$(1)/www/luci-static/resources/engsel/engsel.css

	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./root/etc/uci-defaults/99-luci-app-engsel \
		$(1)/etc/uci-defaults/99-luci-app-engsel
endef

define Package/luci-app-engsel/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || /etc/uci-defaults/99-luci-app-engsel
exit 0
endef

$(eval $(call BuildPackage,luci-app-engsel))
