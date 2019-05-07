MAINTAINERCLEANFILES = \
	Makefile.in

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(ICD2_CFLAGS) \
	$(OSSO_IC_DEV_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(MCE_CFLAGS)

LDFLAGS = -avoid-version --Wl,as-needed

plugindir = $(ICD_PLUGIN_DIR)

plugin_LTLIBRARIES = \
	libicd_network_ofono.la

libicd_network_ofono_la_SOURCES = \
	notifier.c \
	dbus-helpers.c \
	utils.c \
	modem.c \
	mce.c \
	ofono-net.c \
	ofono-sim.c \
	ofono-modem.c \
	ofono-manager.c \
	search.c \
	libicd-network-ofono.c

libicd_network_ofono_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GCONF_LIBS) \
	$(ICD2_LIBS) \
	$(OSSO_IC_DEV_LIBS) \
	$(DBUS_LIBS) \
	$(MCE_LIBS)
