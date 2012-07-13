
noinst_LTLIBRARIES = libgislanguage.la

AM_CPPFLAGS = \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

libgislanguage_la_SOURCES =	\
	cc-common-language.c cc-common-language.h \
	gdm-languages.c gdm-languages.h \
	locarchive.h \
	gis-language-page.c gis-language-page.h

libgislanguage_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "../.."
libgislanguage_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgislanguage_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
