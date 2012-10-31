
noinst_LTLIBRARIES = libgislocation.la

BUILT_SOURCES =

# GNOMECC_DATA_DIR is used in tz.c
# DATADIR is used in cc-timezone-map.c
# these two files are copied from gnome-control-center,
# which explains the slightly odd locations

# XXX -- move to libtimezonemap at some point
AM_CPPFLAGS = \
	-DGNOMECC_DATA_DIR="\"$(datadir)/gnome-control-center\"" \
	-DDATADIR="\"$(datadir)/gnome-control-center/ui/datetime\""

timedated.c: timedated.h
timedated.h: $(srcdir)/timedated1-interface.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.freedesktop. \
		--generate-c-code timedated $<
BUILT_SOURCES += timedated.c timedated.h

libgislocation_la_SOURCES =	\
	tz.c tz.h \
	cc-timezone-map.c cc-timezone-map.h \
	gis-location-page.c gis-location-page.h \
	$(BUILT_SOURCES)

libgislocation_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgislocation_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgislocation_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST = 	\
	timedated1-interface.xml
