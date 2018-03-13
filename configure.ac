AC_INIT([gnome-initial-setup],[3.28.0])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([dist-xz no-dist-gzip foreign tar-ustar])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC

LT_INIT

IT_PROG_INTLTOOL([0.40])

GETTEXT_PACKAGE=gnome-initial-setup
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [gettext package])

AM_GLIB_GNU_GETTEXT

AC_ARG_WITH(vendor-conf-file,
            AS_HELP_STRING([--with-vendor-conf-file=<file>],
                           [vendor conf file]))

if ! test -z "$with_vendor_conf_file"; then
   VENDOR_CONF_FILE=$with_vendor_conf_file
else
   VENDOR_CONF_FILE=${localstatedir}/lib/gnome-initial-setup/vendor.conf
fi
AC_SUBST(VENDOR_CONF_FILE)

LIBNM_REQUIRED_VERSION=1.2
LIBNMA_REQUIRED_VERSION=1.0
GLIB_REQUIRED_VERSION=2.53.0
GTK_REQUIRED_VERSION=3.11.3
PANGO_REQUIRED_VERSION=1.32.5
IBUS_REQUIRED_VERSION=1.4.99
GNOME_DESKTOP_REQUIRED_VERSION=3.7.5
POLKIT_REQUIRED_VERSION=0.103
GDM_REQUIRED_VERSION=3.8.3
GEOCLUE_REQUIRED_VERSION=2.3.1
PACKAGEKIT_REQUIRED_VERSION=1.1.4

PKG_CHECK_MODULES(INITIAL_SETUP,
                  libnm >= $LIBNM_REQUIRED_VERSION
                  libnma >= $LIBNMA_REQUIRED_VERSION
                  polkit-gobject-1 >= $POLKIT_REQUIRED_VERSION
                  accountsservice
                  gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION
                  fontconfig
                  gweather-3.0
                  goa-1.0
                  goa-backend-1.0
                  gtk+-3.0 >= $GTK_REQUIRED_VERSION
                  gio-unix-2.0 >= $GLIB_REQUIRED_VERSION
                  gdm >= $GDM_REQUIRED_VERSION
                  geocode-glib-1.0
                  libgeoclue-2.0 >= $GEOCLUE_REQUIRED_VERSION
                  pango >= $PANGO_REQUIRED_VERSION
                  rest-0.7
                  json-glib-1.0
                  libsecret-1
                  pwquality
                  webkit2gtk-4.0)

INITIAL_SETUP_CFLAGS="$INITIAL_SETUP_CFLAGS -DNM_VERSION_MIN_REQUIRED=NM_VERSION_1_2"
INITIAL_SETUP_CFLAGS="$INITIAL_SETUP_CFLAGS -DNM_VERSION_MAX_ALLOWED=NM_VERSION_1_2"

AC_ARG_ENABLE(software-sources,
              [AS_HELP_STRING([--enable-software-sources],
                              [enable software sources page])],,
              [enable_software_sources=no])

if test "$enable_software_sources" = "yes"; then
  PKG_CHECK_MODULES(SOFTWARE_SOURCES, packagekit-glib2 >= $PACKAGEKIT_REQUIRED_VERSION)
  AC_DEFINE(ENABLE_SOFTWARE_SOURCES, [1], [Enable software sources page])
fi

PKG_CHECK_MODULES(CHEESE,
                  cheese
                  cheese-gtk >= 3.3.5,
                  have_cheese=yes, have_cheese=no)
AM_CONDITIONAL(HAVE_CHEESE, [test x$have_cheese = xyes])
if test x$have_cheese = xyes; then
  INITIAL_SETUP_CFLAGS="$INITIAL_SETUP_CFLAGS $CHEESE_CFLAGS"
  INITIAL_SETUP_LIBS="$INITIAL_SETUP_LIBS $CHEESE_LIBS"
  AC_DEFINE(HAVE_CHEESE, 1, [Build with Cheese support?])
fi

PKG_CHECK_MODULES(COPY_WORKER, gio-2.0)

AC_ARG_ENABLE(ibus,
        AS_HELP_STRING([--disable-ibus],
                       [Disable IBus support]),
        enable_ibus=$enableval,
        enable_ibus=yes)

AS_IF([test x$enable_ibus = xyes],
      [PKG_CHECK_MODULES(IBUS,
                        ibus-1.0 >= $IBUS_REQUIRED_VERSION,
                        have_ibus=yes, have_ibus=no)],
      [have_ibus=no])

if test x$have_ibus = xyes; then
  INITIAL_SETUP_CFLAGS="$INITIAL_SETUP_CFLAGS $IBUS_CFLAGS"
  INITIAL_SETUP_LIBS="$INITIAL_SETUP_LIBS $IBUS_LIBS"
  AC_DEFINE(HAVE_IBUS, 1, [Build with IBus support?])
fi

# Kerberos kerberos support
AC_PATH_PROG(KRB5_CONFIG, krb5-config, no)
if test "$KRB5_CONFIG" = "no"; then
  AC_MSG_ERROR([krb5-config executable not found in your path - should be installed with the kerberos libraries])
fi

AC_MSG_CHECKING(for krb5 libraries and flags)
KRB5_CFLAGS="`$KRB5_CONFIG --cflags`"
KRB5_LIBS="`$KRB5_CONFIG --libs`"
AC_MSG_RESULT($KRB5_CFLAGS $KRB5_LIBS)

AC_SUBST(KRB5_CFLAGS)
AC_SUBST(KRB5_LIBS)

INITIAL_SETUP_CFLAGS="$INITIAL_SETUP_CFLAGS $KRB5_CFLAGS"
INITIAL_SETUP_LIBS="$INITIAL_SETUP_LIBS $KRB5_LIBS"

# This is a hard-dependency for the region and user-accounts panels
PKG_CHECK_MODULES(ISOCODES, iso-codes)

AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
ISO_CODES=iso-codes

AC_SUBST(INITIAL_SETUP_CFLAGS)
AC_SUBST(INITIAL_SETUP_LIBS)

GNOME_COMPILE_WARNINGS([error])
GNOME_MAINTAINER_MODE_DEFINES

AC_SUBST(CFLAGS)
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
Makefile
data/Makefile
gnome-initial-setup/Makefile
gnome-initial-setup/pages/Makefile
gnome-initial-setup/pages/language/Makefile
gnome-initial-setup/pages/region/Makefile
gnome-initial-setup/pages/keyboard/Makefile
gnome-initial-setup/pages/eulas/Makefile
gnome-initial-setup/pages/network/Makefile
gnome-initial-setup/pages/timezone/Makefile
gnome-initial-setup/pages/privacy/Makefile
gnome-initial-setup/pages/goa/Makefile
gnome-initial-setup/pages/account/Makefile
gnome-initial-setup/pages/password/Makefile
gnome-initial-setup/pages/software/Makefile
gnome-initial-setup/pages/summary/Makefile
po/Makefile.in
])
AC_OUTPUT
