
NULL =

noinst_LTLIBRARIES = libgisaccount.la

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

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/account.gresource.xml)
account-resources.c: account.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
account-resources.h: account.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += account-resources.c account-resources.h

libgisaccount_la_SOURCES =						\
	$(BUILT_SOURCES)						\
	gis-account-page.c gis-account-page.h				\
	gis-account-pages.c gis-account-pages.h				\
	gis-account-page-local.c gis-account-page-local.h		\
	gis-account-page-enterprise.c gis-account-page-enterprise.h	\
	um-realm-manager.c um-realm-manager.h				\
	um-utils.c um-utils.h						\
	um-photo-dialog.c um-photo-dialog.h				\
	$(NULL)

libgisaccount_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgisaccount_la_LIBADD = $(INITIAL_SETUP_LIBS) -lcrypt
libgisaccount_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =	\
	org.freedesktop.realmd.xml	\
	account.gresource.xml		\
	meson.build			\
	$(resource_files)		\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
