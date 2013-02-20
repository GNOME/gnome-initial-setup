SUBDIRS = pages

uidir = $(datadir)/gnome-initial-setup

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DUIDIR="\"$(uidir)\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\" \
	-DDATADIR=\""$(datadir)"\"

libexec_PROGRAMS = gnome-initial-setup gnome-initial-setup-copy-worker

gnome_initial_setup_SOURCES =	\
	gnome-initial-setup.c gnome-initial-setup.h \
	gis-assistant.c gis-assistant.h gis-assistant-private.h \
	gis-assistant-gtk.c gis-assistant-gtk.h \
	gis-page.c gis-page.h \
	gis-driver.c gis-driver.h

if HAVE_CLUTTER
gnome_initial_setup_SOURCES += \
	gis-assistant-clutter.c gis-assistant-clutter.h	\
	cc-notebook.c cc-notebook.h
endif

gnome_initial_setup_LDADD =	\
	pages/welcome/libgiswelcome.la \
	pages/language/libgislanguage.la \
	pages/keyboard/libgiskeyboard.la \
	pages/eulas/libgiseulas.la \
	pages/network/libgisnetwork.la \
	pages/location/libgislocation.la \
	pages/account/libgisaccount.la \
	pages/goa/libgisgoa.la \
	pages/summary/libgissummary.la \
	$(INITIAL_SETUP_LIBS) \
	-lm

gnome_initial_setup_copy_worker_SOURCES =		\
	gnome-initial-setup-copy-worker.c

gnome_initial_setup_copy_worker_CFLAGS = \
	$(COPY_WORKER_CFLAGS)

gnome_initial_setup_copy_worker_LDADD = \
	$(COPY_WORKER_LIBS)

EXTRA_DIST = \
	$(UI_FILES) \
	setup.gresource.xml
