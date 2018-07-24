
noinst_LTLIBRARIES = libgisnetwork.la

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/network.gresource.xml)
network-resources.c: network.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
network-resources.h: network.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += network-resources.c network-resources.h

libgisnetwork_la_SOURCES =			\
	network-dialogs.c network-dialogs.h	\
	gis-network-page.c gis-network-page.h	\
	$(BUILT_SOURCES)

libgisnetwork_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgisnetwork_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisnetwork_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =			\
	meson.build		\
	network.gresource.xml	\
	$(resource_files)	\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
