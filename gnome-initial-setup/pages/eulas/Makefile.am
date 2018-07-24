
noinst_LTLIBRARIES = libgiseulas.la

pkgdatadir = $(datadir)/gnome-initial-setup

AM_CPPFLAGS = \
	-DPKGDATADIR="\"$(pkgdatadir)\""

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/eulas.gresource.xml)
eulas-resources.c: eulas.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
eulas-resources.h: eulas.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += eulas-resources.c eulas-resources.h

libgiseulas_la_SOURCES =			\
	gis-eula-page.c gis-eula-page.h		\
	gis-eula-pages.c gis-eula-pages.h 	\
	utils.c utils.h 			\
	$(BUILT_SOURCES)

libgiseulas_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgiseulas_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgiseulas_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =			\
	eulas.gresource.xml	\
	meson.build		\
	$(resource_files)	\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
