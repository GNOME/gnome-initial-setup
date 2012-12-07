
noinst_LTLIBRARIES = libgislanguage.la

AM_CPPFLAGS = \
	-I"$(top_srcdir)" \
	-I"$(top_srcdir)/gnome-initial-setup" \
	-I"$(top_builddir)" \
	-DDATADIR=\""$(datadir)"\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

libgislanguage_la_SOURCES =	\
	cc-common-language.c cc-common-language.h \
	gis-language-page.c gis-language-page.h

libgislanguage_la_CFLAGS = $(INITIAL_SETUP_CFLAGS)
libgislanguage_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgislanguage_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
