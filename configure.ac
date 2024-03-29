AC_INIT([ofono gprs network configuration module], patsubst(esyscmd([dpkg-parsechangelog | sed -n '/^Version: \(.*\)$/ {s//\1/;p}']), [
]),[],[libicd-network-ofono])
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([foreign])
LT_INIT([shared disable-static])

AC_PREFIX_DEFAULT([/usr])

AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

AM_GCONF_SOURCE_2
AC_PATH_PROG(GCONFTOOL, gconftool-2)

PKG_CHECK_MODULES(GLIB, glib-2.0 gobject-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GCONF, gconf-2.0)
AC_SUBST(GCONF_CFLAGS)
AC_SUBST(GCONF_LIBS)

PKG_CHECK_MODULES(ICD2, icd2)
AC_SUBST(ICD2_CFLAGS)
AC_SUBST(ICD2_LIBS)

PKG_CHECK_MODULES(LIBGOFONO, libgofono libglibutil)
AC_SUBST(LIBGOFONO_CFLAGS)
AC_SUBST(LIBGOFONO_LIBS)

AC_MSG_CHECKING([where ICd2 plugin dir is])
ICD_PLUGIN_DIR=`$PKG_CONFIG --variable=icdplugindir icd2`
AC_SUBST(ICD_PLUGIN_DIR)
AC_MSG_RESULT($ICD_PLUGIN_DIR)

PKG_CHECK_MODULES(OSSO_IC_DEV, osso-ic >= 0.1)
AC_SUBST(OSSO_IC_DEV_CFLAGS)

PKG_CHECK_MODULES(DBUS, dbus-1)
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

PKG_CHECK_MODULES(OFONO, ofono)
AC_SUBST(OFONO_CFLAGS)
AC_SUBST(OFONO_LIBS)

PKG_CHECK_MODULES(CONNUI, connui)
AC_SUBST(CONNUI_CFLAGS)
AC_SUBST(CONNUI_LIBS)

PKG_CHECK_MODULES(MBPI, [mobile-broadband-provider-info])
PKG_CHECK_MODULES(XML, [libxml-2.0])

AC_MSG_CHECKING([wheter to send logs to stderr])
AC_ARG_ENABLE(log_stderr,
	[AS_HELP_STRING([--enable-log-stderr],
			[send logs to stderr (default=no)]
			)],
	[],
	enable_log_stderr=no)
if (test x$enable_log_stderr = xyes); then
	AC_DEFINE(ICD_LOG_STDERR, 1, [Log messages to stderr.])
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([wheter to build doxygen documentation])
AC_ARG_ENABLE(doxygen,
	[AS_HELP_STRING([--enable-doxygen],
			[enable doxygen document creation (default=no)]
			)])
if (test x$enable_doxygen = xyes); then
   AC_MSG_RESULT(yes)
   AC_CHECK_PROG(DOXYGEN, doxygen, doxygen, [])
   if (test x$DOXYGEN = x); then
      AC_MSG_WARN([doxygen requested but doxygen not found])
   else
      echo -n
      AC_SUBST(DOXYGEN)
   fi
else
   AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(DOXYGEN_DOCS_ENABLED, [test x$DOXYGEN != x])

CFLAGS="$CFLAGS -Wall -Werror -Wmissing-declarations"
CFLAGS="$CFLAGS -include config.h"

MBPI_DATABASE=`pkg-config mobile-broadband-provider-info --variable=database`
AC_SUBST(MBPI_DATABASE)

AC_OUTPUT([
	Makefile
	src/Makefile
	etc/Makefile
	])
