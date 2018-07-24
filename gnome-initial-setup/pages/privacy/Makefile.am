
noinst_LTLIBRARIES = libgisprivacy.la

AM_CPPFLAGS = \
	-I"$(top_srcdir)" \
	-I"$(top_srcdir)/gnome-initial-setup" \
	-I"$(top_builddir)" \
	-DDATADIR=\""$(datadir)"\"

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/privacy.gresource.xml)
privacy-resources.c: privacy.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
privacy-resources.h: privacy.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += privacy-resources.c privacy-resources.h

libgisprivacy_la_SOURCES =				\
	gis-privacy-page.c gis-privacy-page.h		\
	$(BUILT_SOURCES)

libgisprivacy_la_CFLAGS = $(INITIAL_SETUP_CFLAGS)
libgisprivacy_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisprivacy_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =			\
	meson.build		\
	privacy.gresource.xml	\
	$(resource_files)	\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
