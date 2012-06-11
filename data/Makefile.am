NULL =

edit = $(AM_V_GEN) sed \
	-e 's|@SBINDIR[@]|$(sbindir)|g' \
	-e 's|@SYSCONFDIR[@]|$(sysconfdir)|g' \
	-e 's|@LIBDIR[@]|$(libdir)|g' \
	-e 's|@LIBEXECDIR[@]|$(libexecdir)|g' \
	-e 's|@LOCALSTATEDIR[@]|$(localstatedir)|g'

%.desktop.in: %.desktop.in.in Makefile
	$(edit) $< >$@

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(pkgdatadir)/applications
desktop_DATA =						\
	gnome-initial-setup.desktop			\
	$(NULL)

EXTRA_DIST =						\
	gnome-initial-setup.desktop.in.in		\
	$(NULL)

CLEANFILES =						\
	gnome-initial-setup.desktop.in			\
	$(NULL)
