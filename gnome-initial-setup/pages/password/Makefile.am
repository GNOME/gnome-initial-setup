
NULL =

noinst_LTLIBRARIES = libgispassword.la

BUILT_SOURCES =

um-realm-generated.c: um-realm-generated.h
um-realm-generated.h: $(srcdir)/org.freedesktop.realmd.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.freedesktop.realmd \
		--generate-c-code um-realm-generated \
		--c-namespace UmRealm $< \
		--c-generate-object-manager \
		--annotate "org.freedesktop.realmd.Realm" org.gtk.GDBus.C.Name Common
BUILT_SOURCES += um-realm-generated.c um-realm-generated.h

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/password.gresource.xml)
password-resources.c: password.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
password-resources.h: password.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += password-resources.c password-resources.h

libgispassword_la_SOURCES =						\
	$(BUILT_SOURCES)						\
	gis-password-page.c gis-password-page.h				\
	gis-account-page-local.c gis-account-page-local.h		\
	gis-account-page-enterprise.c gis-account-page-enterprise.h	\
	um-realm-manager.c um-realm-manager.h				\
	um-utils.c um-utils.h						\
	pw-utils.c pw-utils.h						\
	$(NULL)

libgispassword_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgispassword_la_LIBADD = $(INITIAL_SETUP_LIBS) -lcrypt
libgispassword_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =	\
	org.freedesktop.realmd.xml	\
	password.gresource.xml		\
	$(resource_files)
