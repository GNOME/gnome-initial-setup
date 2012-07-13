AC_INIT([gnome-initial-setup],[0.2])
AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE([dist-xz foreign])
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
                  gnome-keyring-1
                  gnome-desktop-3.0
                  gstreamer-0.10
                  cheese
                  cheese-gtk >= 3.3.5
                  clutter-gtk-1.0
                  clutter-1.0 >= $CLUTTER_REQUIRED_VERSION
                  fontconfig
                  geoclue
                  gweather-3.0
                  goa-1.0
                  goa-backend-1.0
                  gtk+-3.0 >= $GTK_REQUIRED_VERSION
                  glib-2.0 >= $GLIB_REQUIRED_VERSION
                  gio-2.0 >= $GLIB_REQUIRED_VERSION
                  gio-unix-2.0 >= $GLIB_REQUIRED_VERSION
                  gdmgreeter)

AC_CHECK_HEADER([pwquality.h], [have_pwquality=yes], [have_pwquality=no])
AM_CONDITIONAL(HAVE_PWQUALITY, [test x$have_pwquality = xyes])
if test x$have_pwquality = xyes ; then
  INITIAL_SETUP_LIBS="$INITIAL_SETUP_LIBS -lpwquality"
fi

# This is a hard-dependency for the region and user-accounts panels
PKG_CHECK_MODULES(ISOCODES, iso-codes)

AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
ISO_CODES=iso-codes

AC_SUBST(INITIAL_SETUP_CFLAGS)
AC_SUBST(INITIAL_SETUP_LIBS)

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
gnome-initial-setup/pages/language/languages/Makefile
gnome-initial-setup/pages/eulas/Makefile
gnome-initial-setup/pages/account/Makefile
gnome-initial-setup/pages/location/Makefile
gnome-initial-setup/pages/network/Makefile
gnome-initial-setup/pages/goa/Makefile
gnome-initial-setup/pages/summary/Makefile
po/Makefile.in
])
AC_OUTPUT
