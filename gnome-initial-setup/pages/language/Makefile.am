
noinst_LTLIBRARIES = libgislanguage.la

AM_CPPFLAGS = \
	-I"$(top_srcdir)" \
	-I"$(top_srcdir)/gnome-initial-setup" \
	-I"$(top_builddir)" \
	-DDATADIR=\""$(datadir)"\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/language.gresource.xml)
language-resources.c: language.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
language-resources.h: language.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += language-resources.c language-resources.h

libgislanguage_la_SOURCES =				\
	cc-common-language.c cc-common-language.h 	\
	cc-language-chooser.c cc-language-chooser.h	\
	cc-util.c cc-util.h				\
	gis-welcome-widget.c gis-welcome-widget.h	\
	gis-language-page.c gis-language-page.h		\
	$(BUILT_SOURCES)

libgislanguage_la_CFLAGS = $(INITIAL_SETUP_CFLAGS)
libgislanguage_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgislanguage_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =			\
	language.gresource.xml	\
	meson.build		\
	$(resource_files)	\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
