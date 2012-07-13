
if HAVE_PWQUALITY
pw_utils_sources = pw-utils-pwquality.c pw-utils.h
else
pw_utils_sources = pw-utils.c pw-utils.h
endif

noinst_LTLIBRARIES = libgisaccount.la

libgisaccount_la_SOURCES =	\
	$(pw_utils_sources) \
	um-utils.c um-utils.h \
	um-crop-area.c um-crop-area.h \
	um-photo-dialog.c um-photo-dialog.h \
	gis-account-page.c gis-account-page.h

libgisaccount_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "../.."
libgisaccount_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisaccount_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
