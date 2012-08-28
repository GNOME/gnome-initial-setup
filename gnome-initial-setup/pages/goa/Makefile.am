
noinst_LTLIBRARIES = libgisgoa.la

libgisgoa_la_SOURCES =	\
	cc-online-accounts-add-account-dialog.c cc-online-accounts-add-account-dialog.h \
	gis-goa-page.c gis-goa-page.h

libgisgoa_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgisgoa_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisgoa_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
