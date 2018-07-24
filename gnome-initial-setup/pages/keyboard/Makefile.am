
noinst_LTLIBRARIES = libgiskeyboard.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DLOCALSTATEDIR="\"$(localstatedir)\"" \
	-DUIDIR="\"$(uidir)\""

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/keyboard.gresource.xml)
keyboard-resources.c: keyboard.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
keyboard-resources.h: keyboard.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += keyboard-resources.c keyboard-resources.h

libgiskeyboard_la_SOURCES =				\
	cc-input-chooser.c cc-input-chooser.h		\
	cc-common-language.c cc-common-language.h	\
	cc-ibus-utils.c cc-ibus-utils.h			\
	gis-keyboard-page.c gis-keyboard-page.h		\
	$(BUILT_SOURCES)

libgiskeyboard_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgiskeyboard_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgiskeyboard_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =			\
	keyboard.gresource.xml	\
	meson.build		\
	$(resource_files)	\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
