MAINTAINERCLEANFILES = \
	Makefile.in

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(ICD2_CFLAGS) \
	$(OSSO_IC_DEV_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBGOFONO_CFLAGS) \
	$(CONNUI_CFLAGS)

AM_LDFLAGS = -avoid-version -Wl,--as-needed

plugindir = $(ICD_PLUGIN_DIR)

plugin_LTLIBRARIES = \
	libicd_network_ofono.la

libicd_network_ofono_la_SOURCES = \
	iap.c \
	ofono-gconf.c \
	ofono-modem.c \
	search.c \
	link.c \
	utils.c \
	libicd-network-ofono.c

libicd_network_ofono_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GCONF_LIBS) \
	$(ICD2_LIBS) \
	$(OSSO_IC_DEV_LIBS) \
	$(DBUS_LIBS) \
	$(LIBGOFONO_LIBS) \
	$(CONNUI_LIBS)
