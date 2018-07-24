ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CFLAGS = ${WARN_CFLAGS}

SUBDIRS = data gnome-initial-setup po

EXTRA_DIST = \
    autogen.sh \
    COPYING \
    AUTHORS \
    NEWS \
    gnome-initial-setup.doap \
    meson.build \
    meson_options.txt \
    po/meson.build \
    $(NULL)

MAINTAINERCLEANFILES =		\
    po/*.header		\
    po/Makevars.template	\
    po/Rules-quot		\
    po/*.sed		\
    po/*.sin
