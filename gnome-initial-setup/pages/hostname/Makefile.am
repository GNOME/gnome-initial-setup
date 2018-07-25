
NULL =

noinst_LTLIBRARIES = libgishostname.la

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/hostname.gresource.xml)
hostname-resources.c: hostname.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
hostname-resources.h: hostname.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += hostname-resources.c hostname-resources.h

libgishostname_la_SOURCES =				\
	$(BUILT_SOURCES)				\
	gis-hostname-page.c gis-hostname-page.h		\
	hostname-utils.c hostname-utils.h		\
	$(NULL)

libgishostname_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgishostname_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgishostname_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =	\
	hostname.gresource.xml		\
	$(resource_files)
