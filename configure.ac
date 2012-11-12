AC_INIT([gnome-initial-setup],[0.3.1])
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

NETWORK_MANAGER_REQUIRED_VERSION=0.8.992
GLIB_REQUIRED_VERSION=2.29.4
GTK_REQUIRED_VERSION=3.1.2
CLUTTER_REQUIRED_VERSION=1.11.3

PKG_CHECK_MODULES(INITIAL_SETUP,
                  NetworkManager >= $NETWORK_MANAGER_REQUIRED_VERSION
                  libnm-glib >= $NETWORK_MANAGER_REQUIRED_VERSION
                  libnm-util >= $NETWORK_MANAGER_REQUIRED_VERSION
                  accountsservice
                  gnome-desktop-3.0
                  gstreamer-1.0
                  fontconfig
                  geoclue
                  gweather-3.0
                  goa-1.0
                  goa-backend-1.0
                  gtk+-3.0 >= $GTK_REQUIRED_VERSION
                  gio-unix-2.0 >= $GLIB_REQUIRED_VERSION
                  gdm)

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

PKG_CHECK_MODULES(CLUTTER,
                  clutter-gtk-1.0
                  clutter-1.0 >= $CLUTTER_REQUIRED_VERSION,
                  have_clutter=yes, have_clutter=no)
AM_CONDITIONAL(HAVE_CLUTTER, [test x$have_clutter = xyes])
if test x$have_clutter = xyes; then
  INITIAL_SETUP_CFLAGS="$INITIAL_SETUP_CFLAGS $CLUTTER_CFLAGS"
  INITIAL_SETUP_LIBS="$INITIAL_SETUP_LIBS $CLUTTER_LIBS"
  AC_DEFINE(HAVE_CLUTTER, 1, [Build with Clutter support?])
fi

PKG_CHECK_MODULES(PWQUALITY,
                  pwquality,
                  have_pwquality=yes, have_pwquality=no)
AM_CONDITIONAL(HAVE_PWQUALITY, [test x$have_pwquality = xyes])
if test x$have_pwquality = xyes; then
  INITIAL_SETUP_CFLAGS="$INITIAL_SETUP_CFLAGS $PWQUALITY_CFLAGS"
  INITIAL_SETUP_LIBS="$INITIAL_SETUP_LIBS $PWQUALITY_LIBS"
  AC_DEFINE(HAVE_PWQUALITY, 1, [Build with libpwquality support?])
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

# systemd
AC_ARG_WITH([systemdsystemunitdir],
        AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
        [],
        [with_systemdsystemunitdir=yes])
if test "x$with_systemdsystemunitdir" = "xyes"; then
    with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)
fi
if test "x$with_systemdsystemunitdir" != "xno"; then
    AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi

GLIB_VERSION_DEFINES="-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_30 '-DGLIB_VERSION_MAX_ALLOWED=G_ENCODE_VERSION(2,34)'"

INITIAL_SETUP_CFLAGS="$INITIAL_SETUP_CFLAGS $GLIB_VERSION_DEFINES"

AC_SUBST(INITIAL_SETUP_CFLAGS)
AC_SUBST(INITIAL_SETUP_LIBS)

PKG_CHECK_MODULES(PLAYER, [clutter-gst-1.0 clutter-x11-1.0])

if test "$GCC" = "yes"; then
        CFLAGS="\
        -Wall \
        -Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes \
        -Wnested-externs -Wpointer-arith \
        -Wcast-align -Wsign-compare \
        -Werror $CFLAGS"
fi

AC_SUBST(CFLAGS)
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

AM_CONFIG_HEADER(config.h)
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
po/Makefile.in
player/Makefile
])
AC_OUTPUT
