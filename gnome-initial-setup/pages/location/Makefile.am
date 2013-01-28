
noinst_LTLIBRARIES = libgislocation.la

BUILT_SOURCES =

# GNOMECC_DATA_DIR is used in tz.c
# these two files are copied from gnome-control-center,
# which explains the slightly odd locations

# XXX -- move to libtimezonemap at some point
AM_CPPFLAGS = \
	-DGNOMECC_DATA_DIR="\"$(datadir)/gnome-control-center\""

timedated.c: timedated.h
timedated.h: $(srcdir)/timedated1-interface.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.freedesktop. \
		--generate-c-code timedated $<
BUILT_SOURCES += timedated.c timedated.h

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/datetime.gresource.xml)
cc-datetime-resources.c: datetime.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
cc-datetime-resources.h: datetime.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += cc-datetime-resources.c cc-datetime-resources.h

resource_files_location = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/location.gresource.xml)
location-resources.c: location.gresource.xml $(resource_files_location)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
location-resources.h: location.gresource.xml $(resource_files_location)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += location-resources.c location-resources.h

libgislocation_la_SOURCES =	\
	tz.c tz.h \
	cc-timezone-map.c cc-timezone-map.h \
	gis-location-page.c gis-location-page.h \
	$(BUILT_SOURCES)

libgislocation_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgislocation_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgislocation_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =				\
	timedated1-interface.xml	\
	$(resource_files)		\
	$(resource_files_location)	\
	datetime.gresource.xml
