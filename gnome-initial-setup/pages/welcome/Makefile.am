
noinst_LTLIBRARIES = libgiswelcome.la

libgiswelcome_la_SOURCES =	\
	gis-welcome-page.c gis-welcome-page.h

libgiswelcome_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgiswelcome_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgiswelcome_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
