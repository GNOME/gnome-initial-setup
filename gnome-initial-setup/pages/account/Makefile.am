
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

libgisaccount_la_SOURCES =				\
	$(BUILT_SOURCES)				\
	gis-account-page.c gis-account-page.h		\
	um-realm-manager.c um-realm-manager.h		\
	um-utils.c um-utils.h				\
	pw-utils.c pw-utils.h				\
	$(NULL)

libgisaccount_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgisaccount_la_LIBADD = $(INITIAL_SETUP_LIBS) -lcrypt
libgisaccount_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =	\
	org.freedesktop.realmd.xml
