
SUBDIRS = languages

noinst_LTLIBRARIES = libgislanguage.la

libgislanguage_la_SOURCES =	\
	gis-language-page.c gis-language-page.h

libgislanguage_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "../.."
libgislanguage_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgislanguage_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
