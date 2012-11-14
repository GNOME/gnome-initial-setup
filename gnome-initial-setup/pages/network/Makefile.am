
noinst_LTLIBRARIES = libgisnetwork.la

libgisnetwork_la_SOURCES =	\
	panel-cell-renderer-signal.c panel-cell-renderer-signal.h \
	panel-cell-renderer-mode.c panel-cell-renderer-mode.h \
	panel-cell-renderer-security.c panel-cell-renderer-security.h \
	network-dialogs.c network-dialogs.h \
	gis-network-page.c gis-network-page.h

libgisnetwork_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgisnetwork_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisnetwork_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
