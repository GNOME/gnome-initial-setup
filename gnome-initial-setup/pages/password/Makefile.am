
NULL =

noinst_LTLIBRARIES = libgispassword.la

BUILT_SOURCES =

BUILT_SOURCES += $(srcdir)/../account/um-realm-generated.c $(srcdir)/../account/um-realm-generated.h

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/password.gresource.xml)
password-resources.c: password.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
password-resources.h: password.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += password-resources.c password-resources.h

libgispassword_la_SOURCES =						\
	$(BUILT_SOURCES)						\
	gis-password-page.c gis-password-page.h				\
	$(srcdir)/../account/um-realm-manager.c $(srcdir)/../account/um-realm-manager.h \
	$(srcdir)/../account/um-utils.c $(srcdir)/../account/um-utils.h	\
	pw-utils.c pw-utils.h						\
	$(NULL)

libgispassword_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.." -I "$(srcdir)/../account"
libgispassword_la_LIBADD = $(INITIAL_SETUP_LIBS) -lcrypt
libgispassword_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =	\
	$(srcdir)/../account/org.freedesktop.realmd.xml \
	password.gresource.xml		\
	$(resource_files)
