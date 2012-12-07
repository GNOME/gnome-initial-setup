
noinst_LTLIBRARIES = libgiskeyboard.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DLOCALSTATEDIR="\"$(localstatedir)\"" \
	-DUIDIR="\"$(uidir)\""

libgiskeyboard_la_SOURCES =	\
	gis-keyboard-page.c gis-keyboard-page.h \
	gnome-region-panel-input.c gnome-region-panel-input.h

libgiskeyboard_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgiskeyboard_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgiskeyboard_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
