
noinst_LTLIBRARIES = libgiswelcome.la

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/welcome.gresource.xml)
welcome-resources.c: welcome.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
welcome-resources.h: welcome.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += welcome-resources.c welcome-resources.h

libgiswelcome_la_SOURCES =			\
	gis-welcome-page.c gis-welcome-page.h	\
	$(BUILT_SOURCES)

libgiswelcome_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgiswelcome_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgiswelcome_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
