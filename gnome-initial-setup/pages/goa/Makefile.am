
noinst_LTLIBRARIES = libgisgoa.la

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/goa.gresource.xml)
goa-resources.c: goa.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
goa-resources.h: goa.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += goa-resources.c goa-resources.h

libgisgoa_la_SOURCES =	\
	gis-goa-page.c gis-goa-page.h	\
	$(BUILT_SOURCES)

libgisgoa_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.." -I "$(top_srcdir)" -I "$(top_builddir)"
libgisgoa_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisgoa_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =			\
	goa.gresource.xml	\
	meson.build		\
	$(resource_files)	\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
