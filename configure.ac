AC_INIT([gnome-initial-setup],[3.10.0])
AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE([dist-xz no-dist-gzip foreign])
AM_SILENT_RULES([yes])
LT_INIT
AC_PROG_CC

IT_PROG_INTLTOOL([0.40])

GETTEXT_PACKAGE=gnome-initial-setup
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [gettext package])

AM_GLIB_GNU_GETTEXT

NETWORK_MANAGER_REQUIRED_VERSION=0.9.6.4
GLIB_REQUIRED_VERSION=2.36.0
GTK_REQUIRED_VERSION=3.7.11
PANGO_REQUIRED_VERSION=1.32.5
IBUS_REQUIRED_VERSION=1.4.99
GNOME_DESKTOP_REQUIRED_VERSION=3.7.5
POLKIT_REQUIRED_VERSION=0.103
GDM_REQUIRED_VERSION=3.8.3

# EggListBox submodule
prev_top_build_prefix=$ac_top_build_prefix
prev_ac_configure_args=$ac_configure_args
AX_CONFIG_DIR([egg-list-box])
ac_top_build_prefix=$prev_top_build_prefix
ac_configure_args=$prev_ac_configure_args

export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:"$ac_top_build_prefix"egg-list-box

PKG_CHECK_MODULES(INITIAL_SETUP,
                  NetworkManager >= $NETWORK_MANAGER_REQUIRED_VERSION
                  libnm-glib >= $NETWORK_MANAGER_REQUIRED_VERSION
                  libnm-util >= $NETWORK_MANAGER_REQUIRED_VERSION
                  libnm-gtk >= $NETWORK_MANAGER_REQUIRED_VERSION
                  polkit-gobject-1 >= $POLKIT_REQUIRED_VERSION
                  accountsservice
                  gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION
                  gstreamer-1.0
                  fontconfig
                  gweather-3.0
                  goa-1.0
                  goa-backend-1.0
                  gtk+-3.0 >= $GTK_REQUIRED_VERSION
                  gio-unix-2.0 >= $GLIB_REQUIRED_VERSION
                  gdm >= $GDM_REQUIRED_VERSION
                  pango >= $PANGO_REQUIRED_VERSION
                  pwquality
                  egg-list-box-uninstalled)

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
gnome-initial-setup/pages/eulas/Makefile
gnome-initial-setup/pages/account/Makefile
gnome-initial-setup/pages/location/Makefile
gnome-initial-setup/pages/network/Makefile
gnome-initial-setup/pages/goa/Makefile
gnome-initial-setup/pages/summary/Makefile
gnome-initial-setup/pages/keyboard/Makefile
po/Makefile.in
])
AC_OUTPUT
