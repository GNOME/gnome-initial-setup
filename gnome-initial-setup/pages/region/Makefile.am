
noinst_LTLIBRARIES = libgisregion.la

AM_CPPFLAGS = \
	-I"$(top_srcdir)" \
	-I"$(top_srcdir)/gnome-initial-setup" \
	-I"$(top_builddir)" \
	-DDATADIR=\""$(datadir)"\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/region.gresource.xml)
region-resources.c: region.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
region-resources.h: region.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += region-resources.c region-resources.h

libgisregion_la_SOURCES =				\
	cc-common-language.c cc-common-language.h 	\
	cc-region-chooser.c cc-region-chooser.h	\
	gis-region-page.c gis-region-page.h		\
	$(BUILT_SOURCES)

libgisregion_la_CFLAGS = $(INITIAL_SETUP_CFLAGS)
libgisregion_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisregion_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =			\
	meson.build		\
	region.gresource.xml	\
	$(resource_files)	\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
