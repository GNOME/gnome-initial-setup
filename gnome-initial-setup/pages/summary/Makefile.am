
noinst_LTLIBRARIES = libgissummary.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DUIDIR="\"$(uidir)\""

libgissummary_la_SOURCES =	\
	gis-summary-page.c gis-summary-page.h

libgissummary_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgissummary_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgissummary_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
