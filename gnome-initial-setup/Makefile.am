uidir = $(datadir)/gnome-initial-setup

# GNOMECC_DATA_DIR is used in tz.c
# DATADIR is used in cc-timezone-map.c
# these two files are copied from gnome-control-center,
# which explains the slightly odd locations
AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DUIDIR="\"$(uidir)\"" \
	-DGNOMECC_DATA_DIR="\"$(datadir)/gnome-control-center\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATADIR="\"$(datadir)/gnome-control-center/ui/datetime\""

LIBS = $(INITIAL_SETUP_LIBS) -lm

libexec_PROGRAMS = gnome-initial-setup

BUILT_SOURCES =

timedated.c: timedated.h
timedated.h: $(srcdir)/timedated1-interface.xml
	gdbus-codegen \
		--interface-prefix org.freedesktop. \
		--generate-c-code timedated $<
BUILT_SOURCES += timedated.c timedated.h

UI_FILES = \
	setup.ui \
	gis-welcome-page.ui \
	gis-network-page.ui \
	gis-account-page.ui \
	gis-location-page.ui \
	gis-goa-page.ui \
	gis-summary-page.ui

setup_resources.c: setup.gresource.xml $(UI_FILES) welcome-image.png
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/setup.gresource.xml
BUILT_SOURCES += setup_resources.c

if HAVE_PWQUALITY
pw_utils_sources = pw-utils-pwquality.c pw-utils.h
else
pw_utils_sources = pw-utils.c pw-utils.h
endif

gnome_initial_setup_SOURCES =	\
	gnome-initial-setup.c gnome-initial-setup.h \
	cc-notebook.c cc-notebook.h \
	gis-assistant.c gis-assistant.h \
	gis-utils.c gis-utils.h \
	gis-welcome-page.c gis-welcome-page.h \
	gis-eula-pages.c gis-eula-pages.h \
	gis-location-page.c gis-location-page.h \
	gis-account-page.c gis-account-page.h \
	gis-network-page.c gis-network-page.h \
	gis-summary-page.c gis-summary-page.h \
	gis-goa-page.c gis-goa-page.h \
	cc-online-accounts-add-account-dialog.c cc-online-accounts-add-account-dialog.h \
	panel-cell-renderer-signal.c panel-cell-renderer-signal.h \
	panel-cell-renderer-mode.c panel-cell-renderer-mode.h \
	panel-cell-renderer-security.c panel-cell-renderer-security.h \
	cc-timezone-map.c cc-timezone-map.h \
	$(pw_utils_sources) \
	um-utils.c um-utils.h \
	um-crop-area.c um-crop-area.h \
	um-photo-dialog.c um-photo-dialog.h \
	tz.c tz.h \
	$(BUILT_SOURCES)

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = welcome-tour.desktop

included_source_files = \
	gis-account-page.c

EXTRA_DIST = \
	$(sys_DATA) \
	$(included_source_files) \
	timedated1-interface.xml \
	$(UI_FILES) \
	setup.gresource.xml \
	welcome-image.png \
	welcome-tour.desktop
