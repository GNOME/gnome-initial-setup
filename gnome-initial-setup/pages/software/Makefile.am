noinst_LTLIBRARIES = libgissoftware.la

pkgdatadir = $(datadir)/gnome-initial-setup

AM_CPPFLAGS = \
	-DPKGDATADIR="\"$(pkgdatadir)\""

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/software.gresource.xml)
software-resources.c: software.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
software-resources.h: software.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += software-resources.c software-resources.h

libgissoftware_la_SOURCES =			\
	gis-software-page.c gis-software-page.h	\
	$(BUILT_SOURCES)

libgissoftware_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) $(SOFTWARE_SOURCES_CFLAGS) -I "$(srcdir)/../.."
libgissoftware_la_LIBADD = $(INITIAL_SETUP_LIBS) $(SOFTWARE_SOURCES_LIBS)
libgissoftware_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =			\
	meson.build		\
	software.gresource.xml	\
	$(resource_files)	\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
