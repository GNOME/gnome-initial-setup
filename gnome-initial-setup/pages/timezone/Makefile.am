
noinst_LTLIBRARIES = libgistimezone.la

BUILT_SOURCES =

# Used for backward file
AM_CPPFLAGS = \
	-DGNOMECC_DATA_DIR="\"$(datadir)/gnome-control-center\""

timedated.c: timedated.h
timedated.h: Makefile.am timedated1-interface.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.freedesktop. \
		--generate-c-code timedated \
		$(srcdir)/timedated1-interface.xml
BUILT_SOURCES += timedated.c timedated.h

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/datetime.gresource.xml)
cc-datetime-resources.c: datetime.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
cc-datetime-resources.h: datetime.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += cc-datetime-resources.c cc-datetime-resources.h

resource_files_timezone = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/timezone.gresource.xml)
timezone-resources.c: timezone.gresource.xml $(resource_files_timezone)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
timezone-resources.h: timezone.gresource.xml $(resource_files_timezone)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += timezone-resources.c timezone-resources.h

libgistimezone_la_SOURCES =	\
	cc-timezone-map.c cc-timezone-map.h \
	tz.c tz.h \
	gis-bubble-widget.c gis-bubble-widget.h \
	gis-timezone-page.c gis-timezone-page.h \
	$(BUILT_SOURCES)

libgistimezone_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgistimezone_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgistimezone_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =				\
	timedated1-interface.xml	\
	$(resource_files)		\
	$(resource_files_timezone)	\
	datetime.gresource.xml		\
	meson.build			\
	timezone.gresource.xml		\
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
