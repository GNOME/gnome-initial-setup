SUBDIRS = pages

uidir = $(datadir)/gnome-initial-setup

# GNOMECC_DATA_DIR is used in tz.c
# DATADIR is used in cc-timezone-map.c
# these two files are copied from gnome-control-center,
# which explains the slightly odd locations
AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DUIDIR="\"$(uidir)\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

LIBS = $(INITIAL_SETUP_LIBS) -lm

libexec_PROGRAMS = gnome-initial-setup

BUILT_SOURCES =

UI_FILES = \
	pages/language/gis-language-page.ui \
	pages/network/gis-network-page.ui \
	pages/account/gis-account-page.ui \
	pages/location/gis-location-page.ui \
	pages/goa/gis-goa-page.ui \
	pages/summary/gis-summary-page.ui

setup_resources.c: setup.gresource.xml $(UI_FILES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/setup.gresource.xml
BUILT_SOURCES += setup_resources.c

gnome_initial_setup_SOURCES =	\
	gnome-initial-setup.c gnome-initial-setup.h \
	cc-notebook.c cc-notebook.h \
	gis-assistant.c gis-assistant.h gis-assistant-private.h \
	gis-assistant-clutter.c gis-assistant-clutter.h \
	gis-utils.c gis-utils.h \
	$(BUILT_SOURCES)

gnome_initial_setup_LDADD =	\
	pages/language/libgislanguage.la \
	pages/eulas/libgiseulas.la \
	pages/network/libgisnetwork.la \
	pages/location/libgislocation.la \
	pages/account/libgisaccount.la \
	pages/goa/libgisgoa.la \
	pages/summary/libgissummary.la

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = welcome-tour.desktop

EXTRA_DIST = \
	$(UI_FILES) \
	setup.gresource.xml \
	welcome-tour.desktop
