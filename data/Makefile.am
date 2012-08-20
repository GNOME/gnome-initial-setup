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

policydir = $(datadir)/gnome-initial-setup
policy_DATA =						\
	20-gnome-initial-setup.rules			\
	$(NULL)

polkitdir = $(datadir)/polkit-1/actions
polkit_DATA =						\
	org.gnome.initial-setup.policy			\
	$(NULL)

sessiondir = $(datadir)/gnome-session/sessions
session_DATA =						\
	gnome-initial-setup.session			\
	$(NULL)

desktopdir = $(pkgdatadir)/applications
desktop_DATA =						\
	gnome-initial-setup.desktop			\
	$(NULL)

EXTRA_DIST =						\
	20-gnome-initial-setup.rules			\
	org.gnome.initial-setup.policy			\
	gnome-initial-setup.desktop.in.in		\
	gnome-initial-setup.session			\
	$(NULL)

CLEANFILES =						\
	gnome-initial-setup.desktop.in			\
	gnome-initial-setup.desktop			\
	$(NULL)

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA =					\
	welcome-tour.desktop				\
	$(NULL)
