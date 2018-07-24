SUBDIRS = pages

BUILT_SOURCES =

uidir = $(datadir)/gnome-initial-setup

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DSECRET_API_SUBJECT_TO_CHANGE \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DVENDOR_CONF_FILE="\"$(VENDOR_CONF_FILE)\"" \
	-DUIDIR="\"$(uidir)\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\" \
	-DDATADIR=\""$(datadir)"\"

libexec_PROGRAMS = gnome-initial-setup gnome-initial-setup-copy-worker

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/gis-assistant.gresource.xml)
gis-assistant-resources.c: gis-assistant.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
gis-assistant-resources.h: gis-assistant.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += gis-assistant-resources.c gis-assistant-resources.h

gnome_initial_setup_SOURCES = \
	$(BUILT_SOURCES) \
	gnome-initial-setup.c gnome-initial-setup.h \
	gis-assistant.c gis-assistant.h \
	gis-page.c gis-page.h \
	gis-driver.c gis-driver.h \
	gis-keyring.c gis-keyring.h

gnome_initial_setup_LDADD =	\
	pages/language/libgislanguage.la \
	pages/region/libgisregion.la \
	pages/keyboard/libgiskeyboard.la \
	pages/eulas/libgiseulas.la \
	pages/network/libgisnetwork.la \
	pages/timezone/libgistimezone.la \
	pages/privacy/libgisprivacy.la \
	pages/goa/libgisgoa.la \
	pages/account/libgisaccount.la \
	pages/password/libgispassword.la \
	pages/software/libgissoftware.la \
	pages/summary/libgissummary.la \
	$(INITIAL_SETUP_LIBS) \
	-lm

gnome_initial_setup_copy_worker_SOURCES =		\
	gnome-initial-setup-copy-worker.c

gnome_initial_setup_copy_worker_CFLAGS = \
	$(COPY_WORKER_CFLAGS)

gnome_initial_setup_copy_worker_LDADD = \
	$(COPY_WORKER_LIBS)

EXTRA_DIST =				\
	gis-assistant.gresource.xml	\
	meson.build			\
	$(resource_files)		\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
