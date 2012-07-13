
noinst_LTLIBRARIES = libgiseulas.la

AM_CPPFLAGS = \
	-DUIDIR="\"$(uidir)\""

libgiseulas_la_SOURCES =	\
	gis-eula-pages.c gis-eula-pages.h

libgiseulas_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "../.."
libgiseulas_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgiseulas_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
